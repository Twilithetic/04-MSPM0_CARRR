/*
 *  ======== XDS110_cdc.h ========
 *  XDS110 CDC-UART backchannel driver  (UART0: PA10 TX, PA11 RX, 115200-8N1).
 *  Polling mode — simple, no DMA/ISR.
 */

#ifndef XDS110_CDC_H
#define XDS110_CDC_H

#include <FreeRTOS.h>

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

void uart_init(void);
void uart_init_post_scheduler(void);

/// Send a NUL-terminated string (polling, blocking).
void uart_send(const char *str);

/// Send a single byte (polling, blocking).
void uart_send_byte(uint8_t b);

/// Async-compatible wrapper (same signature as DMA version, but polling).
BaseType_t uart_send_async(const uint8_t *data, size_t len, TickType_t timeout);

/// No-op in polling mode.
void uart_send_flush(TickType_t timeout);

uint8_t uart_recv_byte(void);
bool uart_rx_ready(void);

#endif /* XDS110_CDC_H */
