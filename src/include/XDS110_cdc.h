/*
 *  ======== XDS110_cdc.h ========
 *  XDS110 CDC-UART backchannel driver  (UART0: PA10 TX, PA11 RX, 115200-8N1).
 */

#ifndef XDS110_CDC_H
#define XDS110_CDC_H

#include <stdbool.h>
#include <stdint.h>

/// One-time UART init (calls SysConfig-generated init).
void uart_init(void);

/// Send a NUL-terminated string (blocking).
void uart_send(const char *str);

/// Send a single byte (blocking).
void uart_send_byte(uint8_t b);

/// Receive a single byte (blocking).
uint8_t uart_recv_byte(void);

/// Non-blocking check: true if at least one byte is available.
bool uart_rx_ready(void);

#endif /* XDS110_CDC_H */
