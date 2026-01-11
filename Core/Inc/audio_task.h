#ifndef AUDIO_TASK_H
#define AUDIO_TASK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void audio_task_run(void);
void audio_set_volume(uint8_t level);
uint8_t audio_get_volume(void);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_TASK_H */
