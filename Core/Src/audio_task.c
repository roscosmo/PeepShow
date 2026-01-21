#include "audio_task.h"

#include "app_freertos.h"
#include "cmsis_os2.h"
#include "main.h"
#include "sound_manager.h"
#include "storage_task.h"
#include "power_task.h"


extern SAI_HandleTypeDef hsai_BlockA1;

typedef enum
{
  AUDIO_STATE_IDLE = 0,
  AUDIO_STATE_PLAYING = 1
} audio_state_t;

typedef enum
{
  WAV_FORMAT_PCM = 1U,
  WAV_FORMAT_IMA_ADPCM = 0x11U
} wav_format_t;

typedef struct
{
  const uint8_t *data;
  uint32_t data_bytes;
  uint32_t sample_rate;
  wav_format_t format;
  uint32_t total_frames;
  uint16_t channels;
  uint16_t bits_per_sample;
  uint16_t block_align;
  uint16_t samples_per_block;
} wav_info_t;

typedef struct
{
  uint32_t data_offset;
  uint32_t block_end;
  uint32_t byte_offset;
  uint16_t samples_left;
  int16_t predictor;
  uint8_t index;
  uint8_t nibble_high;
} adpcm_state_t;

typedef struct
{
  uint16_t samples_left;
  uint16_t block_bytes_left;
  int16_t predictor;
  uint8_t index;
  uint8_t nibble_high;
  uint8_t cur_byte;
} adpcm_stream_state_t;

typedef struct
{
  uint8_t active;
  sound_id_t id;
  sound_prio_t prio;
  sound_flags_t flags;
  sound_category_t category;
  uint8_t gain_q8;
  wav_info_t wav;
  adpcm_state_t adpcm;
} audio_voice_t;

static const uint32_t kAudioFlagHalf = (1UL << 0U);
static const uint32_t kAudioFlagFull = (1UL << 1U);
static const uint32_t kAudioFlagError = (1UL << 2U);

#define AUDIO_MAX_SFX_VOICES 5U

static const uint32_t kAudioSampleRate = 16000U;
static const uint8_t kAudioVolumeMax = 20U;
static const uint8_t kAudioVolumeDefault = 7U;
static const uint32_t kAudioStreamPrebufferMin = 512U;
static const uint32_t kAudioStreamPrebufferMax = 2048U;
static const uint32_t kAudioStreamRetryMaxTries = 100U;

static const int16_t kImaStepTable[89] =
{
  7, 8, 9, 10, 11, 12, 13, 14,
  16, 17, 19, 21, 23, 25, 28, 31,
  34, 37, 41, 45, 50, 55, 60, 66,
  73, 80, 88, 97, 107, 118, 130, 143,
  157, 173, 190, 209, 230, 253, 279, 307,
  337, 371, 408, 449, 494, 544, 598, 658,
  724, 796, 876, 963, 1060, 1166, 1282, 1411,
  1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024,
  3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484,
  7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
  15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
  32767
};

static const int8_t kImaIndexTable[16] =
{
  -1, -1, -1, -1, 2, 4, 6, 8,
  -1, -1, -1, -1, 2, 4, 6, 8
};

static int16_t s_audio_buf[2048];
static audio_state_t s_audio_state = AUDIO_STATE_IDLE;
static audio_voice_t s_sfx_voices[AUDIO_MAX_SFX_VOICES];
static wav_info_t s_stream_wav;
static adpcm_stream_state_t s_stream_adpcm;
static uint32_t s_stream_bytes_left = 0U;
static uint32_t s_stream_prebuffer = 0U;
static sound_id_t s_stream_id = SND_COUNT;
static sound_flags_t s_stream_flags = 0U;
static uint8_t s_stream_gain_q8 = 0U;
static uint8_t s_stream_wait = 0U;
static uint8_t s_stream_done = 0U;
static uint8_t s_stream_active = 0U;
static uint8_t s_stream_retry = 0U;
static sound_id_t s_stream_retry_id = SND_COUNT;
static sound_flags_t s_stream_retry_flags = 0U;
static uint32_t s_stream_retry_tries = 0U;
static volatile uint8_t s_audio_volume = kAudioVolumeDefault;
static uint8_t s_category_volume[SOUND_CAT_COUNT] = {5U, 5U, 5U};
static uint8_t s_audio_power_ref = 0U;
static uint8_t s_audio_dma_circular = 0U;
static DMA_QListTypeDef s_audio_dma_queue;
static DMA_NodeTypeDef s_audio_dma_node;

static uint8_t audio_volume_to_q8(uint8_t level)
{
  if (level >= kAudioVolumeMax)
  {
    return 255U;
  }
  return (uint8_t)(((uint32_t)level * 255U) / kAudioVolumeMax);
}

static uint8_t audio_scale_gain_q8(uint8_t gain_q8, uint8_t scale_q8)
{
  return (uint8_t)(((uint32_t)gain_q8 * scale_q8) / 255U);
}

static uint8_t audio_category_gain_q8(sound_category_t category)
{
  if ((uint32_t)category >= (uint32_t)SOUND_CAT_COUNT)
  {
    return 255U;
  }
  return audio_volume_to_q8(s_category_volume[category]);
}

static uint16_t audio_read_u16_le(const uint8_t *data)
{
  return (uint16_t)data[0] | (uint16_t)((uint16_t)data[1] << 8);
}

static uint32_t audio_read_u32_le(const uint8_t *data)
{
  return (uint32_t)data[0]
         | ((uint32_t)data[1] << 8)
         | ((uint32_t)data[2] << 16)
         | ((uint32_t)data[3] << 24);
}

static uint8_t audio_match_fourcc(const uint8_t *data, char a, char b, char c, char d)
{
  return (data[0] == (uint8_t)a)
         && (data[1] == (uint8_t)b)
         && (data[2] == (uint8_t)c)
         && (data[3] == (uint8_t)d);
}

static uint8_t audio_parse_wav(const uint8_t *data, uint32_t len, wav_info_t *out)
{
  if ((data == NULL) || (out == NULL) || (len < 44U))
  {
    return 0U;
  }

  if ((!audio_match_fourcc(data, 'R', 'I', 'F', 'F')) ||
      (!audio_match_fourcc(&data[8], 'W', 'A', 'V', 'E')))
  {
    return 0U;
  }

  uint8_t found_fmt = 0U;
  uint8_t found_data = 0U;
  uint16_t fmt_audio = 0U;
  uint16_t fmt_channels = 0U;
  uint32_t fmt_rate = 0U;
  uint16_t fmt_align = 0U;
  uint16_t fmt_bits = 0U;
  uint16_t fmt_cb_size = 0U;
  uint16_t fmt_samples_per_block = 0U;
  const uint8_t *data_ptr = NULL;
  uint32_t data_bytes = 0U;
  uint32_t offset = 12U;

  while ((offset + 8U) <= len)
  {
    uint32_t chunk_size = audio_read_u32_le(&data[offset + 4U]);
    const uint8_t *chunk = &data[offset];
    offset += 8U;

    if ((offset + chunk_size) > len)
    {
      break;
    }

    if (audio_match_fourcc(chunk, 'f', 'm', 't', ' '))
    {
      if (chunk_size < 16U)
      {
        return 0U;
      }
      fmt_audio = audio_read_u16_le(&data[offset + 0U]);
      fmt_channels = audio_read_u16_le(&data[offset + 2U]);
      fmt_rate = audio_read_u32_le(&data[offset + 4U]);
      fmt_align = audio_read_u16_le(&data[offset + 12U]);
      fmt_bits = audio_read_u16_le(&data[offset + 14U]);
      if (chunk_size >= 18U)
      {
        fmt_cb_size = audio_read_u16_le(&data[offset + 16U]);
      }
      if ((fmt_audio == (uint16_t)WAV_FORMAT_IMA_ADPCM) && (chunk_size >= 20U))
      {
        if (fmt_cb_size >= 2U)
        {
          fmt_samples_per_block = audio_read_u16_le(&data[offset + 18U]);
        }
      }
      found_fmt = 1U;
    }
    else if (audio_match_fourcc(chunk, 'd', 'a', 't', 'a'))
    {
      data_ptr = &data[offset];
      data_bytes = chunk_size;
      found_data = 1U;
    }

    offset += chunk_size;
    if ((chunk_size & 1U) != 0U)
    {
      offset += 1U;
    }
  }

  if ((found_fmt == 0U) || (found_data == 0U))
  {
    return 0U;
  }

  if ((fmt_channels == 0U) || (fmt_channels > 2U))
  {
    return 0U;
  }

  if (fmt_align == 0U)
  {
    fmt_align = (uint16_t)((fmt_channels * fmt_bits) / 8U);
  }

  if (fmt_align == 0U)
  {
    return 0U;
  }

  if (data_ptr == NULL)
  {
    return 0U;
  }

  if (fmt_audio == (uint16_t)WAV_FORMAT_PCM)
  {
    if ((fmt_bits != 8U) && (fmt_bits != 16U))
    {
      return 0U;
    }

    uint32_t frames = data_bytes / (uint32_t)fmt_align;
    if (frames == 0U)
    {
      return 0U;
    }

    out->format = WAV_FORMAT_PCM;
    out->data = data_ptr;
    out->data_bytes = data_bytes;
    out->sample_rate = fmt_rate;
    out->total_frames = frames;
    out->channels = fmt_channels;
    out->bits_per_sample = fmt_bits;
    out->block_align = fmt_align;
    out->samples_per_block = 0U;
    return 1U;
  }

  if (fmt_audio == (uint16_t)WAV_FORMAT_IMA_ADPCM)
  {
    if (fmt_channels != 1U)
    {
      return 0U;
    }
    if (fmt_bits != 4U)
    {
      return 0U;
    }
    if (fmt_align <= 4U)
    {
      return 0U;
    }
    if (fmt_samples_per_block == 0U)
    {
      fmt_samples_per_block = (uint16_t)(((fmt_align - 4U) * 2U) + 1U);
    }
    if (fmt_samples_per_block == 0U)
    {
      return 0U;
    }

    uint32_t block_count = data_bytes / (uint32_t)fmt_align;
    uint32_t total_samples = block_count * (uint32_t)fmt_samples_per_block;
    if (total_samples == 0U)
    {
      return 0U;
    }

    out->format = WAV_FORMAT_IMA_ADPCM;
    out->data = data_ptr;
    out->data_bytes = data_bytes;
    out->sample_rate = fmt_rate;
    out->total_frames = total_samples;
    out->channels = fmt_channels;
    out->bits_per_sample = fmt_bits;
    out->block_align = fmt_align;
    out->samples_per_block = fmt_samples_per_block;
    return 1U;
  }

  return 0U;
}

static uint8_t audio_stream_prepare(const storage_stream_info_t *info)
{
  if (info == NULL)
  {
    return 0U;
  }

  if ((info->format != STORAGE_STREAM_FORMAT_IMA_ADPCM) ||
      (info->sample_rate != kAudioSampleRate) ||
      (info->channels != 1U))
  {
    return 0U;
  }

  if ((info->block_align == 0U) || (info->samples_per_block == 0U))
  {
    return 0U;
  }

  s_stream_wav.format = WAV_FORMAT_IMA_ADPCM;
  s_stream_wav.data = NULL;
  s_stream_wav.sample_rate = info->sample_rate;
  s_stream_wav.total_frames = 0U;
  s_stream_wav.channels = info->channels;
  s_stream_wav.bits_per_sample = 4U;
  s_stream_wav.block_align = info->block_align;
  s_stream_wav.samples_per_block = info->samples_per_block;
  s_stream_wav.data_bytes = info->data_bytes;
  s_stream_bytes_left = info->data_bytes;

  uint32_t target = (uint32_t)info->block_align * 2U;
  if (target < kAudioStreamPrebufferMin)
  {
    target = kAudioStreamPrebufferMin;
  }
  if (target > kAudioStreamPrebufferMax)
  {
    target = kAudioStreamPrebufferMax;
  }
  s_stream_prebuffer = target;
  return 1U;
}

static void audio_adpcm_reset(adpcm_state_t *state)
{
  if (state == NULL)
  {
    return;
  }

  state->data_offset = 0U;
  state->block_end = 0U;
  state->byte_offset = 0U;
  state->samples_left = 0U;
  state->predictor = 0;
  state->index = 0U;
  state->nibble_high = 0U;
}

static uint8_t audio_adpcm_begin_block(const wav_info_t *wav, adpcm_state_t *state)
{
  if ((wav == NULL) || (state == NULL))
  {
    return 0U;
  }

  if ((state->data_offset + wav->block_align) > wav->data_bytes)
  {
    return 0U;
  }

  const uint8_t *block = &wav->data[state->data_offset];
  state->predictor = (int16_t)audio_read_u16_le(block);
  state->index = block[2];
  if (state->index > 88U)
  {
    state->index = 88U;
  }
  state->byte_offset = state->data_offset + 4U;
  state->block_end = state->data_offset + wav->block_align;
  state->samples_left = wav->samples_per_block;
  state->nibble_high = 0U;
  state->data_offset = state->block_end;
  return 1U;
}

static uint8_t audio_adpcm_next_sample(const wav_info_t *wav, adpcm_state_t *state, int16_t *out)
{
  if ((wav == NULL) || (state == NULL) || (out == NULL))
  {
    return 0U;
  }

  if (state->samples_left == 0U)
  {
    if (audio_adpcm_begin_block(wav, state) == 0U)
    {
      return 0U;
    }
  }

  if (state->samples_left == wav->samples_per_block)
  {
    state->samples_left--;
    *out = state->predictor;
    return 1U;
  }

  if (state->byte_offset >= state->block_end)
  {
    state->samples_left = 0U;
    return 0U;
  }

  uint8_t code;
  if (state->nibble_high == 0U)
  {
    code = wav->data[state->byte_offset] & 0x0FU;
    state->nibble_high = 1U;
  }
  else
  {
    code = (wav->data[state->byte_offset] >> 4) & 0x0FU;
    state->nibble_high = 0U;
    state->byte_offset++;
  }

  int32_t predictor = state->predictor;
  int32_t step = kImaStepTable[state->index];
  int32_t diff = step >> 3;
  if ((code & 1U) != 0U)
  {
    diff += step >> 2;
  }
  if ((code & 2U) != 0U)
  {
    diff += step >> 1;
  }
  if ((code & 4U) != 0U)
  {
    diff += step;
  }
  if ((code & 8U) != 0U)
  {
    predictor -= diff;
  }
  else
  {
    predictor += diff;
  }

  if (predictor > 32767)
  {
    predictor = 32767;
  }
  else if (predictor < -32768)
  {
    predictor = -32768;
  }

  state->predictor = (int16_t)predictor;

  int32_t index = (int32_t)state->index + (int32_t)kImaIndexTable[code];
  if (index < 0)
  {
    index = 0;
  }
  else if (index > 88)
  {
    index = 88;
  }
  state->index = (uint8_t)index;

  state->samples_left--;
  *out = state->predictor;
  return 1U;
}

static void audio_adpcm_stream_reset(adpcm_stream_state_t *state)
{
  if (state == NULL)
  {
    return;
  }

  state->samples_left = 0U;
  state->block_bytes_left = 0U;
  state->predictor = 0;
  state->index = 0U;
  state->nibble_high = 0U;
  state->cur_byte = 0U;
}

static uint8_t audio_adpcm_stream_next_sample(adpcm_stream_state_t *state, int16_t *out, uint8_t *done)
{
  if ((state == NULL) || (out == NULL))
  {
    return 0U;
  }

  if (done != NULL)
  {
    *done = 0U;
  }

  if (state->samples_left == 0U)
  {
    if (s_stream_bytes_left < s_stream_wav.block_align)
    {
      if (done != NULL)
      {
        *done = 1U;
      }
      return 0U;
    }

    if (storage_stream_available() < 4U)
    {
      return 0U;
    }

    uint8_t header[4];
    if (storage_stream_read(header, (uint32_t)sizeof(header)) != (uint32_t)sizeof(header))
    {
      return 0U;
    }

    state->predictor = (int16_t)((uint16_t)header[0] | ((uint16_t)header[1] << 8));
    state->index = header[2];
    if (state->index > 88U)
    {
      state->index = 88U;
    }
    state->samples_left = s_stream_wav.samples_per_block;
    state->block_bytes_left = (uint16_t)(s_stream_wav.block_align - 4U);
    state->nibble_high = 0U;
    state->cur_byte = 0U;
    s_stream_bytes_left -= s_stream_wav.block_align;

    state->samples_left--;
    *out = state->predictor;
    return 1U;
  }

  if (state->block_bytes_left == 0U)
  {
    state->samples_left = 0U;
    return 0U;
  }

  uint8_t code;
  if (state->nibble_high == 0U)
  {
    if (storage_stream_available() < 1U)
    {
      return 0U;
    }
    if (storage_stream_read(&state->cur_byte, 1U) != 1U)
    {
      return 0U;
    }
    state->block_bytes_left--;
    code = state->cur_byte & 0x0FU;
    state->nibble_high = 1U;
  }
  else
  {
    code = (state->cur_byte >> 4) & 0x0FU;
    state->nibble_high = 0U;
  }

  int32_t predictor = state->predictor;
  int32_t step = kImaStepTable[state->index];
  int32_t diff = step >> 3;
  if ((code & 1U) != 0U)
  {
    diff += step >> 2;
  }
  if ((code & 2U) != 0U)
  {
    diff += step >> 1;
  }
  if ((code & 4U) != 0U)
  {
    diff += step;
  }
  if ((code & 8U) != 0U)
  {
    predictor -= diff;
  }
  else
  {
    predictor += diff;
  }

  if (predictor > 32767)
  {
    predictor = 32767;
  }
  else if (predictor < -32768)
  {
    predictor = -32768;
  }

  state->predictor = (int16_t)predictor;

  int32_t index = (int32_t)state->index + (int32_t)kImaIndexTable[code];
  if (index < 0)
  {
    index = 0;
  }
  else if (index > 88)
  {
    index = 88;
  }
  state->index = (uint8_t)index;

  state->samples_left--;
  *out = state->predictor;
  return 1U;
}

static int16_t audio_wav_sample(const wav_info_t *wav, uint32_t frame)
{
  uint32_t offset = frame * (uint32_t)wav->block_align;
  const uint8_t *p = &wav->data[offset];

  if (wav->bits_per_sample == 8U)
  {
    if (wav->channels == 1U)
    {
      int16_t s = (int16_t)p[0] - 128;
      return (int16_t)((int32_t)s * 256);
    }
    int16_t l = (int16_t)p[0] - 128;
    int16_t r = (int16_t)p[1] - 128;
    int32_t avg = ((int32_t)l + (int32_t)r) / 2;
    return (int16_t)(avg * 256);
  }

  if (wav->channels == 1U)
  {
    return (int16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
  }

  int16_t l = (int16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
  int16_t r = (int16_t)((uint16_t)p[2] | ((uint16_t)p[3] << 8));
  return (int16_t)(((int32_t)l + (int32_t)r) / 2);
}

static int16_t audio_apply_volume(int32_t sample)
{
  int32_t level = (int32_t)s_audio_volume;
  int32_t scaled = (sample * (level * 5)) / (int32_t)kAudioVolumeMax;

  if (scaled > 32767)
  {
    return 32767;
  }
  if (scaled < -32768)
  {
    return -32768;
  }

  return (int16_t)scaled;
}

static void audio_request_power_on(void)
{
  if (s_audio_power_ref != 0U)
  {
    return;
  }

  if (qSysEventsHandle == NULL)
  {
    return;
  }

  app_sys_event_t sys_event = APP_SYS_EVENT_AUDIO_ON;
  if (osMessageQueuePut(qSysEventsHandle, &sys_event, 0U, osWaitForever) == osOK)
  {
    s_audio_power_ref = 1U;
  }
}

static void audio_request_power_off(void)
{
  if (s_audio_power_ref == 0U)
  {
    return;
  }

  if (qSysEventsHandle == NULL)
  {
    s_audio_power_ref = 0U;
    return;
  }

  app_sys_event_t sys_event = APP_SYS_EVENT_AUDIO_OFF;
  if (osMessageQueuePut(qSysEventsHandle, &sys_event, 0U, osWaitForever) == osOK)
  {
    s_audio_power_ref = 0U;
  }
}

static uint8_t audio_configure_dma_circular(void)
{
  if (hsai_BlockA1.hdmatx == NULL)
  {
    return 0U;
  }

  if (s_audio_dma_circular != 0U)
  {
    return 1U;
  }

  DMA_HandleTypeDef *hdma = hsai_BlockA1.hdmatx;
  DMA_NodeConfTypeDef node_conf = {0};

  (void)HAL_DMA_Abort(hdma);

  hdma->InitLinkedList.Priority = hdma->Init.Priority;
  hdma->InitLinkedList.LinkStepMode = DMA_LSM_FULL_EXECUTION;
  hdma->InitLinkedList.LinkAllocatedPort = DMA_LINK_ALLOCATED_PORT0;
  hdma->InitLinkedList.TransferEventMode = hdma->Init.TransferEventMode;
  hdma->InitLinkedList.LinkedListMode = DMA_LINKEDLIST_CIRCULAR;

  if (HAL_DMAEx_List_Init(hdma) != HAL_OK)
  {
    hdma->Init.Mode = DMA_NORMAL;
    (void)HAL_DMA_Init(hdma);
    return 0U;
  }

  s_audio_dma_queue.Type = QUEUE_TYPE_STATIC;
  if (HAL_DMAEx_List_ResetQ(&s_audio_dma_queue) != HAL_OK)
  {
    hdma->Init.Mode = DMA_NORMAL;
    (void)HAL_DMA_Init(hdma);
    return 0U;
  }

  node_conf.NodeType = DMA_GPDMA_LINEAR_NODE;
  node_conf.Init = hdma->Init;
  node_conf.DataHandlingConfig.DataExchange = DMA_EXCHANGE_NONE;
  node_conf.DataHandlingConfig.DataAlignment = DMA_DATA_RIGHTALIGN_ZEROPADDED;
  node_conf.TriggerConfig.TriggerMode = DMA_TRIGM_BLOCK_TRANSFER;
  node_conf.TriggerConfig.TriggerPolarity = DMA_TRIG_POLARITY_MASKED;
  node_conf.TriggerConfig.TriggerSelection = 0U;
  node_conf.RepeatBlockConfig.RepeatCount = 1U;
  node_conf.RepeatBlockConfig.SrcAddrOffset = 0;
  node_conf.RepeatBlockConfig.DestAddrOffset = 0;
  node_conf.RepeatBlockConfig.BlkSrcAddrOffset = 0;
  node_conf.RepeatBlockConfig.BlkDestAddrOffset = 0;
  node_conf.SrcAddress = (uint32_t)s_audio_buf;
  node_conf.DstAddress = (uint32_t)&hsai_BlockA1.Instance->DR;
  node_conf.DataSize = (uint32_t)sizeof(s_audio_buf);

  if (HAL_DMAEx_List_BuildNode(&node_conf, &s_audio_dma_node) != HAL_OK)
  {
    hdma->Init.Mode = DMA_NORMAL;
    (void)HAL_DMA_Init(hdma);
    return 0U;
  }

  if (HAL_DMAEx_List_InsertNode_Tail(&s_audio_dma_queue, &s_audio_dma_node) != HAL_OK)
  {
    hdma->Init.Mode = DMA_NORMAL;
    (void)HAL_DMA_Init(hdma);
    return 0U;
  }

  if (HAL_DMAEx_List_SetCircularMode(&s_audio_dma_queue) != HAL_OK)
  {
    hdma->Init.Mode = DMA_NORMAL;
    (void)HAL_DMA_Init(hdma);
    return 0U;
  }

  if (HAL_DMAEx_List_LinkQ(hdma, &s_audio_dma_queue) != HAL_OK)
  {
    hdma->Init.Mode = DMA_NORMAL;
    (void)HAL_DMA_Init(hdma);
    return 0U;
  }

  s_audio_dma_circular = 1U;
  return 1U;
}

static uint8_t audio_has_output(void)
{
  if (s_stream_active != 0U)
  {
    return 1U;
  }

  for (uint32_t i = 0U; i < AUDIO_MAX_SFX_VOICES; ++i)
  {
    if (s_sfx_voices[i].active != 0U)
    {
      return 1U;
    }
  }

  return 0U;
}

static uint8_t audio_has_pending(void)
{
  return ((audio_has_output() != 0U) || (s_stream_wait != 0U)) ? 1U : 0U;
}

static void audio_stop_all_sfx(void)
{
  for (uint32_t i = 0U; i < AUDIO_MAX_SFX_VOICES; ++i)
  {
    s_sfx_voices[i].active = 0U;
  }
}

static int32_t audio_scale_sample(int16_t sample, uint8_t gain_q8)
{
  return ((int32_t)sample * (int32_t)gain_q8) >> 8;
}

static uint8_t audio_voice_next_sample(audio_voice_t *voice, int16_t *out)
{
  if ((voice == NULL) || (out == NULL) || (voice->active == 0U))
  {
    return 0U;
  }

  if (audio_adpcm_next_sample(&voice->wav, &voice->adpcm, out) != 0U)
  {
    return 1U;
  }

  if ((voice->flags & SOUND_F_LOOP) != 0U)
  {
    audio_adpcm_reset(&voice->adpcm);
    if (audio_adpcm_next_sample(&voice->wav, &voice->adpcm, out) != 0U)
    {
      return 1U;
    }
  }

  voice->active = 0U;
  return 0U;
}

static void audio_mix_fill(int16_t *dst, uint32_t count)
{
  if ((dst == NULL) || (count == 0U))
  {
    return;
  }

  if (audio_has_output() == 0U)
  {
    for (uint32_t i = 0U; i < count; ++i)
    {
      dst[i] = 0;
    }
    return;
  }

  uint32_t frames = count / 2U;
  for (uint32_t i = 0U; i < frames; ++i)
  {
    int32_t mix = 0;

    if (s_stream_active != 0U)
    {
      int16_t pcm = 0;
      uint8_t done = 0U;
      if (audio_adpcm_stream_next_sample(&s_stream_adpcm, &pcm, &done) != 0U)
      {
        mix += audio_scale_sample(pcm, s_stream_gain_q8);
      }
      else if (done != 0U)
      {
        s_stream_done = 1U;
        s_stream_active = 0U;
      }
    }

    for (uint32_t v = 0U; v < AUDIO_MAX_SFX_VOICES; ++v)
    {
      if (s_sfx_voices[v].active == 0U)
      {
        continue;
      }

      int16_t pcm = 0;
      if (audio_voice_next_sample(&s_sfx_voices[v], &pcm) != 0U)
      {
        mix += audio_scale_sample(pcm, s_sfx_voices[v].gain_q8);
      }
    }

    int16_t out = audio_apply_volume(mix);
    dst[i * 2U] = out;
    dst[i * 2U + 1U] = out;
  }
}

static void audio_hw_start(void)
{
  if (s_audio_state == AUDIO_STATE_PLAYING)
  {
    return;
  }

  audio_mix_fill(s_audio_buf, (uint32_t)(sizeof(s_audio_buf) / sizeof(s_audio_buf[0])));

  (void)osThreadFlagsClear(kAudioFlagHalf | kAudioFlagFull | kAudioFlagError);
  (void)audio_configure_dma_circular();
  audio_request_power_on();
  HAL_GPIO_WritePin(SD_MODE_GPIO_Port, SD_MODE_Pin, GPIO_PIN_SET);

  if (HAL_SAI_Transmit_DMA(&hsai_BlockA1, (uint8_t *)s_audio_buf,
                           (uint16_t)(sizeof(s_audio_buf) / sizeof(s_audio_buf[0]))) == HAL_OK)
  {
    s_audio_state = AUDIO_STATE_PLAYING;
  }
  else
  {
    HAL_GPIO_WritePin(SD_MODE_GPIO_Port, SD_MODE_Pin, GPIO_PIN_RESET);
    audio_request_power_off();
  }
}

static void audio_hw_stop(void)
{
  if (s_audio_state == AUDIO_STATE_IDLE)
  {
    return;
  }

  (void)HAL_SAI_DMAStop(&hsai_BlockA1);
  HAL_GPIO_WritePin(SD_MODE_GPIO_Port, SD_MODE_Pin, GPIO_PIN_RESET);
  s_audio_state = AUDIO_STATE_IDLE;
  audio_request_power_off();
}

static void audio_update_hw_state(void)
{
  if (audio_has_output() != 0U)
  {
    if (s_audio_state == AUDIO_STATE_IDLE)
    {
      audio_hw_start();
    }
    return;
  }

  audio_hw_stop();
}

static void audio_stream_retry_clear(void)
{
  s_stream_retry = 0U;
  s_stream_retry_id = SND_COUNT;
  s_stream_retry_flags = 0U;
  s_stream_retry_tries = 0U;
}

static void audio_stream_retry_set(sound_id_t id, sound_flags_t flags)
{
  s_stream_retry = 1U;
  s_stream_retry_id = id;
  s_stream_retry_flags = flags;
  s_stream_retry_tries = 0U;
}

static uint8_t audio_stream_retry_try_open(void)
{
  if ((s_stream_retry == 0U) || (s_stream_active != 0U) || (s_stream_wait != 0U))
  {
    return 0U;
  }

  if (s_stream_retry_tries >= kAudioStreamRetryMaxTries)
  {
    audio_stream_retry_clear();
    return 0U;
  }

  const sound_registry_entry_t *entry = sound_registry_get(s_stream_retry_id);
  if ((entry == NULL) || (entry->path == NULL))
  {
    audio_stream_retry_clear();
    return 0U;
  }

  s_stream_retry_tries++;
  if (!storage_request_stream_open(entry->path))
  {
    return 0U;
  }

  s_stream_id = entry->id;
  s_stream_flags = s_stream_retry_flags;
  s_stream_gain_q8 = audio_scale_gain_q8(entry->default_gain_q8,
                                         audio_category_gain_q8(entry->category));
  s_stream_wait = 1U;
  s_stream_done = 0U;
  s_stream_bytes_left = 0U;
  s_stream_prebuffer = 0U;
  audio_adpcm_stream_reset(&s_stream_adpcm);
  audio_stream_retry_clear();
  return 1U;
}

static void audio_stream_stop(void)
{
  if ((s_stream_active == 0U) && (s_stream_wait == 0U))
  {
    audio_stream_retry_clear();
    return;
  }

  s_stream_active = 0U;
  s_stream_wait = 0U;
  s_stream_done = 0U;
  s_stream_bytes_left = 0U;
  s_stream_prebuffer = 0U;
  s_stream_id = SND_COUNT;
  s_stream_flags = 0U;
  s_stream_gain_q8 = 0U;
  audio_stream_retry_clear();
  audio_adpcm_stream_reset(&s_stream_adpcm);

  if (storage_stream_is_active() != 0U)
  {
    (void)storage_request_stream_close();
  }
}

static void audio_stream_start(const sound_registry_entry_t *entry, sound_flags_t flags)
{
  if ((entry == NULL) || (entry->path == NULL))
  {
    return;
  }

  if (s_stream_retry != 0U)
  {
    if (s_stream_retry_id == entry->id)
    {
      audio_stream_retry_clear();
      return;
    }
    audio_stream_retry_clear();
  }

  if ((s_stream_active != 0U) || (s_stream_wait != 0U))
  {
    if ((s_stream_id == entry->id) && ((flags & SOUND_F_OVERLAP) == 0U))
    {
      audio_stream_stop();
      return;
    }
    audio_stream_stop();
  }

  if (!storage_request_stream_open(entry->path))
  {
    audio_stream_retry_set(entry->id, flags);
    return;
  }

  s_stream_id = entry->id;
  s_stream_flags = flags;
  s_stream_gain_q8 = audio_scale_gain_q8(entry->default_gain_q8,
                                         audio_category_gain_q8(entry->category));
  s_stream_wait = 1U;
  s_stream_done = 0U;
  s_stream_bytes_left = 0U;
  s_stream_prebuffer = 0U;
  audio_adpcm_stream_reset(&s_stream_adpcm);
}

static uint8_t audio_stream_try_start(void)
{
  if (s_stream_wait == 0U)
  {
    return 0U;
  }

  if (storage_stream_has_error() != 0U)
  {
    audio_stream_stop();
    return 0U;
  }

  storage_stream_info_t info;
  if (!storage_stream_get_info(&info))
  {
    return 0U;
  }

  if (audio_stream_prepare(&info) == 0U)
  {
    audio_stream_stop();
    return 0U;
  }

  uint32_t prebuffer = (s_stream_prebuffer == 0U) ? kAudioStreamPrebufferMin : s_stream_prebuffer;
  if (storage_stream_available() < prebuffer)
  {
    return 0U;
  }

  s_stream_active = 1U;
  s_stream_wait = 0U;
  s_stream_done = 0U;
  audio_adpcm_stream_reset(&s_stream_adpcm);
  audio_update_hw_state();
  return 1U;
}

static void audio_stream_handle_done(void)
{
  if (s_stream_done == 0U)
  {
    return;
  }

  sound_flags_t flags = s_stream_flags;
  sound_id_t id = s_stream_id;
  s_stream_done = 0U;

  if ((flags & SOUND_F_LOOP) != 0U)
  {
    const sound_registry_entry_t *entry = sound_registry_get(id);
    audio_stream_stop();
    if (entry != NULL)
    {
      audio_stream_start(entry, flags);
    }
    return;
  }

  audio_stream_stop();
}

static void audio_stop_all(void)
{
  audio_stop_all_sfx();
  audio_stream_stop();
  audio_hw_stop();
}

static audio_voice_t *audio_find_voice_by_id(sound_id_t id)
{
  for (uint32_t i = 0U; i < AUDIO_MAX_SFX_VOICES; ++i)
  {
    if ((s_sfx_voices[i].active != 0U) && (s_sfx_voices[i].id == id))
    {
      return &s_sfx_voices[i];
    }
  }
  return NULL;
}

static audio_voice_t *audio_find_free_voice(void)
{
  for (uint32_t i = 0U; i < AUDIO_MAX_SFX_VOICES; ++i)
  {
    if (s_sfx_voices[i].active == 0U)
    {
      return &s_sfx_voices[i];
    }
  }
  return NULL;
}

static audio_voice_t *audio_find_lowest_prio_voice(void)
{
  audio_voice_t *victim = NULL;
  for (uint32_t i = 0U; i < AUDIO_MAX_SFX_VOICES; ++i)
  {
    if (s_sfx_voices[i].active == 0U)
    {
      continue;
    }
    if (victim == NULL)
    {
      victim = &s_sfx_voices[i];
      continue;
    }
    if ((uint8_t)s_sfx_voices[i].prio < (uint8_t)victim->prio)
    {
      victim = &s_sfx_voices[i];
    }
  }
  return victim;
}

static uint8_t audio_voice_start(audio_voice_t *voice, const sound_registry_entry_t *entry,
                                 sound_prio_t prio, sound_flags_t flags)
{
  if ((voice == NULL) || (entry == NULL))
  {
    return 0U;
  }

  const uint8_t *data = NULL;
  uint32_t data_len = 0U;

  if (entry->source == SOUND_SOURCE_LFS)
  {
    if (sound_cache_get(entry->id, &data, &data_len) == 0U)
    {
      return 0U;
    }
  }
  else if (entry->source == SOUND_SOURCE_EMBEDDED)
  {
    data = entry->embedded;
    data_len = entry->embedded_len;
  }
  else
  {
    return 0U;
  }

  wav_info_t wav;
  if (audio_parse_wav(data, data_len, &wav) == 0U)
  {
    return 0U;
  }

  if ((wav.format != WAV_FORMAT_IMA_ADPCM) ||
      (wav.sample_rate != kAudioSampleRate) ||
      (wav.channels != 1U))
  {
    return 0U;
  }

  voice->active = 1U;
  voice->id = entry->id;
  voice->prio = prio;
  voice->flags = flags;
  voice->category = entry->category;
  voice->gain_q8 = audio_scale_gain_q8(entry->default_gain_q8,
                                       audio_category_gain_q8(entry->category));
  voice->wav = wav;
  audio_adpcm_reset(&voice->adpcm);
  return 1U;
}

static void audio_handle_sfx_play(const sound_registry_entry_t *entry, sound_prio_t prio,
                                  sound_flags_t flags)
{
  if ((flags & SOUND_F_INTERRUPT) != 0U)
  {
    audio_stop_all_sfx();
  }

  if ((flags & SOUND_F_OVERLAP) == 0U)
  {
    audio_voice_t *existing = audio_find_voice_by_id(entry->id);
    if (existing != NULL)
    {
      (void)audio_voice_start(existing, entry, prio, flags);
      return;
    }
  }

  audio_voice_t *voice = audio_find_free_voice();
  if (voice == NULL)
  {
    audio_voice_t *victim = audio_find_lowest_prio_voice();
    if ((victim == NULL) || ((uint8_t)prio <= (uint8_t)victim->prio))
    {
      return;
    }
    voice = victim;
  }

  (void)audio_voice_start(voice, entry, prio, flags);
}

static void audio_handle_play(sound_id_t id, sound_prio_t prio, sound_flags_t flags)
{
  const sound_registry_entry_t *entry = sound_registry_get(id);
  if (entry == NULL)
  {
    return;
  }

  sound_flags_t effective_flags = (sound_flags_t)(entry->flags | flags);

  if ((power_task_is_sleepface_active() != 0U) &&
      ((effective_flags & SOUND_F_ALLOW_SLEEPFACE) == 0U))
  {
    return;
  }

  if ((entry->source == SOUND_SOURCE_LFS) && ((effective_flags & SOUND_F_STREAM) != 0U))
  {
    audio_stream_start(entry, effective_flags);
    return;
  }

  audio_handle_sfx_play(entry, prio, effective_flags);
}

static void audio_handle_stop(sound_id_t id)
{
  if ((s_stream_active != 0U) || (s_stream_wait != 0U))
  {
    if (s_stream_id == id)
    {
      audio_stream_stop();
    }
  }
  else if ((s_stream_retry != 0U) && (s_stream_retry_id == id))
  {
    audio_stream_retry_clear();
  }

  for (uint32_t i = 0U; i < AUDIO_MAX_SFX_VOICES; ++i)
  {
    if ((s_sfx_voices[i].active != 0U) && (s_sfx_voices[i].id == id))
    {
      s_sfx_voices[i].active = 0U;
    }
  }
}

void audio_task_run(void)
{
  app_audio_cmd_t cmd = 0U;

  for (;;)
  {
    if (power_task_is_quiescing() != 0U)
    {
      if (audio_has_pending() == 0U)
      {
        audio_hw_stop();
        audio_stream_stop();
        power_task_quiesce_ack(POWER_QUIESCE_ACK_AUDIO);
        while (osMessageQueueGet(qAudioCmdHandle, &cmd, NULL, 0U) == osOK)
        {
        }
        osDelay(5U);
        continue;
      }
      power_task_quiesce_clear(POWER_QUIESCE_ACK_AUDIO);
    }
    else
    {
      power_task_quiesce_clear(POWER_QUIESCE_ACK_AUDIO);
    }

    if ((s_stream_active == 0U) && (s_stream_wait == 0U) && (storage_stream_is_active() != 0U))
    {
      (void)storage_request_stream_close();
    }

    if (s_audio_state == AUDIO_STATE_PLAYING)
    {
      int32_t flags = (int32_t)osThreadFlagsWait(kAudioFlagHalf | kAudioFlagFull | kAudioFlagError,
                                                 osFlagsWaitAny, 20U);
      if (flags >= 0)
      {
        uint32_t half_count = (uint32_t)(sizeof(s_audio_buf) / sizeof(s_audio_buf[0]) / 2U);
        if (((uint32_t)flags & kAudioFlagHalf) != 0U)
        {
          audio_mix_fill(&s_audio_buf[0], half_count);
        }
        if (((uint32_t)flags & kAudioFlagFull) != 0U)
        {
          audio_mix_fill(&s_audio_buf[half_count], half_count);
          if (s_audio_dma_circular == 0U)
          {
            (void)HAL_SAI_Transmit_DMA(&hsai_BlockA1, (uint8_t *)s_audio_buf,
                                       (uint16_t)(sizeof(s_audio_buf) / sizeof(s_audio_buf[0])));
          }
        }
        if (((uint32_t)flags & kAudioFlagError) != 0U)
        {
          audio_stop_all();
        }
      }
      else if ((s_audio_dma_circular == 0U) && (hsai_BlockA1.State == HAL_SAI_STATE_READY))
      {
        (void)HAL_SAI_Transmit_DMA(&hsai_BlockA1, (uint8_t *)s_audio_buf,
                                   (uint16_t)(sizeof(s_audio_buf) / sizeof(s_audio_buf[0])));
      }

      if ((s_stream_active != 0U) && (storage_stream_has_error() != 0U))
      {
        audio_stream_stop();
      }

      audio_stream_handle_done();
      audio_update_hw_state();

      if (osMessageQueueGet(qAudioCmdHandle, &cmd, NULL, 0U) != osOK)
      {
        (void)audio_stream_retry_try_open();
        continue;
      }
    }
    else
    {
      if (audio_stream_try_start() != 0U)
      {
        continue;
      }

      uint32_t timeout = ((s_stream_wait != 0U) || (s_stream_retry != 0U)) ? 20U : osWaitForever;
      if (osMessageQueueGet(qAudioCmdHandle, &cmd, NULL, timeout) != osOK)
      {
        if (s_stream_wait != 0U)
        {
          (void)audio_stream_try_start();
        }
        (void)audio_stream_retry_try_open();
        continue;
      }
    }

    if ((cmd & SOUND_CMD_FLAG) != 0U)
    {
      if (SOUND_CMD_IS(cmd, SOUND_CMD_TYPE_PLAY))
      {
        audio_handle_play(SOUND_CMD_GET_ID(cmd), SOUND_CMD_GET_PRIO(cmd), SOUND_CMD_GET_FLAGS(cmd));
      }
      else if (SOUND_CMD_IS(cmd, SOUND_CMD_TYPE_STOP))
      {
        audio_handle_stop(SOUND_CMD_GET_ID(cmd));
      }
      else if (SOUND_CMD_IS(cmd, SOUND_CMD_TYPE_STOP_ALL))
      {
        audio_stop_all();
      }
    }
    else
    {
      switch (cmd)
      {
        case APP_AUDIO_CMD_STOP:
          audio_stop_all();
          break;
        case APP_AUDIO_CMD_KEYCLICK:
          audio_handle_play(SND_UI_MOVE, SOUND_PRIO_UI, 0U);
          break;
        case APP_AUDIO_CMD_MUSIC_TOGGLE:
        case APP_AUDIO_CMD_FLASH_TOGGLE:
          audio_handle_play(SND_MUSIC_MEGAMAN, SOUND_PRIO_MUSIC, 0U);
          break;
        default:
          break;
      }
    }

    (void)audio_stream_retry_try_open();

    audio_update_hw_state();
  }
}

void HAL_SAI_TxHalfCpltCallback(SAI_HandleTypeDef *hsai)
{
  if ((hsai == &hsai_BlockA1) && (tskAudioHandle != NULL))
  {
    (void)osThreadFlagsSet(tskAudioHandle, kAudioFlagHalf);
  }
}

void HAL_SAI_TxCpltCallback(SAI_HandleTypeDef *hsai)
{
  if ((hsai == &hsai_BlockA1) && (tskAudioHandle != NULL))
  {
    (void)osThreadFlagsSet(tskAudioHandle, kAudioFlagFull);
  }
}

void HAL_SAI_ErrorCallback(SAI_HandleTypeDef *hsai)
{
  if ((hsai == &hsai_BlockA1) && (tskAudioHandle != NULL))
  {
    (void)osThreadFlagsSet(tskAudioHandle, kAudioFlagError);
  }
}

void audio_set_volume(uint8_t level)
{
  if (level > kAudioVolumeMax)
  {
    level = kAudioVolumeMax;
  }

  s_audio_volume = level;
}

uint8_t audio_get_volume(void)
{
  return s_audio_volume;
}

void audio_set_category_volume(sound_category_t category, uint8_t level)
{
  if ((uint32_t)category >= (uint32_t)SOUND_CAT_COUNT)
  {
    return;
  }
  if (level > kAudioVolumeMax)
  {
    level = kAudioVolumeMax;
  }

  s_category_volume[category] = level;

  uint8_t cat_gain = audio_category_gain_q8(category);
  for (uint32_t i = 0U; i < AUDIO_MAX_SFX_VOICES; ++i)
  {
    if ((s_sfx_voices[i].active != 0U) && (s_sfx_voices[i].category == category))
    {
      const sound_registry_entry_t *entry = sound_registry_get(s_sfx_voices[i].id);
      if (entry != NULL)
      {
        s_sfx_voices[i].gain_q8 = audio_scale_gain_q8(entry->default_gain_q8, cat_gain);
      }
    }
  }

  if (s_stream_id != SND_COUNT)
  {
    const sound_registry_entry_t *entry = sound_registry_get(s_stream_id);
    if ((entry != NULL) && (entry->category == category))
    {
      s_stream_gain_q8 = audio_scale_gain_q8(entry->default_gain_q8, cat_gain);
    }
  }
}

uint8_t audio_get_category_volume(sound_category_t category)
{
  if ((uint32_t)category >= (uint32_t)SOUND_CAT_COUNT)
  {
    return 0U;
  }
  return s_category_volume[category];
}

uint8_t audio_is_active(void)
{
  return audio_has_pending();
}
