/*
 *  ======== line8_driver_uart.c ========
 *  8-Channel IR Line Sensor Proxy — UART3 command/response.
 *
 *  Hardware:
 *    UART3: PA26 TX, PA25 RX @ 9600 8N1
 *
 *  Implementation: pure DriverLib + FreeRTOS queue (matching
 *  motor_driver_uart.c pattern).
 *
 *  Protocol (vendor sensor protocol):
 *    Host → Sensor (command):
 *      $0,0,1#        — digital mode  (0/1 per channel, simpler)
 *      $0,1,0#        — analog mode   (0-4095 per channel)
 *      $1,0,0#        — calibration
 *
 *    Sensor → Host (data, 10ms periodic after command):
 *      $D,x1:0,x2:1,x3:0,x4:0,x5:1,x6:0,x7:1,x8:0#
 *      $A,x1:1000,x2:3450,x3:40,x4:450,x5:110,x6:4096,x7:780,x8:80#
 *
 *    Frame format:
 *      Start: '$'   End: '#'    No CR/LF
 *
 *  Digital values: 0 = black (on line), 1 = white (background/off line)
 *  Analog values:  0-4095, lower = darker
 *
 *  Channel mapping: x1=leftmost, x8=rightmost
 */

#include "include/line8_reg.h"

#include <ti/driverlib/driverlib.h>
#include <ti/devices/msp/msp.h>

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

/* ====================================================================
 *  UART3 hardware — PA26 TX / PA25 RX @ 9600 8N1
 *
 *  PA26 = IOMUX_PINCM59, PA25 = IOMUX_PINCM55
 *  UART3 IOMUX functions:
 *    IOMUX_PINCM59_PF_UART3_TX  (PA26 → TX)
 *    IOMUX_PINCM55_PF_UART3_RX  (PA25 → RX)
 * ==================================================================== */

#define LINE8_UART_INST          UART3
#define LINE8_UART_IOMUX_RX      (IOMUX_PINCM55)
#define LINE8_UART_IOMUX_RX_FUNC IOMUX_PINCM55_PF_UART3_RX
#define LINE8_UART_IOMUX_TX      (IOMUX_PINCM59)
#define LINE8_UART_IOMUX_TX_FUNC IOMUX_PINCM59_PF_UART3_TX

/* ====================================================================
 *  RX — interrupt-driven byte queue
 * ==================================================================== */

#define RX_QUEUE_SIZE  256
static QueueHandle_t g_line8_rx_queue = NULL;

void UART3_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    switch (DL_UART_Main_getPendingInterrupt(LINE8_UART_INST)) {
    case DL_UART_IIDX_RX: {
        uint8_t byte = DL_UART_Main_receiveData(LINE8_UART_INST);
        xQueueSendFromISR(g_line8_rx_queue, &byte, &xHigherPriorityTaskWoken);
        break;
    }
    default:
        break;
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/* ---- Init UART3 (pure DriverLib, matching motor_driver_uart.c pattern) ---- */

static bool g_line8_uart_ready = false;

static bool line8_uart_init(void)
{
    if (g_line8_uart_ready) return true;

    g_line8_rx_queue = xQueueCreate(RX_QUEUE_SIZE, sizeof(uint8_t));
    if (g_line8_rx_queue == NULL) return false;

    /* Power + reset */
    DL_UART_Main_reset(LINE8_UART_INST);
    DL_UART_Main_enablePower(LINE8_UART_INST);

    /* IOMUX */
    DL_GPIO_initPeripheralOutputFunction(LINE8_UART_IOMUX_TX, LINE8_UART_IOMUX_TX_FUNC);
    DL_GPIO_initPeripheralInputFunction(LINE8_UART_IOMUX_RX, LINE8_UART_IOMUX_RX_FUNC);

    /* Clock: BUSCLK / 1 = 32MHz */
    DL_UART_Main_ClockConfig clk = {
        .clockSel    = DL_UART_MAIN_CLOCK_BUSCLK,
        .divideRatio = DL_UART_MAIN_CLOCK_DIVIDE_RATIO_1,
    };
    DL_UART_Main_setClockConfig(LINE8_UART_INST, &clk);

    /* UART: 8N1 */
    DL_UART_Main_Config cfg = {
        .mode        = DL_UART_MAIN_MODE_NORMAL,
        .direction   = DL_UART_MAIN_DIRECTION_TX_RX,
        .flowControl = DL_UART_MAIN_FLOW_CONTROL_NONE,
        .parity      = DL_UART_MAIN_PARITY_NONE,
        .wordLength  = DL_UART_MAIN_WORD_LENGTH_8_BITS,
        .stopBits    = DL_UART_MAIN_STOP_BITS_ONE,
    };
    DL_UART_Main_init(LINE8_UART_INST, &cfg);

    /* Baud: 9600 @ 32MHz */
    DL_UART_Main_setOversampling(LINE8_UART_INST, DL_UART_OVERSAMPLING_RATE_16X);
    DL_UART_Main_setBaudRateDivisor(LINE8_UART_INST, 208, 21);

    /* FIFOs */
    DL_UART_Main_enableFIFOs(LINE8_UART_INST);
    DL_UART_Main_setRXFIFOThreshold(LINE8_UART_INST, DL_UART_RX_FIFO_LEVEL_ONE_ENTRY);
    DL_UART_Main_setTXFIFOThreshold(LINE8_UART_INST, DL_UART_TX_FIFO_LEVEL_EMPTY);

    /* RX interrupt */
    DL_UART_Main_enableInterrupt(LINE8_UART_INST, DL_UART_INTERRUPT_RX);
    NVIC_EnableIRQ(UART3_INT_IRQn);

    /* Go */
    DL_UART_Main_enable(LINE8_UART_INST);

    g_line8_uart_ready = true;
    return true;
}

/* ====================================================================
 *  TX — poll FIFO
 * ==================================================================== */

static void line8_uart_send(const char *s)
{
    if (!s || !*s) return;
    while (*s) {
        while (DL_UART_Main_isBusy(LINE8_UART_INST)) {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
        DL_UART_Main_transmitData(LINE8_UART_INST, (uint8_t)*s++);
    }
}

/* ====================================================================
 *  RX — byte queue + '#' frame decoder
 * ==================================================================== */

/* Read one byte with timeout. Returns <0 on timeout. */
static int line8_uart_getc(TickType_t timeout_ticks)
{
    uint8_t byte;
    if (xQueueReceive(g_line8_rx_queue, &byte, timeout_ticks) == pdTRUE) {
        return byte;
    }
    return -1;
}

/* Drain any stale bytes from the queue. */
static void line8_uart_drain(void)
{
    uint8_t junk;
    while (xQueueReceive(g_line8_rx_queue, &junk, 0) == pdTRUE) {}
}

/*
 *  Read a '#'-terminated frame.
 *  Starts on '$', ends on '#'.
 */
#define LINE8_RESP_BUF_SIZE 128
static char g_line8_rx_line[LINE8_RESP_BUF_SIZE];

static const char *line8_uart_recv_frame(TickType_t timeout_ms)
{
    /* 1. Wait for '$' start byte */
    for (;;) {
        int c = line8_uart_getc(timeout_ms);
        if (c < 0) { g_line8_rx_line[0] = '\0'; return g_line8_rx_line; }
        if (c == '$') break;
    }

    /* 2. Read until '#' */
    size_t idx = 0;
    while (idx < sizeof(g_line8_rx_line) - 1) {
        int c = line8_uart_getc(timeout_ms);
        if (c < 0) break;
        if (c == '#') break;
        g_line8_rx_line[idx++] = (char)c;
    }
    g_line8_rx_line[idx] = '\0';
    return g_line8_rx_line;
}

/* ====================================================================
 *  Command helper
 * ==================================================================== */

static void line8_send_cmd_nowait(const char *cmd, unsigned long delay_ms)
{
    if (!line8_uart_init()) return;
    line8_uart_send(cmd);
    if (delay_ms) vTaskDelay(pdMS_TO_TICKS(delay_ms));
}

/* ====================================================================
 *  Frame parser
 * ==================================================================== */

/*
 *  Parse a digital-data frame:
 *    $D,x1:0,x2:1,x3:0,x4:0,x5:1,x6:0,x7:1,x8:0#
 *  After stripping '$' and '#', line looks like:
 *    D,x1:0,x2:1,x3:0,x4:0,x5:1,x6:0,x7:1,x8:0
 *
 *  Each data point is "xN:V" where V is 0 or 1.
 *  Returns true if a valid digital frame was parsed.
 */
static bool parse_digital_frame(const char *line, Line8Reg *r)
{
    /*
     *  Format: D,x1:V,x2:V,x3:V,x4:V,x5:V,x6:V,x7:V,x8:V
     *
     *  Simple approach: the values are at fixed offsets from ':' characters.
     *  For a "xN:V" pattern, the value byte is at &line[3 + N*5] since each
     *  "xN:V" is 5 bytes (e.g. "x1:0," or "x8:1" at end).
     *
     *  Safer approach: scan for ':' then read the character after it.
     */
    if (line[0] != 'D' || line[1] != ',') return false;

    const char *p = line + 2;  /* skip "D," */
    uint8_t i;
    for (i = 0; i < LINE8_CHANNELS; i++) {
        /* find "xN:" */
        p = strchr(p, ':');
        if (!p) break;
        p++;  /* now at the value digit */
        uint8_t val = (uint8_t)(*p - '0');
        line8_set_raw(i, val);
        p++;
    }

    if (i < LINE8_CHANNELS) return false;

    /* recompute position, error, mask */
    line8_compute_position();
    return true;
}

/*
 *  Parse an analog-data frame:
 *    $A,x1:1000,x2:3450,...,x8:80#
 *  Each data point is "xN:V" where V is 0-4095 (1-4 digits).
 *  Returns true if a valid analog frame was parsed.
 */
static bool parse_analog_frame(const char *line, Line8Reg *r)
{
    /*
     *  Format: A,x1:V,x2:V,...,x8:V
     *  V can be 0-4095 (1-4 digits).
     *
     *  Use sscanf for each value.  We'll walk the string finding "xN:" pairs.
     */
    if (line[0] != 'A' || line[1] != ',') return false;

    uint8_t  raw_vals[LINE8_CHANNELS] = {0};
    uint16_t analog_vals[LINE8_CHANNELS] = {0};
    bool     got_analog = false;

    /* Try to parse all 8 analog values: "x1:%hu,x2:%hu,..." */
    int matched = sscanf(line,
        "A,x1:%hu,x2:%hu,x3:%hu,x4:%hu,x5:%hu,x6:%hu,x7:%hu,x8:%hu",
        &analog_vals[0], &analog_vals[1], &analog_vals[2], &analog_vals[3],
        &analog_vals[4], &analog_vals[5], &analog_vals[6], &analog_vals[7]);

    if (matched == LINE8_CHANNELS) {
        got_analog = true;
        uint8_t i;
        for (i = 0; i < LINE8_CHANNELS; i++) {
            line8_set_analog(i, analog_vals[i]);
            /* threshold at 2000 for digital derivation from analog */
            raw_vals[i] = (analog_vals[i] > 2000) ? 1 : 0;
            line8_set_raw(i, raw_vals[i]);
        }
        line8_compute_position();
    }

    return got_analog;
}

/* ====================================================================
 *  Public API
 * ==================================================================== */

/*
 *  Initialize the line sensor.
 *  Sends digital-mode command and waits for first valid response.
 *  Returns true on success.
 */
bool line8_driver_init(void)
{
    if (!line8_uart_init()) return false;

    /* Send digital mode command: $0,0,1# */
    line8_send_cmd_nowait("$0,0,1#", 100);

    /* Wait for first response (up to 500ms) */
    const char *resp = line8_uart_recv_frame(pdMS_TO_TICKS(500));
    if (!resp || !*resp) {
        line8_set_comm_status(0x01);
        return false;
    }

    /* Parse first response to confirm comms */
    if (resp[0] == 'D') {
        parse_digital_frame(resp, &g_line8_reg);
        line8_set_mode(0);
    } else if (resp[0] == 'A') {
        parse_analog_frame(resp, &g_line8_reg);
        line8_set_mode(1);
    } else {
        line8_set_comm_status(0x02);
        return false;
    }

    line8_set_comm_status(0);
    line8_set_initialized(true);
    return true;
}

/*
 *  Send calibration command: $1,0,0#
 */
void line8_send_calibrate(void)
{
    line8_send_cmd_nowait("$1,0,0#", 200);
}

/*
 *  switch to digital mode: $0,0,1#
 */
void line8_set_digital_mode(void)
{
    line8_send_cmd_nowait("$0,0,1#", 50);
    line8_set_mode(0);
}

/*
 *  switch to analog mode: $0,1,0#
 */
void line8_set_analog_mode(void)
{
    line8_send_cmd_nowait("$0,1,0#", 50);
    line8_set_mode(1);
}

/* ====================================================================
 *  SYNC: read UART3 → parse frame → write shadow register
 *
 *  Called periodically (e.g. @ 100 Hz) from line sync task.
 *  Each call drains any pending frames from the RX queue.
 *  The sensor sends data every ~10ms after the mode command.
 * ==================================================================== */

void sync_line8_from_device(Line8Reg *r)
{
    (void) r;
    bool got_data = false;
    uint8_t n_frames = 0;

    /* Drain up to 5 frames per call (safety cap, sensor sends at most ~1/10ms) */
    int i;
    for (i = 0; i < 5; i++) {
        const char *resp = line8_uart_recv_frame(pdMS_TO_TICKS(15));  /* timeout: 1.5× frame period */
        if (!resp || !*resp) break;

        if (resp[0] == 'D') {
            if (parse_digital_frame(resp, &g_line8_reg)) {
                got_data = true;
                n_frames++;
            }
        } else if (resp[0] == 'A') {
            if (parse_analog_frame(resp, &g_line8_reg)) {
                got_data = true;
                n_frames++;
            }
        }
        /* ignore unrecognized frames */
    }

    line8_set_comm_status(got_data ? 0 : 0xFE);
    if (n_frames > 0) {
        line8_inc_frame_count();
    }
}
