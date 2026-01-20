#include "sound_manager.h"

#include "app_freertos.h"
#include "cmsis_os2.h"

#include <string.h>

static uint8_t s_cache_ui_move[11394];
static uint8_t s_cache_ui_confirm[11394];
static uint8_t s_cache_ui_decline[11394];
static uint8_t s_cache_ui_denied[11394];
static uint8_t s_cache_game_ghost[6238];

static const sound_registry_entry_t s_registry[] =
{
  {
    .id = SND_UI_MOVE,
    .name = "UI Move",
    .format = SOUND_FORMAT_IMA_ADPCM,
    .source = SOUND_SOURCE_LFS,
    .path = "/audio/UI_Move.wav",
    .embedded = NULL,
    .embedded_len = 0U,
    .default_gain_q8 = 255U,
    .flags = SOUND_F_OVERLAP,
    .default_prio = SOUND_PRIO_UI,
    .category = SOUND_CAT_UI,
    .cache = s_cache_ui_move,
    .cache_max = (uint32_t)sizeof(s_cache_ui_move)
  },
  {
    .id = SND_UI_CONFIRM,
    .name = "UI Confirm",
    .format = SOUND_FORMAT_IMA_ADPCM,
    .source = SOUND_SOURCE_LFS,
    .path = "/audio/UI_Confirm.wav",
    .embedded = NULL,
    .embedded_len = 0U,
    .default_gain_q8 = 255U,
    .flags = SOUND_F_OVERLAP,
    .default_prio = SOUND_PRIO_UI,
    .category = SOUND_CAT_UI,
    .cache = s_cache_ui_confirm,
    .cache_max = (uint32_t)sizeof(s_cache_ui_confirm)
  },
  {
    .id = SND_UI_DECLINE,
    .name = "UI Decline",
    .format = SOUND_FORMAT_IMA_ADPCM,
    .source = SOUND_SOURCE_LFS,
    .path = "/audio/UI_Decline.wav",
    .embedded = NULL,
    .embedded_len = 0U,
    .default_gain_q8 = 255U,
    .flags = SOUND_F_OVERLAP,
    .default_prio = SOUND_PRIO_UI,
    .category = SOUND_CAT_UI,
    .cache = s_cache_ui_decline,
    .cache_max = (uint32_t)sizeof(s_cache_ui_decline)
  },
  {
    .id = SND_UI_DENIED,
    .name = "UI Denied",
    .format = SOUND_FORMAT_IMA_ADPCM,
    .source = SOUND_SOURCE_LFS,
    .path = "/audio/UI_Denied.wav",
    .embedded = NULL,
    .embedded_len = 0U,
    .default_gain_q8 = 255U,
    .flags = SOUND_F_OVERLAP,
    .default_prio = SOUND_PRIO_UI,
    .category = SOUND_CAT_UI,
    .cache = s_cache_ui_denied,
    .cache_max = (uint32_t)sizeof(s_cache_ui_denied)
  },
  {
    .id = SND_GAME_GHOST,
    .name = "Game Ghost",
    .format = SOUND_FORMAT_IMA_ADPCM,
    .source = SOUND_SOURCE_LFS,
    .path = "/audio/GAME_ghost.wav",
    .embedded = NULL,
    .embedded_len = 0U,
    .default_gain_q8 = 255U,
    .flags = SOUND_F_OVERLAP,
    .default_prio = SOUND_PRIO_GAME,
    .category = SOUND_CAT_SFX,
    .cache = s_cache_game_ghost,
    .cache_max = (uint32_t)sizeof(s_cache_game_ghost)
  },
  {
    .id = SND_MUSIC_MEGAMAN,
    .name = "Music Megaman",
    .format = SOUND_FORMAT_IMA_ADPCM,
    .source = SOUND_SOURCE_LFS,
    .path = "/audio/GAME_music_megaman.wav",
    .embedded = NULL,
    .embedded_len = 0U,
    .default_gain_q8 = 255U,
    .flags = (sound_flags_t)(SOUND_F_STREAM | SOUND_F_LOOP),
    .default_prio = SOUND_PRIO_MUSIC,
    .category = SOUND_CAT_MUSIC,
    .cache = NULL,
    .cache_max = 0U
  }
};

static sound_cache_state_t s_cache_state[SND_COUNT];
static uint32_t s_cache_len[SND_COUNT];

uint32_t sound_registry_count(void)
{
  return (uint32_t)(sizeof(s_registry) / sizeof(s_registry[0]));
}

const sound_registry_entry_t *sound_registry_get_by_index(uint32_t index)
{
  if (index >= sound_registry_count())
  {
    return NULL;
  }
  return &s_registry[index];
}

const sound_registry_entry_t *sound_registry_get(sound_id_t id)
{
  if ((uint32_t)id < sound_registry_count())
  {
    if (s_registry[id].id == id)
    {
      return &s_registry[id];
    }
  }

  uint32_t count = sound_registry_count();
  for (uint32_t i = 0U; i < count; ++i)
  {
    if (s_registry[i].id == id)
    {
      return &s_registry[i];
    }
  }

  return NULL;
}

const sound_registry_entry_t *sound_registry_get_by_path(const char *path)
{
  if (path == NULL)
  {
    return NULL;
  }

  uint32_t count = sound_registry_count();
  for (uint32_t i = 0U; i < count; ++i)
  {
    const char *entry_path = s_registry[i].path;
    if ((entry_path != NULL) && (strcmp(entry_path, path) == 0))
    {
      return &s_registry[i];
    }
  }

  return NULL;
}

void sound_play(sound_id_t id)
{
  const sound_registry_entry_t *entry = sound_registry_get(id);
  if (entry == NULL)
  {
    return;
  }

  sound_play_ex(id, entry->default_prio, 0U);
}

void sound_play_ex(sound_id_t id, sound_prio_t prio, sound_flags_t flags)
{
  if (qAudioCmdHandle == NULL)
  {
    return;
  }

  app_audio_cmd_t cmd = (app_audio_cmd_t)SOUND_CMD_MAKE_PLAY(id, prio, flags);
  (void)osMessageQueuePut(qAudioCmdHandle, &cmd, 0U, 0U);
}

uint8_t sound_cache_get(sound_id_t id, const uint8_t **data, uint32_t *len)
{
  if (id >= SND_COUNT)
  {
    return 0U;
  }
  if (s_cache_state[id] != SOUND_CACHE_READY)
  {
    return 0U;
  }

  const sound_registry_entry_t *entry = sound_registry_get(id);
  if ((entry == NULL) || (entry->cache == NULL))
  {
    return 0U;
  }

  if (data != NULL)
  {
    *data = entry->cache;
  }
  if (len != NULL)
  {
    *len = s_cache_len[id];
  }

  return 1U;
}

sound_cache_state_t sound_cache_get_state(sound_id_t id)
{
  if (id >= SND_COUNT)
  {
    return SOUND_CACHE_ERROR;
  }
  return s_cache_state[id];
}

uint8_t *sound_cache_get_buffer(sound_id_t id, uint32_t *max_len)
{
  const sound_registry_entry_t *entry = sound_registry_get(id);
  if (entry == NULL)
  {
    if (max_len != NULL)
    {
      *max_len = 0U;
    }
    return NULL;
  }

  if (max_len != NULL)
  {
    *max_len = entry->cache_max;
  }

  return entry->cache;
}

void sound_cache_set(sound_id_t id, uint32_t len, uint8_t ok)
{
  if (id >= SND_COUNT)
  {
    return;
  }

  if ((ok != 0U) && (len > 0U))
  {
    s_cache_state[id] = SOUND_CACHE_READY;
    s_cache_len[id] = len;
  }
  else
  {
    s_cache_state[id] = SOUND_CACHE_ERROR;
    s_cache_len[id] = 0U;
  }
}
