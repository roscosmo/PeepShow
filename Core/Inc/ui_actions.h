#ifndef UI_ACTIONS_H
#define UI_ACTIONS_H

#include "app_freertos.h"

#ifdef __cplusplus
extern "C" {
#endif

void ui_actions_send_sensor_req(app_sensor_req_t req);

#ifdef __cplusplus
}
#endif

#endif /* UI_ACTIONS_H */
