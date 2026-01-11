#include "audio_task.h"

#include "app_freertos.h"
#include "cmsis_os2.h"
#include "main.h"
#include "sounds.h"

#include <math.h>
#include <stdbool.h>

extern SAI_HandleTypeDef hsai_BlockA1;

typedef enum
{
  AUDIO_STATE_IDLE = 0,
  AUDIO_STATE_PLAYING = 1
} audio_state_t;

typedef enum
{
  AUDIO_MODE_NONE = 0,
  AUDIO_MODE_TONE,
  AUDIO_MODE_CLICK_TONE,
  AUDIO_MODE_CLICK_WAV
} audio_mode_t;

typedef struct
{
  const uint8_t *data;
  uint32_t data_bytes;
  uint32_t sample_rate;
  uint32_t total_frames;
  uint16_t channels;
  uint16_t bits_per_sample;
  uint16_t block_align;
} wav_info_t;

static const uint32_t kAudioFlagHalf = (1UL << 0U);
static const uint32_t kAudioFlagFull = (1UL << 1U);
static const uint32_t kAudioFlagError = (1UL << 2U);

static const uint32_t kAudioSampleRate = 16000U;
static const float kAudioToneHz = 440.0f;
static const float kAudioClickHz = 2000.0f;
static const uint32_t kAudioClickMs = 25U;

static int16_t s_audio_buf[2048];
static uint32_t s_phase_q16 = 0U;
static uint32_t s_phase_step_q16 = 0U;
static audio_state_t s_audio_state = AUDIO_STATE_IDLE;
static audio_mode_t s_audio_mode = AUDIO_MODE_NONE;
static uint32_t s_click_stop_ms = 0U;
static uint8_t s_click_active = 0U;
static uint8_t s_click_done = 0U;
static wav_info_t s_click_wav;
static uint8_t s_wav_state = 0U;
static uint32_t s_wav_pos_q16 = 0U;
static uint32_t s_wav_step_q16 = 0U;

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

  if (fmt_audio != 1U)
  {
    return 0U;
  }

  if ((fmt_bits != 8U) && (fmt_bits != 16U))
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

  uint32_t frames = data_bytes / (uint32_t)fmt_align;
  if ((frames == 0U) || (data_ptr == NULL))
  {
    return 0U;
  }

  out->data = data_ptr;
  out->data_bytes = data_bytes;
  out->sample_rate = fmt_rate;
  out->total_frames = frames;
  out->channels = fmt_channels;
  out->bits_per_sample = fmt_bits;
  out->block_align = fmt_align;
  return 1U;
}

static uint8_t audio_wav_prepare(void)
{
  if (s_wav_state == 1U)
  {
    return 1U;
  }
  if (s_wav_state == 2U)
  {
    return 0U;
  }

  if (audio_parse_wav((const uint8_t *)menuBeep, (uint32_t)sizeof(menuBeep), &s_click_wav) == 0U)
  {
    s_wav_state = 2U;
    return 0U;
  }

  if (s_click_wav.sample_rate == 0U)
  {
    s_wav_state = 2U;
    return 0U;
  }

  s_wav_state = 1U;
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

static void audio_fill(int16_t *dst, uint32_t count)
{
  if ((dst == NULL) || (count == 0U))
  {
    return;
  }

  if (s_audio_mode == AUDIO_MODE_CLICK_WAV)
  {
    if (s_wav_state != 1U)
    {
      for (uint32_t i = 0U; i < count; ++i)
      {
        dst[i] = 0;
      }
      s_click_done = 1U;
      return;
    }

    uint32_t frames = count / 2U;
    uint32_t i = 0U;
    for (; i < frames; ++i)
    {
      uint32_t frame = s_wav_pos_q16 >> 16;
      if (frame >= s_click_wav.total_frames)
      {
        s_click_done = 1U;
        break;
      }

      int16_t pcm = audio_wav_sample(&s_click_wav, frame);
      dst[i * 2U] = pcm;
      dst[i * 2U + 1U] = pcm;
      s_wav_pos_q16 += s_wav_step_q16;
    }

    for (; i < frames; ++i)
    {
      dst[i * 2U] = 0;
      dst[i * 2U + 1U] = 0;
    }
    return;
  }

  if ((s_audio_mode == AUDIO_MODE_TONE) || (s_audio_mode == AUDIO_MODE_CLICK_TONE))
  {
    const float two_pi = 6.28318530718f;
    uint32_t frames = count / 2U;
    for (uint32_t i = 0U; i < frames; ++i)
    {
      float phase = (float)(s_phase_q16 & 0xFFFFU) / 65536.0f;
      float sample = sinf(two_pi * phase);
      int16_t pcm = (int16_t)(sample * 20000.0f);
      dst[i * 2U] = pcm;
      dst[i * 2U + 1U] = pcm;
      s_phase_q16 += s_phase_step_q16;
    }
    return;
  }

  for (uint32_t i = 0U; i < count; ++i)
  {
    dst[i] = 0;
  }
}

static void audio_start(void)
{
  if (s_audio_state == AUDIO_STATE_PLAYING)
  {
    return;
  }

  s_phase_q16 = 0U;
  s_phase_step_q16 = (uint32_t)((kAudioToneHz * 65536.0f) / (float)kAudioSampleRate);
  s_click_active = 0U;
  s_click_done = 0U;
  s_click_stop_ms = 0U;
  s_audio_mode = AUDIO_MODE_TONE;
  audio_fill(s_audio_buf, (uint32_t)(sizeof(s_audio_buf) / sizeof(s_audio_buf[0])));

  (void)osThreadFlagsClear(kAudioFlagHalf | kAudioFlagFull | kAudioFlagError);
  HAL_GPIO_WritePin(SD_MODE_GPIO_Port, SD_MODE_Pin, GPIO_PIN_SET);

  if (HAL_SAI_Transmit_DMA(&hsai_BlockA1, (uint8_t *)s_audio_buf,
                           (uint16_t)(sizeof(s_audio_buf) / sizeof(s_audio_buf[0]))) == HAL_OK)
  {
    s_audio_state = AUDIO_STATE_PLAYING;
  }
  else
  {
    HAL_GPIO_WritePin(SD_MODE_GPIO_Port, SD_MODE_Pin, GPIO_PIN_RESET);
  }
}

static void audio_stop(void)
{
  if (s_audio_state == AUDIO_STATE_IDLE)
  {
    return;
  }

  (void)HAL_SAI_DMAStop(&hsai_BlockA1);
  HAL_GPIO_WritePin(SD_MODE_GPIO_Port, SD_MODE_Pin, GPIO_PIN_RESET);
  s_audio_state = AUDIO_STATE_IDLE;
  s_click_active = 0U;
  s_click_stop_ms = 0U;
  s_click_done = 0U;
  s_audio_mode = AUDIO_MODE_NONE;
}

static void audio_click_start(void)
{
  if (s_audio_state == AUDIO_STATE_PLAYING)
  {
    return;
  }

  s_click_active = 1U;
  s_click_done = 0U;

  if (audio_wav_prepare() != 0U)
  {
    s_audio_mode = AUDIO_MODE_CLICK_WAV;
    s_wav_pos_q16 = 0U;
    s_wav_step_q16 = (uint32_t)(((uint64_t)s_click_wav.sample_rate << 16) / kAudioSampleRate);
    s_click_stop_ms = 0U;
  }
  else
  {
    s_audio_mode = AUDIO_MODE_CLICK_TONE;
    s_phase_q16 = 0U;
    s_phase_step_q16 = (uint32_t)((kAudioClickHz * 65536.0f) / (float)kAudioSampleRate);
    s_click_stop_ms = osKernelGetTickCount() + kAudioClickMs;
  }

  audio_fill(s_audio_buf, (uint32_t)(sizeof(s_audio_buf) / sizeof(s_audio_buf[0])));

  (void)osThreadFlagsClear(kAudioFlagHalf | kAudioFlagFull | kAudioFlagError);
  HAL_GPIO_WritePin(SD_MODE_GPIO_Port, SD_MODE_Pin, GPIO_PIN_SET);

  if (HAL_SAI_Transmit_DMA(&hsai_BlockA1, (uint8_t *)s_audio_buf,
                           (uint16_t)(sizeof(s_audio_buf) / sizeof(s_audio_buf[0]))) == HAL_OK)
  {
    s_audio_state = AUDIO_STATE_PLAYING;
  }
  else
  {
    HAL_GPIO_WritePin(SD_MODE_GPIO_Port, SD_MODE_Pin, GPIO_PIN_RESET);
    s_click_active = 0U;
    s_click_done = 0U;
    s_audio_mode = AUDIO_MODE_NONE;
  }
}

void audio_task_run(void)
{
  app_audio_cmd_t cmd = 0U;

  for (;;)
  {
    if (s_audio_state == AUDIO_STATE_PLAYING)
    {
      if (s_click_active != 0U)
      {
        if (s_audio_mode == AUDIO_MODE_CLICK_WAV)
        {
          if (s_click_done != 0U)
          {
            audio_stop();
            continue;
          }
        }
        else
        {
          uint32_t now_ms = osKernelGetTickCount();
          if ((int32_t)(now_ms - s_click_stop_ms) >= 0)
          {
            audio_stop();
            continue;
          }
        }
      }

      int32_t flags = (int32_t)osThreadFlagsWait(kAudioFlagHalf | kAudioFlagFull | kAudioFlagError,
                                                 osFlagsWaitAny, 20U);
      if (flags >= 0)
      {
        if (((uint32_t)flags & kAudioFlagHalf) != 0U)
        {
          audio_fill(&s_audio_buf[0], (uint32_t)(sizeof(s_audio_buf) / sizeof(s_audio_buf[0]) / 2U));
        }
        if (((uint32_t)flags & kAudioFlagFull) != 0U)
        {
          audio_fill(&s_audio_buf[sizeof(s_audio_buf) / sizeof(s_audio_buf[0]) / 2U],
                     (uint32_t)(sizeof(s_audio_buf) / sizeof(s_audio_buf[0]) / 2U));
          (void)HAL_SAI_Transmit_DMA(&hsai_BlockA1, (uint8_t *)s_audio_buf,
                                     (uint16_t)(sizeof(s_audio_buf) / sizeof(s_audio_buf[0])));
        }
        if (((uint32_t)flags & kAudioFlagError) != 0U)
        {
          audio_stop();
        }
      }
      else if (hsai_BlockA1.State == HAL_SAI_STATE_READY)
      {
        (void)HAL_SAI_Transmit_DMA(&hsai_BlockA1, (uint8_t *)s_audio_buf,
                                   (uint16_t)(sizeof(s_audio_buf) / sizeof(s_audio_buf[0])));
      }
      if (osMessageQueueGet(qAudioCmdHandle, &cmd, NULL, 0U) != osOK)
      {
        continue;
      }
    }
    else
    {
      if (osMessageQueueGet(qAudioCmdHandle, &cmd, NULL, osWaitForever) != osOK)
      {
        continue;
      }
    }

    switch (cmd)
    {
      case APP_AUDIO_CMD_TOGGLE_TONE:
        if (s_audio_state == AUDIO_STATE_PLAYING)
        {
          audio_stop();
        }
        else
        {
          audio_start();
        }
        break;
      case APP_AUDIO_CMD_STOP:
        audio_stop();
        break;
      case APP_AUDIO_CMD_KEYCLICK:
        audio_click_start();
        break;
      default:
        break;
    }
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
