#ifndef RTOS_ISR_BRIDGE_H
#define RTOS_ISR_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

void rtos_isr_bridge_handle_exti(uint16_t gpio_pin);

#ifdef __cplusplus
}
#endif

#endif /* RTOS_ISR_BRIDGE_H */
