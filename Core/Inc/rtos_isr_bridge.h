#ifndef RTOS_ISR_BRIDGE_H
#define RTOS_ISR_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

void rtos_isr_bridge_handle_exti(uint16_t gpio_pin);
void rtos_isr_bridge_set_nonwake_buttons_enabled(uint8_t enable);

#ifdef __cplusplus
}
#endif

#endif /* RTOS_ISR_BRIDGE_H */
