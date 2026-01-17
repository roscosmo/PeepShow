#include "debug_uart.h"
#include "main.h"   // <- correct include (brings HAL + hlpuart1)
#include <stdarg.h>
#include <stdio.h>

#define DEBUG_UART_BUF_SIZE 128U

extern UART_HandleTypeDef huart1;

static volatile uint8_t g_debug_uart_busy = 0U;
static uint8_t g_debug_uart_buf[DEBUG_UART_BUF_SIZE];

void debug_uart_tx_done(void)
{
#if DEBUGGING
  g_debug_uart_busy = 0U;
#endif
}

void debug_uart_log(const char *msg)
{
#if DEBUGGING
  if (msg == NULL)
  {
    return;
  }

  debug_uart_printf("%s", msg);
#else
  (void)msg;
#endif
}

void debug_uart_printf(const char *fmt, ...)
{
#if DEBUGGING
  if (fmt == NULL)
  {
    return;
  }

  if (osKernelGetState() != osKernelRunning)
  {
    return;
  }

  uint8_t locked = 0U;
  if (mtxLogHandle != NULL)
  {
    if (osMutexAcquire(mtxLogHandle, 0U) != osOK)
    {
      return;
    }
    locked = 1U;
  }

  if (g_debug_uart_busy != 0U)
  {
    if (locked != 0U)
    {
      (void)osMutexRelease(mtxLogHandle);
    }
    return;
  }

  g_debug_uart_busy = 1U;

  va_list args;
  va_start(args, fmt);
  int32_t len = (int32_t)vsnprintf((char *)g_debug_uart_buf, (int)DEBUG_UART_BUF_SIZE, fmt, args);
  va_end(args);

  if (len <= 0)
  {
    g_debug_uart_busy = 0U;
    if (locked != 0U)
    {
      (void)osMutexRelease(mtxLogHandle);
    }
    return;
  }

  if (len >= (int32_t)DEBUG_UART_BUF_SIZE)
  {
    len = (int32_t)DEBUG_UART_BUF_SIZE - 1;
    g_debug_uart_buf[len] = '\0';
  }

  if (HAL_UART_Transmit_DMA(&huart1, g_debug_uart_buf, (uint16_t)len) != HAL_OK)
  {
    g_debug_uart_busy = 0U;
    if (locked != 0U)
    {
      (void)osMutexRelease(mtxLogHandle);
    }
    return;
  }

  if (locked != 0U)
  {
    (void)osMutexRelease(mtxLogHandle);
  }
#else
  (void)fmt;
#endif
}

extern UART_HandleTypeDef huart1;  // handle is defined in usart.c if you enabled LPUART1
int __io_putchar(int ch) {
    uint8_t c = (uint8_t)ch;
    if (c == '\n') { uint8_t cr = '\r'; HAL_UART_Transmit(&huart1, &cr, 1, 1000); }
    HAL_UART_Transmit(&huart1, &c, 1, 1000);
    return ch;
}

