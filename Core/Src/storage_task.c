#include "storage_task.h"

#include "app_freertos.h"
#include "cmsis_os2.h"
#include "lfs.h"
#include "settings.h"
#include "main.h"

#include <string.h>

extern OSPI_HandleTypeDef hospi1;
extern const unsigned char musicWav[];

#define STORAGE_FLASH_BASE 0x00000000UL
#define STORAGE_FLASH_SIZE (16UL * 1024UL * 1024UL)
#define STORAGE_BLOCK_SIZE 4096U
#define STORAGE_READ_SIZE 16U
#define STORAGE_PROG_SIZE 256U
#define STORAGE_CACHE_SIZE STORAGE_PROG_SIZE
#define STORAGE_LOOKAHEAD_SIZE 64U
#define STORAGE_BLOCK_COUNT (STORAGE_FLASH_SIZE / STORAGE_BLOCK_SIZE)
#define STORAGE_FLASH_PAGE_SIZE 256U
#define STORAGE_TIMEOUT_MS 5000U
#define STORAGE_STREAM_BUF_SIZE 4096U
#define STORAGE_STREAM_BUF_MASK (STORAGE_STREAM_BUF_SIZE - 1U)
#define STORAGE_STREAM_FILL_MAX_LOOPS 16U

#define FLASH_CMD_READ_DATA 0x03U
#define FLASH_CMD_WRITE_ENABLE 0x06U
#define FLASH_CMD_PAGE_PROGRAM 0x02U
#define FLASH_CMD_READ_STATUS 0x05U
#define FLASH_CMD_SECTOR_ERASE 0x20U
#define FLASH_CMD_DPD_ENTER 0xB9U
#define FLASH_CMD_DPD_RELEASE 0xABU

#define WAV_FORMAT_IMA_ADPCM 0x11U

typedef struct
{
  uint32_t base;
  uint32_t size;
} flash_ctx_t;

typedef struct
{
  lfs_file_t file;
  storage_stream_info_t info;
  uint32_t data_remaining;
  uint32_t data_offset;
  uint8_t active;
  uint8_t info_valid;
  uint8_t eof;
  uint8_t error;
} storage_stream_state_t;

typedef struct
{
  char path[STORAGE_PATH_MAX];
  uint8_t data[STORAGE_DATA_MAX];
  uint32_t data_len;
} storage_req_payload_t;

static lfs_t s_lfs;
static struct lfs_config s_cfg;
static uint8_t s_read_buf[STORAGE_CACHE_SIZE];
static uint8_t s_prog_buf[STORAGE_CACHE_SIZE];
static uint8_t s_lookahead_buf[STORAGE_LOOKAHEAD_SIZE];
static uint8_t s_file_buf[STORAGE_CACHE_SIZE];
static const struct lfs_file_config s_file_cfg = { .buffer = s_file_buf };
static flash_ctx_t s_flash_ctx = { STORAGE_FLASH_BASE, STORAGE_FLASH_SIZE };
static storage_stream_state_t s_stream;
static uint8_t s_stream_buf[STORAGE_STREAM_BUF_SIZE];
static volatile uint32_t s_stream_wr = 0U;
static volatile uint32_t s_stream_rd = 0U;
static osPriority_t s_stream_prio_prev = osPriorityError;
static uint8_t s_stream_prio_boost = 0U;

static storage_req_payload_t s_req;
static volatile uint8_t s_req_pending = 0U;
static storage_status_t s_status;
static uint8_t s_mounted = 0U;
static uint8_t s_flash_in_dpd = 0U;
static uint8_t s_stream_active = 0U;

static uint8_t s_readback[STORAGE_DATA_MAX];

static const char k_test_path[] = "/test.txt";
static const uint8_t k_test_data[] = "PeepShow littlefs test\n";
static const char k_settings_path[] = SETTINGS_PATH;
static const char k_stream_path[] = "/music.wav";

static uint32_t storage_read_u32_le(const uint8_t *data)
{
  return (uint32_t)data[0]
         | ((uint32_t)data[1] << 8)
         | ((uint32_t)data[2] << 16)
         | ((uint32_t)data[3] << 24);
}

static uint16_t storage_read_u16_le(const uint8_t *data)
{
  return (uint16_t)data[0] | (uint16_t)((uint16_t)data[1] << 8);
}

static uint32_t storage_music_asset_len(void);
static int storage_write_asset(const char *path, const uint8_t *data, uint32_t len);

static void storage_status_update(storage_op_t op, int32_t err, uint32_t value)
{
  s_status.last_op = op;
  s_status.last_err = err;
  s_status.last_value = value;
  s_status.seq++;
}

static void storage_stream_boost_priority(uint8_t enable)
{
  if (tskStorageHandle == NULL)
  {
    return;
  }

  if (enable != 0U)
  {
    if (s_stream_prio_boost != 0U)
    {
      return;
    }

    osPriority_t prio = osThreadGetPriority(tskStorageHandle);
    if (prio == osPriorityError)
    {
      return;
    }

    s_stream_prio_prev = prio;
    (void)osThreadSetPriority(tskStorageHandle, osPriorityAboveNormal);
    s_stream_prio_boost = 1U;
  }
  else
  {
    if (s_stream_prio_boost == 0U)
    {
      return;
    }

    if (s_stream_prio_prev != osPriorityError)
    {
      (void)osThreadSetPriority(tskStorageHandle, s_stream_prio_prev);
    }
    s_stream_prio_boost = 0U;
  }
}

static void storage_status_refresh_stats(void)
{
  s_status.flash_size = STORAGE_FLASH_SIZE;
  s_status.flash_used = 0U;
  s_status.flash_free = 0U;
  s_status.music_size = 0U;
  s_status.stats_valid = 0U;
  s_status.music_present = 0U;

  if (s_mounted == 0U)
  {
    return;
  }

  lfs_ssize_t used_blocks = lfs_fs_size(&s_lfs);
  if (used_blocks < 0)
  {
    return;
  }

  uint32_t used = (uint32_t)used_blocks * STORAGE_BLOCK_SIZE;
  s_status.flash_used = used;
  s_status.flash_free = (used <= STORAGE_FLASH_SIZE) ? (STORAGE_FLASH_SIZE - used) : 0U;
  s_status.stats_valid = 1U;

  struct lfs_info info;
  int res = lfs_stat(&s_lfs, k_stream_path, &info);
  if (res == 0)
  {
    s_status.music_present = 1U;
    s_status.music_size = (uint32_t)info.size;
  }
}

static void storage_stream_power(uint8_t enable)
{
  if (enable != 0U)
  {
    if (s_stream_active != 0U)
    {
      return;
    }
    s_stream_active = 1U;
    storage_stream_boost_priority(1U);
    if (qSysEventsHandle == NULL)
    {
      return;
    }
    app_sys_event_t sys_event = APP_SYS_EVENT_STREAM_ON;
    if (osMessageQueuePut(qSysEventsHandle, &sys_event, 0U, osWaitForever) != osOK)
    {
      s_stream_active = 0U;
      storage_stream_boost_priority(0U);
    }
  }
  else
  {
    if (s_stream_active == 0U)
    {
      return;
    }
    s_stream_active = 0U;
    storage_stream_boost_priority(0U);
    if (qSysEventsHandle == NULL)
    {
      return;
    }
    app_sys_event_t sys_event = APP_SYS_EVENT_STREAM_OFF;
    (void)osMessageQueuePut(qSysEventsHandle, &sys_event, 0U, osWaitForever);
  }
}

static void storage_stream_reset_buffer(void)
{
  s_stream_wr = 0U;
  s_stream_rd = 0U;
}

static uint32_t storage_stream_used(void)
{
  uint32_t wr = s_stream_wr;
  uint32_t rd = s_stream_rd;
  if (wr < rd)
  {
    return 0U;
  }
  return (wr - rd);
}

static uint32_t storage_stream_free(void)
{
  uint32_t used = storage_stream_used();
  if (used >= STORAGE_STREAM_BUF_SIZE)
  {
    return 0U;
  }
  return (STORAGE_STREAM_BUF_SIZE - used);
}

static void storage_stream_write(const uint8_t *data, uint32_t len)
{
  if ((data == NULL) || (len == 0U))
  {
    return;
  }

  uint32_t wr = s_stream_wr;
  for (uint32_t i = 0U; i < len; ++i)
  {
    s_stream_buf[(wr + i) & STORAGE_STREAM_BUF_MASK] = data[i];
  }
  s_stream_wr = wr + len;
}

static uint32_t storage_stream_read_internal(uint8_t *dst, uint32_t len)
{
  if ((dst == NULL) || (len == 0U))
  {
    return 0U;
  }

  uint32_t avail = storage_stream_used();
  if (len > avail)
  {
    len = avail;
  }

  uint32_t rd = s_stream_rd;
  for (uint32_t i = 0U; i < len; ++i)
  {
    dst[i] = s_stream_buf[(rd + i) & STORAGE_STREAM_BUF_MASK];
  }
  s_stream_rd = rd + len;
  return len;
}

static void storage_stream_clear_state(void)
{
  memset(&s_stream, 0, sizeof(s_stream));
  storage_stream_reset_buffer();
}

static void storage_stream_close_file(void);

static int storage_wav_parse_file(lfs_file_t *file, storage_stream_info_t *out, uint32_t *out_offset)
{
  if ((file == NULL) || (out == NULL))
  {
    return LFS_ERR_INVAL;
  }

  if (lfs_file_seek(&s_lfs, file, 0, LFS_SEEK_SET) < 0)
  {
    return LFS_ERR_IO;
  }

  uint8_t header[12];
  lfs_ssize_t read_len = lfs_file_read(&s_lfs, file, header, sizeof(header));
  if (read_len != (lfs_ssize_t)sizeof(header))
  {
    return LFS_ERR_IO;
  }

  if ((header[0] != 'R') || (header[1] != 'I') || (header[2] != 'F') || (header[3] != 'F') ||
      (header[8] != 'W') || (header[9] != 'A') || (header[10] != 'V') || (header[11] != 'E'))
  {
    return LFS_ERR_INVAL;
  }

  uint8_t fmt_found = 0U;
  uint8_t data_found = 0U;
  uint16_t fmt_audio = 0U;
  uint16_t fmt_channels = 0U;
  uint32_t fmt_rate = 0U;
  uint16_t fmt_align = 0U;
  uint16_t fmt_bits = 0U;
  uint16_t fmt_cb_size = 0U;
  uint16_t fmt_samples_per_block = 0U;
  uint32_t data_bytes = 0U;
  uint32_t data_offset = 0U;

  for (;;)
  {
    uint8_t chunk_hdr[8];
    read_len = lfs_file_read(&s_lfs, file, chunk_hdr, sizeof(chunk_hdr));
    if (read_len != (lfs_ssize_t)sizeof(chunk_hdr))
    {
      break;
    }

    uint32_t chunk_size = storage_read_u32_le(&chunk_hdr[4]);
    lfs_soff_t chunk_pos = lfs_file_tell(&s_lfs, file);
    if (chunk_pos < 0)
    {
      return LFS_ERR_IO;
    }

    if ((chunk_hdr[0] == 'f') && (chunk_hdr[1] == 'm') &&
        (chunk_hdr[2] == 't') && (chunk_hdr[3] == ' '))
    {
      if (chunk_size < 16U)
      {
        return LFS_ERR_INVAL;
      }

      uint8_t fmt_buf[32];
      uint32_t to_read = (chunk_size > sizeof(fmt_buf)) ? (uint32_t)sizeof(fmt_buf) : chunk_size;
      read_len = lfs_file_read(&s_lfs, file, fmt_buf, to_read);
      if (read_len != (lfs_ssize_t)to_read)
      {
        return LFS_ERR_IO;
      }

      fmt_audio = storage_read_u16_le(&fmt_buf[0]);
      fmt_channels = storage_read_u16_le(&fmt_buf[2]);
      fmt_rate = storage_read_u32_le(&fmt_buf[4]);
      fmt_align = storage_read_u16_le(&fmt_buf[12]);
      fmt_bits = storage_read_u16_le(&fmt_buf[14]);
      if (chunk_size >= 18U)
      {
        fmt_cb_size = storage_read_u16_le(&fmt_buf[16]);
      }
      if ((fmt_audio == (uint16_t)WAV_FORMAT_IMA_ADPCM) && (chunk_size >= 20U) &&
          (fmt_cb_size >= 2U) && (to_read >= 20U))
      {
        fmt_samples_per_block = storage_read_u16_le(&fmt_buf[18]);
      }
      fmt_found = 1U;
    }
    else if ((chunk_hdr[0] == 'd') && (chunk_hdr[1] == 'a') &&
             (chunk_hdr[2] == 't') && (chunk_hdr[3] == 'a'))
    {
      data_offset = (uint32_t)chunk_pos;
      data_bytes = chunk_size;
      data_found = 1U;
    }

    uint32_t skip = chunk_size + ((chunk_size & 1U) != 0U ? 1U : 0U);
    lfs_soff_t next_pos = chunk_pos + (lfs_soff_t)skip;
    if (lfs_file_seek(&s_lfs, file, next_pos, LFS_SEEK_SET) < 0)
    {
      return LFS_ERR_IO;
    }

    if ((fmt_found != 0U) && (data_found != 0U))
    {
      break;
    }
  }

  if ((fmt_found == 0U) || (data_found == 0U))
  {
    return LFS_ERR_INVAL;
  }

  if ((fmt_audio != (uint16_t)WAV_FORMAT_IMA_ADPCM) || (fmt_channels != 1U) ||
      (fmt_bits != 4U) || (fmt_align <= 4U))
  {
    return LFS_ERR_INVAL;
  }

  if (fmt_samples_per_block == 0U)
  {
    fmt_samples_per_block = (uint16_t)(((fmt_align - 4U) * 2U) + 1U);
  }
  if (fmt_samples_per_block == 0U)
  {
    return LFS_ERR_INVAL;
  }

  out->format = STORAGE_STREAM_FORMAT_IMA_ADPCM;
  out->sample_rate = fmt_rate;
  out->channels = fmt_channels;
  out->block_align = fmt_align;
  out->samples_per_block = fmt_samples_per_block;
  out->data_bytes = data_bytes;

  if (out_offset != NULL)
  {
    *out_offset = data_offset;
  }

  return 0;
}

static int storage_stream_open_file(const char *path)
{
  if (path == NULL)
  {
    s_stream.error = 1U;
    return LFS_ERR_INVAL;
  }

  storage_stream_close_file();

  uint32_t asset_len = storage_music_asset_len();
  if (asset_len == 0U)
  {
    s_stream.error = 1U;
    return LFS_ERR_INVAL;
  }

  struct lfs_info info;
  int res = lfs_stat(&s_lfs, path, &info);
  if ((res != 0) || ((uint32_t)info.size != asset_len))
  {
    res = storage_write_asset(path, musicWav, asset_len);
    if (res != 0)
    {
      s_stream.error = 1U;
      return res;
    }
  }

  res = lfs_file_opencfg(&s_lfs, &s_stream.file, path, LFS_O_RDONLY, &s_file_cfg);
  if (res < 0)
  {
    s_stream.error = 1U;
    return res;
  }

  storage_stream_info_t wav_info = {0};
  uint32_t data_offset = 0U;
  res = storage_wav_parse_file(&s_stream.file, &wav_info, &data_offset);
  if (res != 0)
  {
    (void)lfs_file_close(&s_lfs, &s_stream.file);
    s_stream.error = 1U;
    return res;
  }

  if (lfs_file_seek(&s_lfs, &s_stream.file, (lfs_soff_t)data_offset, LFS_SEEK_SET) < 0)
  {
    (void)lfs_file_close(&s_lfs, &s_stream.file);
    s_stream.error = 1U;
    return LFS_ERR_IO;
  }

  s_stream.info = wav_info;
  s_stream.data_offset = data_offset;
  s_stream.data_remaining = wav_info.data_bytes;
  s_stream.info_valid = 1U;
  s_stream.active = 1U;
  s_stream.eof = 0U;
  s_stream.error = 0U;

  storage_stream_reset_buffer();
  storage_stream_power(1U);
  return 0;
}

static void storage_stream_close_file(void)
{
  if (s_stream.active != 0U)
  {
    (void)lfs_file_close(&s_lfs, &s_stream.file);
  }
  storage_stream_power(0U);
  storage_stream_clear_state();
}

static void storage_stream_fill(void)
{
  if (s_stream.active == 0U)
  {
    return;
  }

  if (s_stream.data_remaining == 0U)
  {
    s_stream.eof = 1U;
    return;
  }

  uint32_t loops = 0U;
  while ((s_stream.data_remaining > 0U) && (loops < STORAGE_STREAM_FILL_MAX_LOOPS))
  {
    uint32_t free_bytes = storage_stream_free();
    if (free_bytes == 0U)
    {
      break;
    }

    uint32_t chunk = free_bytes;
    if (chunk > STORAGE_DATA_MAX)
    {
      chunk = STORAGE_DATA_MAX;
    }
    if (chunk > s_stream.data_remaining)
    {
      chunk = s_stream.data_remaining;
    }

    lfs_ssize_t read_len = lfs_file_read(&s_lfs, &s_stream.file, s_readback, (lfs_size_t)chunk);
    if (read_len < 0)
    {
      s_stream.error = 1U;
      s_stream.data_remaining = 0U;
      break;
    }
    if (read_len == 0)
    {
      s_stream.data_remaining = 0U;
      s_stream.eof = 1U;
      break;
    }

    storage_stream_write(s_readback, (uint32_t)read_len);
    s_stream.data_remaining -= (uint32_t)read_len;
    loops++;
  }
}

static uint32_t storage_music_asset_len(void)
{
  if ((musicWav[0] != 'R') || (musicWav[1] != 'I') || (musicWav[2] != 'F') || (musicWav[3] != 'F'))
  {
    return 0U;
  }
  if ((musicWav[8] != 'W') || (musicWav[9] != 'A') || (musicWav[10] != 'V') || (musicWav[11] != 'E'))
  {
    return 0U;
  }

  uint32_t riff_size = storage_read_u32_le(&musicWav[4]);
  uint32_t total = riff_size + 8U;
  if ((total < 44U) || (total > STORAGE_FLASH_SIZE))
  {
    return 0U;
  }
  return total;
}

static int storage_write_asset(const char *path, const uint8_t *data, uint32_t len)
{
  lfs_file_t file;
  int res = lfs_file_opencfg(&s_lfs, &file, path,
                             LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC,
                             &s_file_cfg);
  if (res < 0)
  {
    return res;
  }

  uint32_t offset = 0U;
  while (offset < len)
  {
    uint32_t chunk = len - offset;
    if (chunk > STORAGE_DATA_MAX)
    {
      chunk = STORAGE_DATA_MAX;
    }

    lfs_ssize_t wrote = lfs_file_write(&s_lfs, &file, &data[offset], (lfs_size_t)chunk);
    if (wrote < 0)
    {
      res = (int)wrote;
      break;
    }
    if ((uint32_t)wrote != chunk)
    {
      res = LFS_ERR_IO;
      break;
    }

    offset += chunk;
  }

  int close_res = lfs_file_close(&s_lfs, &file);
  if ((res == 0) && (close_res < 0))
  {
    res = close_res;
  }

  return res;
}

static void flash_cmd_init(OSPI_RegularCmdTypeDef *cmd)
{
  memset(cmd, 0, sizeof(*cmd));
  cmd->OperationType = HAL_OSPI_OPTYPE_COMMON_CFG;
  cmd->FlashId = HAL_OSPI_FLASH_ID_1;
  cmd->InstructionMode = HAL_OSPI_INSTRUCTION_1_LINE;
  cmd->InstructionSize = HAL_OSPI_INSTRUCTION_8_BITS;
  cmd->InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;
  cmd->AddressMode = HAL_OSPI_ADDRESS_NONE;
  cmd->AddressSize = HAL_OSPI_ADDRESS_24_BITS;
  cmd->AddressDtrMode = HAL_OSPI_ADDRESS_DTR_DISABLE;
  cmd->AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;
  cmd->AlternateBytesDtrMode = HAL_OSPI_ALTERNATE_BYTES_DTR_DISABLE;
  cmd->DataMode = HAL_OSPI_DATA_NONE;
  cmd->DataDtrMode = HAL_OSPI_DATA_DTR_DISABLE;
  cmd->DummyCycles = 0;
  cmd->DQSMode = HAL_OSPI_DQS_DISABLE;
  cmd->SIOOMode = HAL_OSPI_SIOO_INST_EVERY_CMD;
}

static int flash_send_simple(uint8_t instruction)
{
  OSPI_RegularCmdTypeDef cmd;
  flash_cmd_init(&cmd);
  cmd.Instruction = instruction;
  if (HAL_OSPI_Command(&hospi1, &cmd, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    return -1;
  }
  return 0;
}

static int flash_read_status(uint8_t *status)
{
  OSPI_RegularCmdTypeDef cmd;
  flash_cmd_init(&cmd);
  cmd.Instruction = FLASH_CMD_READ_STATUS;
  cmd.DataMode = HAL_OSPI_DATA_1_LINE;
  cmd.NbData = 1U;
  if (HAL_OSPI_Command(&hospi1, &cmd, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    return -1;
  }
  if (HAL_OSPI_Receive(&hospi1, status, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    return -1;
  }
  return 0;
}

static int flash_wait_ready(uint32_t timeout_ms)
{
  uint32_t start = HAL_GetTick();
  uint8_t status = 0U;
  while ((HAL_GetTick() - start) < timeout_ms)
  {
    if (flash_read_status(&status) != 0)
    {
      return -1;
    }
    if ((status & 0x01U) == 0U)
    {
      return 0;
    }
    osDelay(1U);
  }
  return -1;
}

static int flash_write_enable(void)
{
  if (flash_send_simple(FLASH_CMD_WRITE_ENABLE) != 0)
  {
    return -1;
  }
  return 0;
}

static int flash_release_dpd(void)
{
  if (s_flash_in_dpd == 0U)
  {
    return 0;
  }
  if (flash_send_simple(FLASH_CMD_DPD_RELEASE) != 0)
  {
    return -1;
  }
  osDelay(1U);
  s_flash_in_dpd = 0U;
  return 0;
}

static int flash_enter_dpd(void)
{
  if (s_flash_in_dpd != 0U)
  {
    return 0;
  }
  if (flash_wait_ready(STORAGE_TIMEOUT_MS) != 0)
  {
    return -1;
  }
  if (flash_send_simple(FLASH_CMD_DPD_ENTER) != 0)
  {
    return -1;
  }
  s_flash_in_dpd = 1U;
  return 0;
}

static int flash_read(uint32_t addr, void *buffer, uint32_t size)
{
  if (flash_release_dpd() != 0)
  {
    return -1;
  }

  OSPI_RegularCmdTypeDef cmd;
  flash_cmd_init(&cmd);
  cmd.Instruction = FLASH_CMD_READ_DATA;
  cmd.AddressMode = HAL_OSPI_ADDRESS_1_LINE;
  cmd.Address = addr;
  cmd.AddressSize = HAL_OSPI_ADDRESS_24_BITS;
  cmd.DataMode = HAL_OSPI_DATA_1_LINE;
  cmd.NbData = size;

  if (HAL_OSPI_Command(&hospi1, &cmd, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    return -1;
  }
  if (HAL_OSPI_Receive(&hospi1, buffer, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    return -1;
  }
  return 0;
}

static int flash_prog(uint32_t addr, const uint8_t *data, uint32_t size)
{
  if (flash_release_dpd() != 0)
  {
    return -1;
  }

  while (size > 0U)
  {
    uint32_t page_off = addr & (STORAGE_FLASH_PAGE_SIZE - 1U);
    uint32_t chunk = STORAGE_FLASH_PAGE_SIZE - page_off;
    if (chunk > size)
    {
      chunk = size;
    }

    if (flash_write_enable() != 0)
    {
      return -1;
    }

    OSPI_RegularCmdTypeDef cmd;
    flash_cmd_init(&cmd);
    cmd.Instruction = FLASH_CMD_PAGE_PROGRAM;
    cmd.AddressMode = HAL_OSPI_ADDRESS_1_LINE;
    cmd.Address = addr;
    cmd.AddressSize = HAL_OSPI_ADDRESS_24_BITS;
    cmd.DataMode = HAL_OSPI_DATA_1_LINE;
    cmd.NbData = chunk;

    if (HAL_OSPI_Command(&hospi1, &cmd, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
      return -1;
    }
    if (HAL_OSPI_Transmit(&hospi1, (uint8_t *)data, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
      return -1;
    }
    if (flash_wait_ready(STORAGE_TIMEOUT_MS) != 0)
    {
      return -1;
    }

    addr += chunk;
    data += chunk;
    size -= chunk;
  }

  return 0;
}

static int flash_erase(uint32_t addr)
{
  if (flash_release_dpd() != 0)
  {
    return -1;
  }

  if (flash_write_enable() != 0)
  {
    return -1;
  }

  OSPI_RegularCmdTypeDef cmd;
  flash_cmd_init(&cmd);
  cmd.Instruction = FLASH_CMD_SECTOR_ERASE;
  cmd.AddressMode = HAL_OSPI_ADDRESS_1_LINE;
  cmd.Address = addr;
  cmd.AddressSize = HAL_OSPI_ADDRESS_24_BITS;

  if (HAL_OSPI_Command(&hospi1, &cmd, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    return -1;
  }
  if (flash_wait_ready(STORAGE_TIMEOUT_MS) != 0)
  {
    return -1;
  }
  return 0;
}

static int lfs_bd_read(const struct lfs_config *c, lfs_block_t block,
                       lfs_off_t off, void *buffer, lfs_size_t size)
{
  flash_ctx_t *ctx = (flash_ctx_t *)c->context;
  if ((ctx == NULL) || (block >= c->block_count) ||
      ((off + size) > c->block_size))
  {
    return LFS_ERR_IO;
  }

  uint32_t addr = ctx->base + (block * c->block_size) + off;
  if ((addr + size) > (ctx->base + ctx->size))
  {
    return LFS_ERR_IO;
  }

  if (flash_read(addr, buffer, size) != 0)
  {
    return LFS_ERR_IO;
  }

  return 0;
}

static int lfs_bd_prog(const struct lfs_config *c, lfs_block_t block,
                       lfs_off_t off, const void *buffer, lfs_size_t size)
{
  flash_ctx_t *ctx = (flash_ctx_t *)c->context;
  if ((ctx == NULL) || (block >= c->block_count) ||
      ((off + size) > c->block_size))
  {
    return LFS_ERR_IO;
  }

  uint32_t addr = ctx->base + (block * c->block_size) + off;
  if ((addr + size) > (ctx->base + ctx->size))
  {
    return LFS_ERR_IO;
  }

  if (flash_prog(addr, (const uint8_t *)buffer, size) != 0)
  {
    return LFS_ERR_IO;
  }

  return 0;
}

static int lfs_bd_erase(const struct lfs_config *c, lfs_block_t block)
{
  flash_ctx_t *ctx = (flash_ctx_t *)c->context;
  if ((ctx == NULL) || (block >= c->block_count))
  {
    return LFS_ERR_IO;
  }

  uint32_t addr = ctx->base + (block * c->block_size);
  if ((addr + c->block_size) > (ctx->base + ctx->size))
  {
    return LFS_ERR_IO;
  }

  if (flash_erase(addr) != 0)
  {
    return LFS_ERR_IO;
  }

  return 0;
}

static int lfs_bd_sync(const struct lfs_config *c)
{
  (void)c;
  return 0;
}

static void storage_init_config(void)
{
  memset(&s_cfg, 0, sizeof(s_cfg));
  s_cfg.context = &s_flash_ctx;
  s_cfg.read = lfs_bd_read;
  s_cfg.prog = lfs_bd_prog;
  s_cfg.erase = lfs_bd_erase;
  s_cfg.sync = lfs_bd_sync;
  s_cfg.read_size = STORAGE_READ_SIZE;
  s_cfg.prog_size = STORAGE_PROG_SIZE;
  s_cfg.block_size = STORAGE_BLOCK_SIZE;
  s_cfg.block_count = STORAGE_BLOCK_COUNT;
  s_cfg.block_cycles = 500;
  s_cfg.cache_size = STORAGE_CACHE_SIZE;
  s_cfg.lookahead_size = STORAGE_LOOKAHEAD_SIZE;
  s_cfg.read_buffer = s_read_buf;
  s_cfg.prog_buffer = s_prog_buf;
  s_cfg.lookahead_buffer = s_lookahead_buf;
}

static int storage_unmount(void)
{
  if (s_mounted == 0U)
  {
    return 0;
  }
  int res = lfs_unmount(&s_lfs);
  if (res == 0)
  {
    s_mounted = 0U;
    s_status.mount_state = STORAGE_MOUNT_UNMOUNTED;
  }
  return res;
}

static int storage_mount(storage_op_t op)
{
  if (flash_release_dpd() != 0)
  {
    s_status.mount_state = STORAGE_MOUNT_ERROR;
    storage_status_update(op, LFS_ERR_IO, 0U);
    return LFS_ERR_IO;
  }

  int res = lfs_mount(&s_lfs, &s_cfg);
  if (res == 0)
  {
    s_mounted = 1U;
    s_status.mount_state = STORAGE_MOUNT_MOUNTED;
  }
  else
  {
    s_mounted = 0U;
    s_status.mount_state = STORAGE_MOUNT_ERROR;
  }
  storage_status_update(op, res, 0U);
  return res;
}

static int storage_op_write(const char *path, const uint8_t *data, uint32_t len)
{
  lfs_file_t file;
  int res = lfs_file_opencfg(&s_lfs, &file, path,
                             LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC,
                             &s_file_cfg);
  if (res < 0)
  {
    return res;
  }

  lfs_ssize_t wrote = lfs_file_write(&s_lfs, &file, data, len);
  if (wrote < 0)
  {
    (void)lfs_file_close(&s_lfs, &file);
    return (int)wrote;
  }

  res = lfs_file_close(&s_lfs, &file);
  if (res < 0)
  {
    return res;
  }

  if ((uint32_t)wrote != len)
  {
    return LFS_ERR_IO;
  }

  return 0;
}

static int storage_op_read(const char *path, uint32_t *out_len)
{
  lfs_file_t file;
  int res = lfs_file_opencfg(&s_lfs, &file, path, LFS_O_RDONLY, &s_file_cfg);
  if (res < 0)
  {
    return res;
  }

  lfs_ssize_t read_len = lfs_file_read(&s_lfs, &file, s_readback, STORAGE_DATA_MAX);
  if (read_len < 0)
  {
    (void)lfs_file_close(&s_lfs, &file);
    return (int)read_len;
  }

  res = lfs_file_close(&s_lfs, &file);
  if (res < 0)
  {
    return res;
  }

  if (out_len != NULL)
  {
    *out_len = (uint32_t)read_len;
  }

  return 0;
}

static int storage_op_stream_read(const char *path, uint32_t *out_len)
{
  lfs_file_t file;
  int res = lfs_file_opencfg(&s_lfs, &file, path, LFS_O_RDONLY, &s_file_cfg);
  if (res < 0)
  {
    return res;
  }

  storage_stream_power(1U);

  uint32_t total = 0U;
  for (;;)
  {
    lfs_ssize_t read_len = lfs_file_read(&s_lfs, &file, s_readback, sizeof(s_readback));
    if (read_len < 0)
    {
      res = (int)read_len;
      break;
    }
    if (read_len == 0)
    {
      res = 0;
      break;
    }
    total += (uint32_t)read_len;
  }

  int close_res = lfs_file_close(&s_lfs, &file);
  if ((res == 0) && (close_res < 0))
  {
    res = close_res;
  }

  storage_stream_power(0U);

  if (out_len != NULL)
  {
    *out_len = total;
  }

  return res;
}

static int storage_op_stream_test(const char *path, uint32_t *out_len)
{
  uint32_t asset_len = storage_music_asset_len();
  if (asset_len == 0U)
  {
    return LFS_ERR_INVAL;
  }

  struct lfs_info info;
  int res = lfs_stat(&s_lfs, path, &info);
  if ((res != 0) || ((uint32_t)info.size != asset_len))
  {
    res = storage_write_asset(path, musicWav, asset_len);
    if (res != 0)
    {
      return res;
    }
  }

  return storage_op_stream_read(path, out_len);
}

static int storage_op_list(const char *path, uint32_t *out_count)
{
  lfs_dir_t dir;
  struct lfs_info info;
  uint32_t count = 0U;

  int res = lfs_dir_open(&s_lfs, &dir, path);
  if (res < 0)
  {
    return res;
  }

  while ((res = lfs_dir_read(&s_lfs, &dir, &info)) > 0)
  {
    if ((strcmp(info.name, ".") == 0) || (strcmp(info.name, "..") == 0))
    {
      continue;
    }
    count++;
  }

  int close_res = lfs_dir_close(&s_lfs, &dir);
  if (res < 0)
  {
    return res;
  }
  if (close_res < 0)
  {
    return close_res;
  }

  if (out_count != NULL)
  {
    *out_count = count;
  }

  return 0;
}

static int storage_op_exists(const char *path, uint32_t *out_exists)
{
  struct lfs_info info;
  int res = lfs_stat(&s_lfs, path, &info);
  if (res == 0)
  {
    if (out_exists != NULL)
    {
      *out_exists = 1U;
    }
    return 0;
  }
  if (res == LFS_ERR_NOENT)
  {
    if (out_exists != NULL)
    {
      *out_exists = 0U;
    }
    return 0;
  }
  return res;
}

static int storage_op_delete(const char *path, uint32_t *out_removed)
{
  int res = lfs_remove(&s_lfs, path);
  if (res == 0)
  {
    if (out_removed != NULL)
    {
      *out_removed = 1U;
    }
    return 0;
  }
  if (res == LFS_ERR_NOENT)
  {
    if (out_removed != NULL)
    {
      *out_removed = 0U;
    }
    return 0;
  }
  return res;
}

  static int storage_op_test(void)
  {
  int res = storage_op_write(k_test_path, k_test_data,
                             (uint32_t)(sizeof(k_test_data) - 1U));
  storage_status_update(STORAGE_OP_WRITE, res, (uint32_t)(sizeof(k_test_data) - 1U));
  if (res != 0)
  {
    return res;
  }

  uint32_t read_len = 0U;
  res = storage_op_read(k_test_path, &read_len);
  storage_status_update(STORAGE_OP_READ, res, read_len);
  if (res != 0)
  {
    return res;
  }

  if ((read_len != (sizeof(k_test_data) - 1U)) ||
      (memcmp(s_readback, k_test_data, read_len) != 0))
  {
    storage_status_update(STORAGE_OP_READ, LFS_ERR_CORRUPT, read_len);
    return LFS_ERR_CORRUPT;
  }

  uint32_t exists = 0U;
  res = storage_op_exists(k_test_path, &exists);
  storage_status_update(STORAGE_OP_EXISTS, res, exists);
  if (res != 0)
  {
    return res;
  }

  uint32_t count = 0U;
    res = storage_op_list("/", &count);
    storage_status_update(STORAGE_OP_LIST, res, count);
    return res;
  }

static int storage_save_settings(void)
{
  uint32_t len = 0U;
  if (!settings_encode(s_readback, sizeof(s_readback), &len))
  {
    storage_status_update(STORAGE_OP_SAVE_SETTINGS, LFS_ERR_INVAL, 0U);
    return LFS_ERR_INVAL;
  }

  int res = storage_op_write(k_settings_path, s_readback, len);
  storage_status_update(STORAGE_OP_SAVE_SETTINGS, res, len);
  return res;
}

static void storage_load_settings(void)
{
  uint32_t len = 0U;
  int res = storage_op_read(k_settings_path, &len);
  storage_status_update(STORAGE_OP_LOAD_SETTINGS, res, len);
  if (res == 0)
  {
    if (!settings_decode(s_readback, len))
    {
      settings_reset_defaults();
    }
  }
  else if (res != LFS_ERR_NOENT)
  {
    settings_reset_defaults();
  }
}

static const char *storage_request_path(const char *fallback)
{
  if (s_req.path[0] != '\0')
  {
    return s_req.path;
  }
  return fallback;
}

static void storage_handle_request(storage_op_t op)
{
  if ((op != STORAGE_OP_MOUNT) && (op != STORAGE_OP_REMOUNT) &&
      (op != STORAGE_OP_DPD_ENTER) && (op != STORAGE_OP_DPD_EXIT))
  {
    if (s_mounted == 0U)
    {
      storage_status_update(op, LFS_ERR_IO, 0U);
      storage_status_refresh_stats();
      return;
    }
  }

  switch (op)
  {
    case STORAGE_OP_REMOUNT:
    {
      (void)storage_unmount();
      if (storage_mount(STORAGE_OP_REMOUNT) == 0)
      {
        storage_load_settings();
      }
      break;
    }
    case STORAGE_OP_WRITE:
    {
      uint32_t value = s_req.data_len;
      int res = storage_op_write(storage_request_path(k_test_path), s_req.data, s_req.data_len);
      storage_status_update(STORAGE_OP_WRITE, res, value);
      break;
    }
    case STORAGE_OP_READ:
    {
      uint32_t value = 0U;
      int res = storage_op_read(storage_request_path(k_test_path), &value);
      storage_status_update(STORAGE_OP_READ, res, value);
      break;
    }
    case STORAGE_OP_STREAM_READ:
    {
      uint32_t value = 0U;
      int res = storage_op_stream_read(storage_request_path(k_test_path), &value);
      storage_status_update(STORAGE_OP_STREAM_READ, res, value);
      break;
    }
    case STORAGE_OP_STREAM_OPEN:
    {
      uint32_t value = 0U;
      int res = storage_stream_open_file(storage_request_path(k_stream_path));
      if (res == 0)
      {
        value = s_stream.info.data_bytes;
        storage_stream_fill();
      }
      storage_status_update(STORAGE_OP_STREAM_OPEN, res, value);
      break;
    }
    case STORAGE_OP_STREAM_CLOSE:
    {
      storage_stream_close_file();
      storage_status_update(STORAGE_OP_STREAM_CLOSE, 0, 0U);
      break;
    }
    case STORAGE_OP_STREAM_TEST:
    {
      uint32_t value = 0U;
      int res = storage_op_stream_test(storage_request_path(k_stream_path), &value);
      storage_status_update(STORAGE_OP_STREAM_TEST, res, value);
      break;
    }
    case STORAGE_OP_LIST:
    {
      uint32_t count = 0U;
      int res = storage_op_list(storage_request_path("/"), &count);
      storage_status_update(STORAGE_OP_LIST, res, count);
      break;
    }
    case STORAGE_OP_DELETE:
    {
      uint32_t removed = 0U;
      int res = storage_op_delete(storage_request_path(k_test_path), &removed);
      storage_status_update(STORAGE_OP_DELETE, res, removed);
      break;
    }
    case STORAGE_OP_EXISTS:
    {
      uint32_t exists = 0U;
      int res = storage_op_exists(storage_request_path(k_test_path), &exists);
      storage_status_update(STORAGE_OP_EXISTS, res, exists);
      break;
    }
    case STORAGE_OP_TEST:
    {
      (void)storage_op_test();
      break;
    }
    case STORAGE_OP_SAVE_SETTINGS:
    {
      (void)storage_save_settings();
      break;
    }
    case STORAGE_OP_DPD_ENTER:
    {
      (void)storage_unmount();
      int res = flash_enter_dpd();
      storage_status_update(STORAGE_OP_DPD_ENTER, (res == 0) ? 0 : LFS_ERR_IO, 0U);
      break;
    }
    case STORAGE_OP_DPD_EXIT:
    {
      int res = flash_release_dpd();
      storage_status_update(STORAGE_OP_DPD_EXIT, (res == 0) ? 0 : LFS_ERR_IO, 0U);
      break;
    }
    default:
      break;
  }

  storage_status_refresh_stats();
}

static bool storage_request_submit(storage_op_t op, const char *path, const uint8_t *data, uint32_t len)
{
  if (qStorageReqHandle == NULL)
  {
    return false;
  }

  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  if (s_req_pending != 0U)
  {
    __set_PRIMASK(primask);
    return false;
  }
  s_req_pending = 1U;
  __set_PRIMASK(primask);

  if (path != NULL)
  {
    (void)strncpy(s_req.path, path, STORAGE_PATH_MAX - 1U);
    s_req.path[STORAGE_PATH_MAX - 1U] = '\0';
  }
  else
  {
    s_req.path[0] = '\0';
  }

  s_req.data_len = 0U;
  if ((data != NULL) && (len > 0U))
  {
    if (len > STORAGE_DATA_MAX)
    {
      len = STORAGE_DATA_MAX;
    }
    (void)memcpy(s_req.data, data, len);
    s_req.data_len = len;
  }

  app_storage_req_t req = (app_storage_req_t)op;
  if (osMessageQueuePut(qStorageReqHandle, &req, 0U, 0U) != osOK)
  {
    s_req_pending = 0U;
    return false;
  }

  return true;
}

void storage_task_run(void)
{
  memset(&s_status, 0, sizeof(s_status));
  s_status.mount_state = STORAGE_MOUNT_UNMOUNTED;
  storage_init_config();
  storage_stream_clear_state();

  if (storage_mount(STORAGE_OP_MOUNT) == 0)
  {
    storage_load_settings();
  }
  storage_status_refresh_stats();

  for (;;)
  {
    app_storage_req_t req = 0U;
    uint32_t timeout = (s_stream.active != 0U) ? 5U : osWaitForever;
    if (osMessageQueueGet(qStorageReqHandle, &req, NULL, timeout) != osOK)
    {
      if (s_stream.active != 0U)
      {
        storage_stream_fill();
      }
      continue;
    }
    storage_handle_request((storage_op_t)req);
    s_req_pending = 0U;

    if (s_stream.active != 0U)
    {
      storage_stream_fill();
    }
  }
}

void storage_get_status(storage_status_t *out)
{
  if (out == NULL)
  {
    return;
  }
  *out = s_status;
}

bool storage_stream_get_info(storage_stream_info_t *out)
{
  if (out == NULL)
  {
    return false;
  }

  if ((s_stream.active == 0U) || (s_stream.info_valid == 0U))
  {
    return false;
  }

  *out = s_stream.info;
  return true;
}

uint8_t storage_stream_is_active(void)
{
  return s_stream.active;
}

uint8_t storage_stream_has_error(void)
{
  return s_stream.error;
}

uint32_t storage_stream_available(void)
{
  return storage_stream_used();
}

uint32_t storage_stream_read(uint8_t *dst, uint32_t len)
{
  return storage_stream_read_internal(dst, len);
}

bool storage_request_remount(void)
{
  return storage_request_submit(STORAGE_OP_REMOUNT, NULL, NULL, 0U);
}

bool storage_request_test(void)
{
  return storage_request_submit(STORAGE_OP_TEST, NULL, NULL, 0U);
}

bool storage_request_save_settings(void)
{
  return storage_request_submit(STORAGE_OP_SAVE_SETTINGS, NULL, NULL, 0U);
}

bool storage_request_write(const char *path, const uint8_t *data, uint32_t len)
{
  return storage_request_submit(STORAGE_OP_WRITE, path, data, len);
}

bool storage_request_read(const char *path)
{
  return storage_request_submit(STORAGE_OP_READ, path, NULL, 0U);
}

bool storage_request_list(const char *path)
{
  return storage_request_submit(STORAGE_OP_LIST, path, NULL, 0U);
}

bool storage_request_delete(const char *path)
{
  return storage_request_submit(STORAGE_OP_DELETE, path, NULL, 0U);
}

bool storage_request_exists(const char *path)
{
  return storage_request_submit(STORAGE_OP_EXISTS, path, NULL, 0U);
}

bool storage_request_dpd(bool enable)
{
  return storage_request_submit(enable ? STORAGE_OP_DPD_ENTER : STORAGE_OP_DPD_EXIT,
                                NULL, NULL, 0U);
}

bool storage_request_stream_read(const char *path)
{
  return storage_request_submit(STORAGE_OP_STREAM_READ, path, NULL, 0U);
}

bool storage_request_stream_test(void)
{
  return storage_request_submit(STORAGE_OP_STREAM_TEST, k_stream_path, NULL, 0U);
}

bool storage_request_stream_open(const char *path)
{
  return storage_request_submit(STORAGE_OP_STREAM_OPEN, path, NULL, 0U);
}

bool storage_request_stream_close(void)
{
  return storage_request_submit(STORAGE_OP_STREAM_CLOSE, NULL, NULL, 0U);
}
