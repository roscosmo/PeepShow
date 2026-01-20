#ifndef STORAGE_TASK_H
#define STORAGE_TASK_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define STORAGE_PATH_MAX 64U
#define STORAGE_DATA_MAX 4096U

typedef enum
{
  STORAGE_MOUNT_UNMOUNTED = 0,
  STORAGE_MOUNT_MOUNTED = 1,
  STORAGE_MOUNT_ERROR = 2
} storage_mount_state_t;

typedef enum
{
  STORAGE_OP_NONE = 0,
  STORAGE_OP_MOUNT = 1,
  STORAGE_OP_REMOUNT = 2,
  STORAGE_OP_WRITE = 3,
  STORAGE_OP_READ = 4,
  STORAGE_OP_LIST = 5,
  STORAGE_OP_DELETE = 6,
  STORAGE_OP_EXISTS = 7,
  STORAGE_OP_TEST = 8,
  STORAGE_OP_DPD_ENTER = 9,
  STORAGE_OP_DPD_EXIT = 10,
  STORAGE_OP_SAVE_SETTINGS = 11,
  STORAGE_OP_LOAD_SETTINGS = 12,
  STORAGE_OP_STREAM_READ = 13,
  STORAGE_OP_STREAM_TEST = 14,
  STORAGE_OP_STREAM_OPEN = 15,
  STORAGE_OP_STREAM_CLOSE = 16,
  STORAGE_OP_AUDIO_LIST = 17
} storage_op_t;

typedef enum
{
  STORAGE_STREAM_FORMAT_NONE = 0,
  STORAGE_STREAM_FORMAT_IMA_ADPCM = 1
} storage_stream_format_t;

typedef struct
{
  storage_stream_format_t format;
  uint32_t sample_rate;
  uint32_t data_bytes;
  uint16_t channels;
  uint16_t block_align;
  uint16_t samples_per_block;
} storage_stream_info_t;

#define STORAGE_AUDIO_NAME_MAX 32U
#define STORAGE_AUDIO_LIST_MAX 16U

typedef struct
{
  char name[STORAGE_AUDIO_NAME_MAX];
  uint32_t size;
} storage_audio_entry_t;

typedef struct
{
  storage_mount_state_t mount_state;
  storage_op_t last_op;
  int32_t last_err;
  uint32_t last_value;
  uint32_t seq;
  uint32_t flash_size;
  uint32_t flash_used;
  uint32_t flash_free;
  uint32_t music_size;
  uint8_t stats_valid;
  uint8_t music_present;
} storage_status_t;

typedef enum
{
  STORAGE_SEED_IDLE = 0,
  STORAGE_SEED_ACTIVE = 1,
  STORAGE_SEED_DONE = 2,
  STORAGE_SEED_ERROR = 3
} storage_seed_state_t;

void storage_task_run(void);
void storage_get_status(storage_status_t *out);

bool storage_request_remount(void);
bool storage_request_test(void);
bool storage_request_save_settings(void);
bool storage_request_write(const char *path, const uint8_t *data, uint32_t len);
bool storage_request_read(const char *path);
bool storage_request_list(const char *path);
bool storage_request_delete(const char *path);
bool storage_request_exists(const char *path);
bool storage_request_dpd(bool enable);
bool storage_request_stream_read(const char *path);
bool storage_request_stream_test(void);
bool storage_request_stream_open(const char *path);
bool storage_request_stream_close(void);
bool storage_request_audio_list(void);

bool storage_stream_get_info(storage_stream_info_t *out);
uint8_t storage_stream_is_active(void);
uint8_t storage_stream_has_error(void);
uint32_t storage_stream_available(void);
uint32_t storage_stream_read(uint8_t *dst, uint32_t len);
uint8_t storage_is_busy(void);
uint32_t storage_audio_list_count(void);
uint32_t storage_audio_list_seq(void);
uint8_t storage_audio_list_get(uint32_t index, storage_audio_entry_t *out);
void storage_set_seed_audio_on_boot(uint8_t enable);
storage_seed_state_t storage_get_seed_state(void);

#ifdef __cplusplus
}
#endif

#endif /* STORAGE_TASK_H */
