#ifndef AUDIO_ASSETS_H
#define AUDIO_ASSETS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  const char *path;
  const uint8_t *data;
  uint32_t len;
} audio_asset_t;

uint32_t audio_assets_count(void);
const audio_asset_t *audio_assets_get(uint32_t index);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_ASSETS_H */
