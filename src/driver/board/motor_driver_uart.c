/*
 *  ======== motor_driver_uart.c ========
 *  4-Way Motor Driver Board Proxy — UART command/response.
 *
 *  The driver board has built-in PID.  Application sets PID params + target
 *  speed; the board controls motors closed-loop.  Direct PWM bypass is also
 *  available for testing.
 *
 *  Hardware:
 *    UART1: PA8 TX, PA9 RX @ 115200 8N1
 *
 *  Implementation: pure DriverLib + FreeRTOS queue (matching vendor's
 *  reference code in docs/4路电机驱动板/CarMove_USART/BSP/).
 *
 *  Protocol:
 *    Commands:  $cmd:args#         (no CR/LF — '#' is the terminator)
 *    Responses: $KEY:data#         (terminated by '#')
 *
 *    Board PID:
 *      $mpid:P,I,D#               — set PID parameters on board
 *      $spd:M1,M2,M3,M4#          — target speed (encoder counts/10ms)
 *
 *    Direct PWM:
 *      $pwm:M1,M2,M3,M4#          — raw PWM (-7200 ~ +7200)
 *
 *    Config:
 *      $mtype:N#  $deadzone:N#  $mline:N#  $mphase:N#  $wdiameter:F#
 *
 *    Upload (periodic responses):
 *      $upload:1,1,1#  →  $MAll:, $MTEP:, $MSPD: every 10ms
 *
 *    Query:
 *      $read_vol#       →  $Battery:X.XXV#
 *
 *  Motor mapping:
 *    M1=LF  M2=LEFT  M3=RF  M4=RIGHT
 */

#include "include/motor_driver_uart.h"
#include "ti_msp_dl_config.h"

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

/* ====================================================================
 *  UART1 hardware — PA8 TX / PA9 RX
 * ==================================================================== */

#define UART_1_INST          UART1
#define UART_1_IOMUX_RX      (IOMUX_PINCM20)
#define UART_1_IOMUX_RX_FUNC IOMUX_PINCM20_PF_UART1_RX
#define UART_1_IOMUX_TX      (IOMUX_PINCM19)
#define UART_1_IOMUX_TX_FUNC IOMUX_PINCM19_PF_UART1_TX

/* ====================================================================
 *  RX — interrupt-driven byte queue (matching vendor's ISR pattern)
 * ==================================================================== */

#define RX_QUEUE_SIZE  256
static QueueHandle_t g_rx_queue = NULL;

void UART1_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    switch (DL_UART_Main_getPendingInterrupt(UART_1_INST)) {
    case DL_UART_IIDX_RX: {
        uint8_t byte = DL_UART_Main_receiveData(UART_1_INST);
        xQueueSendFromISR(g_rx_queue, &byte, &xHigherPriorityTaskWoken);
        break;
    }
    default:
        break;
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/* ---- Init UART1 (pure DriverLib, matching vendor BSP) ---- */

static bool g_uart_ready = false;

static bool motor_uart_init(void)
{
    if (g_uart_ready) return true;

    g_rx_queue = xQueueCreate(RX_QUEUE_SIZE, sizeof(uint8_t));
    if (g_rx_queue == NULL) return false;

    /* Power + reset */
    DL_UART_Main_reset(UART_1_INST);
    DL_UART_Main_enablePower(UART_1_INST);

    /* IOMUX */
    DL_GPIO_initPeripheralOutputFunction(UART_1_IOMUX_TX, UART_1_IOMUX_TX_FUNC);
    DL_GPIO_initPeripheralInputFunction(UART_1_IOMUX_RX, UART_1_IOMUX_RX_FUNC);

    /* Clock: BUSCLK / 1 = 32MHz */
    DL_UART_Main_ClockConfig clk = {
        .clockSel    = DL_UART_MAIN_CLOCK_BUSCLK,
        .divideRatio = DL_UART_MAIN_CLOCK_DIVIDE_RATIO_1,
    };
    DL_UART_Main_setClockConfig(UART_1_INST, &clk);

    /* UART: 8N1 */
    DL_UART_Main_Config cfg = {
        .mode        = DL_UART_MAIN_MODE_NORMAL,
        .direction   = DL_UART_MAIN_DIRECTION_TX_RX,
        .flowControl = DL_UART_MAIN_FLOW_CONTROL_NONE,
        .parity      = DL_UART_MAIN_PARITY_NONE,
        .wordLength  = DL_UART_MAIN_WORD_LENGTH_8_BITS,
        .stopBits    = DL_UART_MAIN_STOP_BITS_ONE,
    };
    DL_UART_Main_init(UART_1_INST, &cfg);

    /* Baud: 115200 @ 32MHz */
    DL_UART_Main_setOversampling(UART_1_INST, DL_UART_OVERSAMPLING_RATE_16X);
    DL_UART_Main_setBaudRateDivisor(UART_1_INST, 17, 23);

    /* FIFOs */
    DL_UART_Main_enableFIFOs(UART_1_INST);
    DL_UART_Main_setRXFIFOThreshold(UART_1_INST, DL_UART_RX_FIFO_LEVEL_ONE_ENTRY);
    DL_UART_Main_setTXFIFOThreshold(UART_1_INST, DL_UART_TX_FIFO_LEVEL_EMPTY);

    /* RX interrupt */
    DL_UART_Main_enableInterrupt(UART_1_INST, DL_UART_INTERRUPT_RX);
    NVIC_EnableIRQ(UART1_INT_IRQn);

    /* Go */
    DL_UART_Main_enable(UART_1_INST);

    g_uart_ready = true;
    return true;
}

/* ====================================================================
 *  TX — poll FIFO, matching vendor's Send_Motor_ArrayU8 pattern
 * ==================================================================== */

static void motor_uart_send(const char *s)
{
    if (!s || !*s) return;
    while (*s) {
        while (DL_UART_Main_isBusy(UART_1_INST)) {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
        DL_UART_Main_transmitData(UART_1_INST, (uint8_t)*s++);
    }
}

/* ====================================================================
 *  RX — byte queue + '#' frame decoder, matching vendor pattern
 * ==================================================================== */

/* Read one byte with timeout. Returns <0 on timeout. */
static int motor_uart_getc(TickType_t timeout_ticks)
{
    uint8_t byte;
    if (xQueueReceive(g_rx_queue, &byte, timeout_ticks) == pdTRUE) {
        return byte;
    }
    return -1;
}

/* Drain any stale bytes from the queue. */
static void motor_uart_drain(void)
{
    uint8_t junk;
    while (xQueueReceive(g_rx_queue, &junk, 0) == pdTRUE) {}
}

/*
 *  Read a '#'-terminated frame.
 *  Matching vendor's Deal_Control_Rxtemp: starts on '$', ends on '#'.
 *  Returns pointer to static buffer (g_motor_rx_line), or empty string on
 *  timeout.
 */
#define RESP_BUF_SIZE 128
static char g_motor_rx_line[RESP_BUF_SIZE];

static const char *motor_uart_recv_frame(TickType_t timeout_ms)
{
    /* 1. Wait for '$' start byte */
    for (;;) {
        int c = motor_uart_getc(timeout_ms);
        if (c < 0) { g_motor_rx_line[0] = '\0'; return g_motor_rx_line; }
        if (c == '$') break;
    }

    /* 2. Read until '#' */
    size_t idx = 0;
    while (idx < sizeof(g_motor_rx_line) - 1) {
        int c = motor_uart_getc(timeout_ms);
        if (c < 0) break;        /* timeout */
        if (c == '#') break;     /* frame end */
        g_motor_rx_line[idx++] = (char)c;
    }
    g_motor_rx_line[idx] = '\0';
    return g_motor_rx_line;
}

/* ====================================================================
 *  Command helper
 * ==================================================================== */

/*
 *  Send a command and read back a '#'-delimited response frame.
 *  Returns pointer to response payload (without '$' / '#' delimiters),
 *  or NULL/empty string on timeout.
 *
 *  NOTE: Most config commands ($mtype:, $spd:, etc.) do NOT produce a
 *  response.  Only call this for query commands that return data.
 */
static const char *motor_send_cmd(const char *cmd, unsigned long timeout_ms)
{
    if (!motor_uart_init()) return NULL;

    motor_uart_drain();
    motor_uart_send(cmd);
    motor_uart_recv_frame(timeout_ms);
    return g_motor_rx_line;
}

/*
 *  Send a command that does NOT produce a response, then delay.
 */
static void motor_send_cmd_nowait(const char *cmd, unsigned long delay_ms)
{
    if (!motor_uart_init()) return;
    motor_uart_send(cmd);
    if (delay_ms) vTaskDelay(pdMS_TO_TICKS(delay_ms));
}

/* ====================================================================
 *  Public API
 * ==================================================================== */

bool motor_driver_init(void)
{
    if (!motor_uart_init()) return false;

    /* Stop motors */
    motor_send_cmd_nowait("$spd:0,0,0,0#", 50);
    motor_send_cmd_nowait("$pwm:0,0,0,0#", 50);

    /* Health check — this one returns a response */
    const char *resp = motor_send_cmd("$read_vol#", 200);
    if (resp && strstr(resp, "Battery")) {
        float volts = 0.0f;
        sscanf(resp, "Battery:%fV", &volts);
        motor_set_comm_status((uint8_t)(volts * 10.0f));
        return true;
    }
    return false;
}

bool cmd_config_tt_encoder(MotorDriverReg *r)
{
    (void)r;
    motor_send_cmd_nowait("$mtype:3#", 100);
    motor_send_cmd_nowait("$deadzone:1250#", 100);
    motor_send_cmd_nowait("$mline:500#", 100);
    motor_send_cmd_nowait("$mphase:30#", 100);
    motor_send_cmd_nowait("$wdiameter:67#", 100);

    /* Enable all three upload streams at once:
     *   MAll = total encoder, MTEP = 10ms delta, MSPD = speed */
    motor_send_cmd_nowait("$upload:1,1,1#", 100);

    /* Verify with read_vol */
    const char *resp = motor_send_cmd("$read_vol#", 200);
    if (!resp || !*resp) return false;

    motor_set_motor_type(3);     motor_set_pulse_line(500);
    motor_set_reduction_ratio(30); motor_set_wheel_diameter(67.0f);
    motor_set_deadzone(1250);    motor_set_comm_status(0);
    motor_set_initialized(true);
    return true;
}

/* ---- Board PID: set PID parameters on the driver board ---- */

void motor_send_pid(float kp, float ki, float kd)
{
    char buf[64];
    int n = snprintf(buf, sizeof(buf), "$mpid:%.3f,%.3f,%.3f#",
                     (double)kp, (double)ki, (double)kd);
    if (n > 0 && (size_t)n < sizeof(buf)) motor_send_cmd_nowait(buf, 0);
}

/* ---- Speed control: target speed in encoder counts/10ms ----
 *
 *  Sends $spd:M1,M2,M3,M4#.
 *  Mapping: M2=LEFT, M4=RIGHT.  M1/M3 unused (set to 0). */

void motor_send_speed(int16_t m1, int16_t m2, int16_t m3, int16_t m4)
{
    char buf[64];
    int n = snprintf(buf, sizeof(buf), "$spd:%d,%d,%d,%d#",
                     (int)m1, (int)m2, (int)m3, (int)m4);
    if (n > 0 && (size_t)n < sizeof(buf)) motor_uart_send(buf);
}

/*
 *  Speed in mm/s → encoder counts/10ms.
 *
 *  Encoder: 13 lines × 4 edges × 45 reduction = 2340 counts/wheel-rev
 *  Wheel circumference = π × 67.0 ≈ 210.5 mm
 *  counts_per_mm = 2340 / 210.5 ≈ 11.12
 *  mm/s → counts/10ms:  counts_10ms = mm_s × counts_per_mm / 100
 *                                = mm_s × 11.12 / 100
 *                                ≈ mm_s × 0.1112
 */
void motor_send_speed_mm_s(float left_mm_s, float right_mm_s)
{
    /* Conversion constant: (13*4*45) / (PI * 67.0) / 100 */
    #define COUNTS_PER_REV       (500.0f * 4.0f * 30.0f)     /* 60000 */
    #define WHEEL_CIRC_MM        (3.1415926f * 67.0f)       /* ~210.5 */
    #define MM_S_TO_COUNTS_10MS  (COUNTS_PER_REV / WHEEL_CIRC_MM / 100.0f)  /* ~0.1112 */

    int16_t l = (int16_t)(left_mm_s  * MM_S_TO_COUNTS_10MS);
    int16_t r = (int16_t)(right_mm_s * MM_S_TO_COUNTS_10MS);

    motor_send_speed(0, r, 0, l);   /* M2=RIGHT wheel, M4=LEFT wheel */

    #undef WHEEL_CIRC_MM
    #undef MM_S_TO_COUNTS_10MS
    #undef COUNTS_PER_REV
}

/* ---- Direct PWM: raw PWM bypassing board PID ----
 *
 *  Sends $pwm:M1,M2,M3,M4#.  Range: -7200 ~ +7200. */

void motor_send_pwm(int16_t m1, int16_t m2, int16_t m3, int16_t m4)
{
    char buf[64];
    int n = snprintf(buf, sizeof(buf), "$pwm:%d,%d,%d,%d#",
                     (int)m1, (int)m2, (int)m3, (int)m4);
    if (n > 0 && (size_t)n < sizeof(buf)) motor_send_cmd_nowait(buf, 50);
}

/* ---- Stop: zero-speed + zero-PWM ---- */

void motor_send_stop(void)
{
    motor_send_cmd_nowait("$spd:0,0,0,0#", 0);
    motor_send_cmd_nowait("$pwm:0,0,0,0#", 50);
}

/* ---- Battery health check ---- */

uint16_t motor_read_battery_voltage(void)
{
    const char *resp = motor_send_cmd("$read_vol#", 200);
    if (!resp || !*resp) return 0;
    float volts = 0.0f;
    if (sscanf(resp, "Battery:%fV", &volts) == 1)
        return (uint16_t)(volts * 10.0f);
    return 0;
}

/* ---- SYNC: read UART1 → write shadow register ---- */

void sync_encoder_from_device(MotorDriverReg *r)
{
    /*
     * Drain ALL pending frames.  $upload:1,1,1# (sent in cmd_config_tt_encoder)
     * causes the driver board to send three frames every 10 ms automatically:
     *   $MAll:...  $MTEP:...  $MSPD:...
     *
     * IMPORTANT: MAll is int16 (-32768..32767) and wraps around in <1 sec
     * at 60000 counts/rev.  We IGNORE it and instead ACCUMULATE MTEP (10ms
     * delta, also int16 but small per tick) into encoder_total_left/right
     * which are int32 — won't overflow for ~16 hours at 100 Hz.
     *
     * Each call reads up to 5 frames (safety cap), stopping when no more '$'
     * is immediately available.
     */
    (void)r;
    bool got_data = false;
    uint16_t n_frames = 0;

    for (int i = 0; i < 5; i++) {
        const char *resp = motor_uart_recv_frame(1); /* 1 tick timeout */
        if (!resp || !*resp) break;

        int16_t m[4] = {0};

        if (strncmp(resp, "MAll:", 5) == 0) {
            /* int16 — wraps fast, ignore; use MTEP accumulation instead */
            if (sscanf(resp + 5, "%hd,%hd,%hd,%hd",
                       &m[0], &m[1], &m[2], &m[3]) >= 4) {
                got_data = true; n_frames++;
            }
        } else if (strncmp(resp, "MTEP:", 5) == 0) {
            if (sscanf(resp + 5, "%hd,%hd,%hd,%hd",
                       &m[0], &m[1], &m[2], &m[3]) >= 4) {
                /* Accumulate 10ms delta into int32 total */
                int32_t prev_l = motor_get_encoder_left();
                int32_t prev_r = motor_get_encoder_right();
                motor_set_encoder_left(prev_l + (int32_t)m[3]);   /* M4=LEFT */
                motor_set_encoder_right(prev_r + (int32_t)m[1]);  /* M2=RIGHT */
                motor_set_encoder_10ms_left(m[3]);
                motor_set_encoder_10ms_right(m[1]);
                got_data = true; n_frames++;
            }
        } else if (strncmp(resp, "MSPD:", 5) == 0) {
            if (sscanf(resp + 5, "%hd,%hd,%hd,%hd",
                       &m[0], &m[1], &m[2], &m[3]) >= 4) {
                motor_set_speed_left(m[3]);  /* M4=LEFT */
                motor_set_speed_right(m[1]); /* M2=RIGHT */
                got_data = true; n_frames++;
            }
        }
    }

    motor_add_sync_count(n_frames);
    motor_set_comm_status(got_data ? 0 : 0xFE);
}

void sync_config_from_device(MotorDriverReg *r) { (void)r; }

void motor_uart_putchar(char c)
{
    while (DL_UART_Main_isBusy(UART_1_INST)) { vTaskDelay(pdMS_TO_TICKS(1)); }
    DL_UART_Main_transmitData(UART_1_INST, (uint8_t)c);
}

/* ====================================================================
 *  Print motor config to debug-UART (UART0, DMA non-blocking)
 * ==================================================================== */

#include "include/XDS110_cdc.h"    /* uart_send_async */

void motor_print_config(bool ok)
{
    if (ok) {
        char buf[64];
        int n = snprintf(buf, sizeof(buf),
                         "Motor init OK: type=%u enc=%u ratio=%u dia=%.1fmm dz=%u\r\n",
                         (unsigned int) motor_get_motor_type(),
                         (unsigned int) motor_get_pulse_line(),
                         (unsigned int) motor_get_reduction_ratio(),
                         (double) motor_get_wheel_diameter(),
                         (unsigned int) motor_get_deadzone());
        if (n > 0 && (size_t) n < sizeof(buf)) {
            uart_send_async((const uint8_t *) buf, (size_t) n, 0);
        }
    } else {
        char buf[48];
        int n = snprintf(buf, sizeof(buf),
                         "Motor init FAIL: err_step=0x%02X\r\n",
                         (unsigned int) motor_get_comm_status());
        if (n > 0 && (size_t) n < sizeof(buf)) {
            uart_send_async((const uint8_t *) buf, (size_t) n, 0);
        }
    }
}

/* ====================================================================
 *  Encoder → travel distance + real-time speed conversion
 *
 *  Uses REAL encoder parameters stored in shadow register:
 *    counts_per_rev = pulse_line × 4 (quadrature edges) × reduction_ratio
 *    mm_per_count   = (π × wheel_diameter) / counts_per_rev
 *
 *  Speed: encoder_10ms is the delta over 10ms (from $MTEP).
 *    speed_mm_s = encoder_10ms × mm_per_count × 100
 *
 *  Called by vMotorSyncTask every 10ms after sync_encoder_from_device.
 * ==================================================================== */

void motor_update_derived(void)
{
    float pulse_line       = (float)motor_get_pulse_line();       /* 500 */
    float reduction_ratio  = (float)motor_get_reduction_ratio();  /* 30 */
    float wheel_diameter   = motor_get_wheel_diameter();          /* 67.0 */

    /* counts per wheel revolution (real encoder, not board-config) */
    float counts_per_rev   = pulse_line * 4.0f * reduction_ratio;
    float wheel_circ_mm    = 3.1415926f * wheel_diameter;
    float mm_per_count     = wheel_circ_mm / counts_per_rev;

    /* ---- travel distance from accumulated encoder total ---- */
    float dist_left  = (float)motor_get_encoder_left()  * mm_per_count;
    float dist_right = (float)motor_get_encoder_right() * mm_per_count;
    motor_set_distance_left_mm(dist_left);
    motor_set_distance_right_mm(dist_right);

    /* ---- real-time speed from 10ms encoder delta ---- */
    float spd_left  = (float)motor_get_encoder_10ms_left()  * mm_per_count * 100.0f;
    float spd_right = (float)motor_get_encoder_10ms_right() * mm_per_count * 100.0f;
    motor_set_speed_left_mm_s(spd_left);
    motor_set_speed_right_mm_s(spd_right);
}

/* Backward-compat wrapper */
void motor_update_distance(void)
{
    motor_update_derived();
}
