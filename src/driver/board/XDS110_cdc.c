/*
 *  ======== XDS110_cdc.c ========
 *  XDS110 CDC-UART backchannel driver  (UART0: PA10 TX, PA11 RX, 115200-8N1).
 *
 *  TX: DMA channel 0, triggered by UART0 TXIFG.  Non-blocking;
 *      FreeRTOS binary semaphore signals completion from ISR.
 *  RX: polling (simple, low-overhead for debug UART).
 *
 *  SysConfig UART DMA setup (empty.syscfg):
 *    UART1.enableDMATX          = true
 *    UART1.enabledDMATXTriggers = "DL_UART_DMA_INTERRUPT_TX"
 *    UART1.DMA_CHANNEL_TX.peripheral.$assign = "DMA_CH0"
 *    → SysConfig generates DMA_CH0 with trigger = UART_0_INST_DMA_TRIGGER
 *      (= DMA_UART0_TX_TRIG) and triggerType = EXTERNAL.
 */

#include "include/XDS110_cdc.h"
#include "ti_msp_dl_config.h"

#include <FreeRTOS.h>
#include <semphr.h>

#include <string.h>
#include <stdbool.h>

/* DMA channel — SysConfig-generated constant */
#define TX_CHAN_ID      DMA_CH0_CHAN_ID    /* 0 */

/* TX buffer for DMA (persistent — DMA reads from it asynchronously) */
#define TX_BUF_SIZE     128
static uint8_t  g_tx_buf[TX_BUF_SIZE];
static volatile bool g_tx_busy = false;
static SemaphoreHandle_t g_tx_done_sem = NULL;

/* ---- DMA interrupt handler ---- */
void DMA_IRQHandler(void)
{
    switch (DL_DMA_getPendingInterrupt(DMA)) {
        case DL_DMA_EVENT_IIDX_DMACH0:
            DL_DMA_clearInterruptStatus(DMA, DL_DMA_INTERRUPT_CHANNEL0);
            DL_DMA_disableChannel(DMA, TX_CHAN_ID);
            DL_UART_disableDMATransmitEvent(UART_0_INST);
            g_tx_busy = false;
            if (g_tx_done_sem != NULL) {
                BaseType_t xHigherPriorityTaskWoken = pdFALSE;
                xSemaphoreGiveFromISR(g_tx_done_sem, &xHigherPriorityTaskWoken);
                portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
            }
            break;
        default:
            break;
    }
}

/* ---- public API ---- */

void uart_init(void)
{
    SYSCFG_DL_UART_0_init();
    SYSCFG_DL_DMA_init();

    /* Enable DMA channel-0 interrupt in the DMA peripheral itself.
       NVIC_EnableIRQ gates the ISR call; DL_DMA_enableInterrupt gates
       whether the peripheral ever raises the interrupt at all.  Both needed. */
    DL_DMA_enableInterrupt(DMA, DL_DMA_INTERRUPT_CHANNEL0);
    NVIC_EnableIRQ(DMA_INT_IRQn);

    g_tx_done_sem = xSemaphoreCreateBinary();
}

/*
 * ---- Non-blocking DMA TX ----
 */

BaseType_t uart_send_async(const uint8_t *data, size_t len, TickType_t timeout)
{
    if (len == 0 || len > TX_BUF_SIZE) { return pdFALSE; }

    /* Wait for previous transfer (block on semaphore) */
    if (g_tx_busy) {
        if (xSemaphoreTake(g_tx_done_sem, timeout) != pdTRUE) {
            return pdFALSE;
        }
    }

    /* Copy to DMA-safe buffer */
    memcpy(g_tx_buf, data, len);

    /* Arm DMA */
    g_tx_busy = true;
    DL_DMA_setSrcAddr(DMA, TX_CHAN_ID, (uint32_t) g_tx_buf);
    DL_DMA_setDestAddr(DMA, TX_CHAN_ID, (uint32_t) &UART_0_INST->TXDATA);
    DL_DMA_setTransferSize(DMA, TX_CHAN_ID, (uint16_t) len);
    DL_DMA_enableChannel(DMA, TX_CHAN_ID);
    DL_UART_enableDMATransmitEvent(UART_0_INST);

    return pdTRUE;
}

/*
 * ---- RX (polling) ----
 */

uint8_t uart_recv_byte(void)
{
    return DL_UART_receiveDataBlocking(UART_0_INST);
}

bool uart_rx_ready(void)
{
    return !DL_UART_isRXFIFOEmpty(UART_0_INST);
}
