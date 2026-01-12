#ifndef STORAGE_TASK_H
#define STORAGE_TASK_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define STORAGE_PATH_MAX 64U
#define STORAGE_DATA_MAX 256U

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
  STORAGE_OP_LOAD_SETTINGS = 12
} storage_op_t;

typedef struct
{
  storage_mount_state_t mount_state;
  storage_op_t last_op;
  int32_t last_err;
  uint32_t last_value;
  uint32_t seq;
} storage_status_t;

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

#ifdef __cplusplus
}
#endif

#endif /* STORAGE_TASK_H */
