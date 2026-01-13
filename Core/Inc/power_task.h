#ifndef POWER_TASK_H
#define POWER_TASK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define POWER_QUIESCE_REQ_FLAG    (1UL << 0U)
#define POWER_QUIESCE_ACK_DISPLAY (1UL << 1U)
#define POWER_QUIESCE_ACK_STORAGE (1UL << 2U)
#define POWER_QUIESCE_ACK_AUDIO   (1UL << 3U)
#define POWER_QUIESCE_ACK_SENSOR  (1UL << 4U)

#define POWER_QUIESCE_ACK_MASK (POWER_QUIESCE_ACK_DISPLAY | POWER_QUIESCE_ACK_STORAGE | \
                                POWER_QUIESCE_ACK_AUDIO | POWER_QUIESCE_ACK_SENSOR)

void power_task_run(void);
void power_task_activity_ping(void);
void power_task_request_sleep(void);
void power_task_set_sleep_enabled(uint8_t enabled);
uint8_t power_task_get_sleep_enabled(void);
void power_task_set_inactivity_timeout_ms(uint32_t timeout_ms);
uint32_t power_task_get_inactivity_timeout_ms(void);
void power_task_set_game_sleep_allowed(uint8_t allow);
uint8_t power_task_get_game_sleep_allowed(void);

uint8_t power_task_is_quiescing(void);
void power_task_quiesce_ack(uint32_t ack_flag);
void power_task_quiesce_clear(uint32_t ack_flag);

#ifdef __cplusplus
}
#endif

#endif /* POWER_TASK_H */
