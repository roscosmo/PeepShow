#ifndef AUDIO_TASK_H
#define AUDIO_TASK_H

#include "sound_manager.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void audio_task_run(void);
void audio_set_volume(uint8_t level);
uint8_t audio_get_volume(void);
uint8_t audio_is_active(void);
void audio_set_category_volume(sound_category_t category, uint8_t level);
uint8_t audio_get_category_volume(sound_category_t category);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_TASK_H */
