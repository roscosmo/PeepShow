#include "rtos_isr_bridge.h"

#include "app_freertos.h"
#include "main.h"

typedef struct
{
  uint16_t pin;
  GPIO_TypeDef *port;
  app_button_id_t id;
} app_button_map_t;

static const app_button_map_t kButtonMap[] = {
  { BTN_A_Pin, BTN_A_GPIO_Port, APP_BUTTON_A },
  { BTN_B_Pin, BTN_B_GPIO_Port, APP_BUTTON_B },
  { BTN_L_Pin, BTN_L_GPIO_Port, APP_BUTTON_L },
  { BTN_R_Pin, BTN_R_GPIO_Port, APP_BUTTON_R },
  { BTN_BOOT_Pin, BTN_BOOT_GPIO_Port, APP_BUTTON_BOOT },
};

void rtos_isr_bridge_set_nonwake_buttons_enabled(uint8_t enable)
{
  uint16_t pins = (uint16_t)(BTN_A_Pin | BTN_B_Pin | BTN_BOOT_Pin);

  if (enable != 0U)
  {
    SET_BIT(EXTI->IMR1, pins);
    SET_BIT(EXTI->EMR1, pins);
    __HAL_GPIO_EXTI_CLEAR_IT(pins);
    NVIC_ClearPendingIRQ(EXTI1_IRQn);
    NVIC_ClearPendingIRQ(EXTI2_IRQn);
    NVIC_ClearPendingIRQ(EXTI3_IRQn);
    HAL_NVIC_EnableIRQ(EXTI1_IRQn);
    HAL_NVIC_EnableIRQ(EXTI2_IRQn);
    HAL_NVIC_EnableIRQ(EXTI3_IRQn);
    return;
  }

  HAL_NVIC_DisableIRQ(EXTI1_IRQn);
  HAL_NVIC_DisableIRQ(EXTI2_IRQn);
  HAL_NVIC_DisableIRQ(EXTI3_IRQn);
  CLEAR_BIT(EXTI->IMR1, pins);
  CLEAR_BIT(EXTI->EMR1, pins);
  __HAL_GPIO_EXTI_CLEAR_IT(pins);
  NVIC_ClearPendingIRQ(EXTI1_IRQn);
  NVIC_ClearPendingIRQ(EXTI2_IRQn);
  NVIC_ClearPendingIRQ(EXTI3_IRQn);
}

void rtos_isr_bridge_handle_exti(uint16_t gpio_pin)
{
  g_exti_callback_count++;
  g_exti_last_pin = gpio_pin;
  g_exti_last_kernel_state = (uint32_t)osKernelGetState();
  g_exti_last_flags = 0U;

  for (uint32_t i = 0U; i < (uint32_t)(sizeof(kButtonMap) / sizeof(kButtonMap[0])); ++i)
  {
    if (kButtonMap[i].pin == gpio_pin)
    {
      app_input_event_t evt;
      evt.button_id = (uint8_t)kButtonMap[i].id;
      evt.pressed = (uint8_t)((HAL_GPIO_ReadPin(kButtonMap[i].port, kButtonMap[i].pin) == GPIO_PIN_SET) ? 1U : 0U);
      evt.reserved = 0U;

      g_exti_last_event_id = evt.button_id;
      g_exti_last_event_pressed = evt.pressed;
      g_exti_last_flags = (1UL << evt.button_id);

      g_exti_qput_attempts++;
      if (qInputHandle != NULL)
      {
        g_exti_last_qput_result = (int32_t)osMessageQueuePut(qInputHandle, &evt, 0U, 0U);
      }
      else
      {
        g_exti_last_qput_result = (int32_t)osErrorResource;
      }

      if (g_exti_last_qput_result != (int32_t)osOK)
      {
        g_exti_qput_errors++;
      }
      return;
    }
  }
}
