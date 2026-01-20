#ifndef SOUND_MANAGER_H
#define SOUND_MANAGER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
  SOUND_FORMAT_IMA_ADPCM = 0,
  SOUND_FORMAT_PCM16 = 1,
  SOUND_FORMAT_TONE = 2
} sound_format_t;

typedef enum
{
  SOUND_SOURCE_LFS = 0,
  SOUND_SOURCE_EMBEDDED = 1,
  SOUND_SOURCE_TONE = 2
} sound_source_t;

typedef enum
{
  SOUND_PRIO_LOW = 0,
  SOUND_PRIO_UI = 1,
  SOUND_PRIO_GAME = 2,
  SOUND_PRIO_MUSIC = 3,
  SOUND_PRIO_SYSTEM = 4
} sound_prio_t;

typedef uint16_t sound_flags_t;

#define SOUND_F_OVERLAP         (1U << 0U)
#define SOUND_F_INTERRUPT       (1U << 1U)
#define SOUND_F_LOOP            (1U << 2U)
#define SOUND_F_ALLOW_SLEEPFACE (1U << 3U)
#define SOUND_F_STREAM          (1U << 4U)

typedef enum
{
  SND_UI_CLICK = 0,
  SND_MUSIC_1 = 1,
  SND_GHOST = 2,
  SND_COUNT
} sound_id_t;

typedef struct
{
  sound_id_t id;
  const char *name;
  sound_format_t format;
  sound_source_t source;
  const char *path;
  const uint8_t *embedded;
  uint32_t embedded_len;
  uint8_t default_gain_q8;
  sound_flags_t flags;
  sound_prio_t default_prio;
  uint8_t *cache;
  uint32_t cache_max;
} sound_registry_entry_t;

typedef enum
{
  SOUND_CACHE_EMPTY = 0,
  SOUND_CACHE_READY = 1,
  SOUND_CACHE_ERROR = 2
} sound_cache_state_t;

uint32_t sound_registry_count(void);
const sound_registry_entry_t *sound_registry_get_by_index(uint32_t index);
const sound_registry_entry_t *sound_registry_get(sound_id_t id);
const sound_registry_entry_t *sound_registry_get_by_path(const char *path);

void sound_play(sound_id_t id);
void sound_play_ex(sound_id_t id, sound_prio_t prio, sound_flags_t flags);

uint8_t sound_cache_get(sound_id_t id, const uint8_t **data, uint32_t *len);
sound_cache_state_t sound_cache_get_state(sound_id_t id);
uint8_t *sound_cache_get_buffer(sound_id_t id, uint32_t *max_len);
void sound_cache_set(sound_id_t id, uint32_t len, uint8_t ok);

#define SOUND_CMD_FLAG (1UL << 31U)
#define SOUND_CMD_TYPE_SHIFT 28U
#define SOUND_CMD_TYPE_MASK (0x7UL << SOUND_CMD_TYPE_SHIFT)
#define SOUND_CMD_TYPE_PLAY (1UL << SOUND_CMD_TYPE_SHIFT)
#define SOUND_CMD_TYPE_STOP (2UL << SOUND_CMD_TYPE_SHIFT)
#define SOUND_CMD_TYPE_STOP_ALL (3UL << SOUND_CMD_TYPE_SHIFT)
#define SOUND_CMD_ID_MASK 0xFFUL
#define SOUND_CMD_PRIO_SHIFT 8U
#define SOUND_CMD_PRIO_MASK 0xFUL
#define SOUND_CMD_FLAGS_SHIFT 12U
#define SOUND_CMD_FLAGS_MASK 0xFFFUL

#define SOUND_CMD_MAKE_PLAY(id, prio, flags) \
  (SOUND_CMD_FLAG | SOUND_CMD_TYPE_PLAY \
   | (((uint32_t)(id)) & SOUND_CMD_ID_MASK) \
   | ((((uint32_t)(prio)) & SOUND_CMD_PRIO_MASK) << SOUND_CMD_PRIO_SHIFT) \
   | ((((uint32_t)(flags)) & SOUND_CMD_FLAGS_MASK) << SOUND_CMD_FLAGS_SHIFT))

#define SOUND_CMD_MAKE_STOP(id) \
  (SOUND_CMD_FLAG | SOUND_CMD_TYPE_STOP \
   | (((uint32_t)(id)) & SOUND_CMD_ID_MASK))

#define SOUND_CMD_MAKE_STOP_ALL() \
  (SOUND_CMD_FLAG | SOUND_CMD_TYPE_STOP_ALL)

#define SOUND_CMD_IS(cmd, type) \
  (((cmd) & (SOUND_CMD_FLAG | SOUND_CMD_TYPE_MASK)) == (SOUND_CMD_FLAG | (type)))

#define SOUND_CMD_GET_ID(cmd) ((sound_id_t)((cmd) & SOUND_CMD_ID_MASK))
#define SOUND_CMD_GET_PRIO(cmd) \
  ((sound_prio_t)(((cmd) >> SOUND_CMD_PRIO_SHIFT) & SOUND_CMD_PRIO_MASK))
#define SOUND_CMD_GET_FLAGS(cmd) \
  ((sound_flags_t)(((cmd) >> SOUND_CMD_FLAGS_SHIFT) & SOUND_CMD_FLAGS_MASK))

#ifdef __cplusplus
}
#endif

#endif /* SOUND_MANAGER_H */
