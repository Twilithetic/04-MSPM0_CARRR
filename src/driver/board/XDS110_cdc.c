/*
 *  ======== XDS110_cdc.c ========
 *  XDS110 CDC-UART backchannel driver  (UART0: PA10 TX, PA11 RX).
 *  UART init is handled by SysConfig via empty.syscfg.
 *  Macros come from ti_msp_dl_config.h, not hard-coded here.
 */

#include "include/XDS110_cdc.h"
#include "ti_msp_dl_config.h"

/* ---- local helpers ---- */

static void uart_putc(char c)
{
    /* Wait until TX FIFO has room, then push */
    while (DL_UART_isTXFIFOFull(UART_0_INST)) {}
    DL_UART_transmitDataBlocking(UART_0_INST, (uint8_t) c);
}

/* ---- public API ---- */

void uart_init(void)
{
    SYSCFG_DL_UART_0_init();
}

void uart_send(const char *str)
{
    while (*str) {
        uart_putc(*str++);
    }
}

void uart_send_byte(uint8_t b)
{
    while (DL_UART_isTXFIFOFull(UART_0_INST)) {}
    DL_UART_transmitDataBlocking(UART_0_INST, b);
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
