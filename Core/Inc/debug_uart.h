#ifndef DEBUG_UART_H
#define DEBUG_UART_H

#ifdef __cplusplus
extern "C" {
#endif

#include "app_freertos.h"

#if DEBUGGING
void debug_uart_printf(const char *fmt, ...);
void debug_uart_log(const char *msg);
void debug_uart_tx_done(void);
#else
#define debug_uart_printf(...) ((void)0)
#define debug_uart_log(...) ((void)0)
#define debug_uart_tx_done() ((void)0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* DEBUG_UART_H */
