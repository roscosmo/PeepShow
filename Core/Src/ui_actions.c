#include "ui_actions.h"

#include "cmsis_os2.h"

void ui_actions_send_sensor_req(app_sensor_req_t req)
{
  if (qSensorReqHandle == NULL)
  {
    return;
  }

  (void)osMessageQueuePut(qSensorReqHandle, &req, 0U, 0U);
}
