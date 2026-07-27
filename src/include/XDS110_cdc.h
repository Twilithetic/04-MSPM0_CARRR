/*
 *  ======== XDS110_cdc.h ========
 *  XDS110 CDC-UART backchannel driver  (UART0: PA10 TX, PA11 RX, 115200-8N1).
 *  TX: DMA non-blocking.  RX: polling.
 */

#ifndef XDS110_CDC_H
#define XDS110_CDC_H

#include <FreeRTOS.h>

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/* TX buffer size — shared between XDS110_cdc.c and callers */
#define UART_TX_BUF_SIZE  256

void uart_init(void);

/// Non-blocking DMA TX.  Returns immediately; DMA sends in background.
/// If previous transfer is still in flight, waits no longer than `timeout`.
/// Pass 0 for timeout to skip (returns pdFALSE) if DMA is busy.
BaseType_t uart_send_async(const uint8_t *data, size_t len, TickType_t timeout);

uint8_t uart_recv_byte(void);
bool uart_rx_ready(void);

#endif /* XDS110_CDC_H */
