#ifndef DISPLAY_TASK_H
#define DISPLAY_TASK_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool display_is_busy(void);
void display_task_run(void);

#ifdef __cplusplus
}
#endif

#endif /* DISPLAY_TASK_H */
