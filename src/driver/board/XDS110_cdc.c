/*
 *  ======== XDS110_cdc.c ========
 *  XDS110 CDC-UART backchannel driver  (UART0: PA10 TX, PA11 RX, 115200-8N1).
 *
 *  POLLING ONLY — no DMA, no ISR.  Simplest possible UART to verify HW works.
 *  Will add DMA back once this baseline is confirmed.
 */

#include "include/XDS110_cdc.h"
#include "ti_msp_dl_config.h"

#include <string.h>

/* ---- public API ---- */

void uart_init(void)
{
    SYSCFG_DL_UART_0_init();
}

void uart_init_post_scheduler(void)
{
    /* no-op in polling mode */
}

void uart_send(const char *str)
{
    while (*str) {
        while (DL_UART_isTXFIFOFull(UART_0_INST)) {}
        DL_UART_transmitDataBlocking(UART_0_INST, (uint8_t) *str++);
    }
}

void uart_send_byte(uint8_t b)
{
    while (DL_UART_isTXFIFOFull(UART_0_INST)) {}
    DL_UART_transmitDataBlocking(UART_0_INST, b);
}

BaseType_t uart_send_async(const uint8_t *data, size_t len, TickType_t timeout)
{
    (void) timeout;
    for (size_t i = 0; i < len; i++) {
        uart_send_byte(data[i]);
    }
    return pdTRUE;
}

void uart_send_flush(TickType_t timeout)
{
    (void) timeout;
    /* nothing to flush — polling is synchronous */
}

uint8_t uart_recv_byte(void)
{
    while (DL_UART_isRXFIFOEmpty(UART_0_INST)) {}
    return DL_UART_receiveDataBlocking(UART_0_INST);
}

bool uart_rx_ready(void)
{
    return !DL_UART_isRXFIFOEmpty(UART_0_INST);
}
