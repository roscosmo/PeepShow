#ifndef SLEEP_FACE_H
#define SLEEP_FACE_H

#include "power_task.h"

#ifdef __cplusplus
extern "C" {
#endif

void sleep_face_render(const power_rtc_datetime_t *dt);

#ifdef __cplusplus
}
#endif

#endif /* SLEEP_FACE_H */
