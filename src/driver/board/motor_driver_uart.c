/*
 *  ======== motor_driver_uart.c ========
 *  4-Way Motor Driver Board Proxy — UART command/response → shadow register.
 *
 *  Hardware:
 *    UART1: PA8 TX, PA9 RX @ 115200 8N1
 *
 *  UART I/O (TI Drivers BLOCKING mode):
 *    Motor protocol is simple request-response: send a short command,
 *    read back a short response line.  BLOCKING mode is the right fit.
 *
 *  NOT using CALLBACK+DMA because:
 *    TX and RX share one DMA channel → TX kills the background RX DMA.
 *    re-arming RX after each TX adds complexity for no benefit when the
 *    protocol is inherently request → wait → response.
 *
 *  Vendor protocol (docs/4路电机驱动板/):
 *    Config:   $mtype:3#  $deadzone:1250#  $mline:13#  $mphase:45#  $wdiameter:67#
 *    Control:  $spd:0,0,0,0#  $pwm:0,0,0,0#
 *    Upload:   $upload:0,1,0#   →  $MTEP:M1,M2,M3,M4#
 *    Read:     $read_vol#       →  $Battery:7.40V#
 *
 *  Motor mapping:
 *    M1=LF  M2=LR  M3=RF  M4=RR
 *    LEFT=M4  RIGHT=M2
 */

#include "include/motor_driver_uart.h"
#include "ti_msp_dl_config.h"

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include <ti/drivers/UART.h>
#include <ti/drivers/uart/UARTMSPM0.h>

/* ====================================================================
 *  UART1 config — PA8/PA9, TI Drivers BLOCKING mode
 * ==================================================================== */

#define CONFIG_UART_COUNT  1
#define CONFIG_UART_0      0

static const UARTMSP_HWAttrs g_uart_hw_attrs[CONFIG_UART_COUNT] = {
    {
        .regs          = UART1,
        .irq           = UART1_INT_IRQn,
        .rxPin         = IOMUX_PINCM20,            /* PA9 */
        .rxPinFunction = IOMUX_PINCM20_PF_UART1_RX,
        .txPin         = IOMUX_PINCM19,            /* PA8 */
        .txPinFunction = IOMUX_PINCM19_PF_UART1_TX,
        .mode          = DL_UART_MODE_NORMAL,
        .direction     = DL_UART_DIRECTION_TX_RX,
        .flowControl   = DL_UART_FLOW_CONTROL_NONE,
        .clockSource   = DL_UART_CLOCK_BUSCLK,
        .clockDivider  = DL_UART_CLOCK_DIVIDE_RATIO_4,
        .rxIntFifoThr  = DL_UART_RX_FIFO_LEVEL_ONE_ENTRY,
        .txIntFifoThr  = DL_UART_TX_FIFO_LEVEL_EMPTY,
    },
};

#define CONFIG_UART_BUFFER_SIZE  128

static uint8_t g_uart_rx_buf[CONFIG_UART_BUFFER_SIZE];
static uint8_t g_uart_tx_buf[CONFIG_UART_BUFFER_SIZE];

static UART_Data_Object g_uart_objects[CONFIG_UART_COUNT] = {
    {
        .object = {
            .supportFxns        = &UARTMSPSupportFxns,
            .buffersSupported   = true,
            .eventsSupported    = false,
            .callbacksSupported = false,   /* BLOCKING mode */
            .dmaSupported       = false,   /* no DMA — interrupt FIFO */
        },
        .buffersObject = {
            .rxBufPtr  = g_uart_rx_buf,
            .txBufPtr  = g_uart_tx_buf,
            .rxBufSize = sizeof(g_uart_rx_buf),
            .txBufSize = sizeof(g_uart_tx_buf),
        },
    },
};

const UART_Config UART_config[CONFIG_UART_COUNT] = {
    { &g_uart_objects[CONFIG_UART_0], &g_uart_hw_attrs[CONFIG_UART_0] },
};

const uint_least8_t UART_count = CONFIG_UART_COUNT;

/* ---- ISR dispatch ---- */
void UART1_IRQHandler(void)
{
    UARTMSP_interruptHandler((UART_Handle)&UART_config[0]);
}

/* ====================================================================
 *  Application state
 * ==================================================================== */

static UART_Handle g_motor_uart = NULL;

/* ---- Init UART1 with TI Drivers ---- */
static bool motor_uart_init(void)
{
    if (g_motor_uart != NULL) return true;

    UART_Params params;
    UART_Params_init(&params);
    params.baudRate       = 115200;
    params.readMode       = UART_Mode_BLOCKING;
    params.writeMode      = UART_Mode_BLOCKING;
    params.readReturnMode = UART_ReadReturnMode_PARTIAL;

    g_motor_uart = UART_open(CONFIG_UART_0, &params);
    return (g_motor_uart != NULL);
}

/* ====================================================================
 *  Send raw string (BLOCKING — all bytes transmitted to shift register)
 * ==================================================================== */

static void motor_uart_send(const char *s)
{
    if (!g_motor_uart || !s || !*s) return;

    size_t len = strlen(s);
    size_t written = 0;
    UART_write(g_motor_uart, s, len, &written);
}

/* ====================================================================
 *  Receive a line terminated by '\n', with per-byte timeout
 * ==================================================================== */

#define MOTOR_RESP_BUF_SIZE 128
static char g_motor_rx_line[MOTOR_RESP_BUF_SIZE];

static const char *motor_uart_recv_line(TickType_t timeout_ms)
{
    size_t idx = 0;
    uint8_t byte;

    while (idx < sizeof(g_motor_rx_line) - 1) {
        size_t n = 0;
        int_fast16_t rc = UART_readTimeout(g_motor_uart, &byte, 1, &n, timeout_ms);
        if (rc != UART_STATUS_SUCCESS || n != 1) break;  /* timeout / error */
        if (byte == '\n') break;
        if (byte != '\r') g_motor_rx_line[idx++] = (char)byte;
    }
    g_motor_rx_line[idx] = '\0';
    return g_motor_rx_line;
}

/* ====================================================================
 *  Command helper: send command, then read response line
 * ==================================================================== */

static const char *motor_send_cmd(const char *cmd, unsigned long timeout_ms)
{
    if (!motor_uart_init()) return NULL;

    /* Flush any stale RX data */
    {
        uint8_t junk;
        size_t n;
        while (UART_readTimeout(g_motor_uart, &junk, 1, &n, 1) == UART_STATUS_SUCCESS && n == 1) { }
    }
    __BKPT(0);
    motor_uart_send(cmd);
    __BKPT(0);
    motor_uart_recv_line(timeout_ms);
    __BKPT(0);
    return g_motor_rx_line;
}

/* ====================================================================
 *  Public API
 * ==================================================================== */

bool motor_driver_init(void)
{
    if (!motor_uart_init()) return false;

    motor_uart_send("$pwm:0,0,0,0#\r\n");
    vTaskDelay(pdMS_TO_TICKS(50));
    motor_uart_send("$spd:0,0,0,0#\r\n");
    vTaskDelay(pdMS_TO_TICKS(50));

    const char *resp = motor_send_cmd("$read_vol#\r\n", 200);
    if (resp && strstr(resp, "Battery")) {
        float volts = 0.0f;
        sscanf(resp, "$Battery:%fV", &volts);
        g_motor_driver_reg.comm_status = (uint8_t)(volts * 10.0f);
        return true;
    }
    return false;
}

bool cmd_config_tt_encoder(MotorDriverReg *r)
{
    const char *resp;

    resp = motor_send_cmd("$mtype:3#\r\n", 200);
    if (!resp || !*resp) return false;  vTaskDelay(pdMS_TO_TICKS(100));

    resp = motor_send_cmd("$deadzone:1250#\r\n", 200);
    if (!resp || !*resp) return false;  vTaskDelay(pdMS_TO_TICKS(100));

    resp = motor_send_cmd("$mline:13#\r\n", 200);
    if (!resp || !*resp) return false;  vTaskDelay(pdMS_TO_TICKS(100));

    resp = motor_send_cmd("$mphase:45#\r\n", 200);
    if (!resp || !*resp) return false;  vTaskDelay(pdMS_TO_TICKS(100));

    resp = motor_send_cmd("$wdiameter:67#\r\n", 200);
    if (!resp || !*resp) return false;  vTaskDelay(pdMS_TO_TICKS(100));

    resp = motor_send_cmd("$read_vol#\r\n", 200);
    if (!resp || !*resp) return false;

    r->motor_type = 3;  r->pulse_line = 13;  r->reduction_ratio = 45;
    r->wheel_diameter = 67.0f;  r->deadzone = 1250;
    r->comm_status = 0;  r->initialized = true;
    return true;
}

uint16_t motor_read_battery_voltage(void)
{
    const char *resp = motor_send_cmd("$read_vol#\r\n", 200);
    if (!resp || !*resp) return 0;
    float volts = 0.0f;
    if (sscanf(resp, "$Battery:%fV", &volts) == 1)
        return (uint16_t)(volts * 10.0f);
    return 0;
}

void sync_encoder_from_device(MotorDriverReg *r)
{
    const char *resp = motor_send_cmd("$upload:0,1,0#\r\n", 100);
    if (!resp || !*resp) { r->comm_status = 0xFF; return; }

    int16_t m1 = 0, m2 = 0, m3 = 0, m4 = 0;
    if (sscanf(resp, "$MTEP:%hd,%hd,%hd,%hd#", &m1, &m2, &m3, &m4) < 4)
        { r->comm_status = 0xFE; return; }

    r->encoder_10ms_left  = m4;
    r->encoder_10ms_right = m2;
    r->comm_status = 0;
}

void sync_config_from_device(MotorDriverReg *r) { (void)r; }

void flush_speed_to_device(MotorDriverReg *r)
{
    char buf[64];
    int n = snprintf(buf, sizeof(buf), "$spd:0,%d,0,%d#\r\n",
                     (int)r->target_speed_right, (int)r->target_speed_left);
    if (n > 0 && (size_t)n < sizeof(buf)) motor_uart_send(buf);
}

void flush_pwm_to_device(MotorDriverReg *r)
{
    char buf[64];
    int n = snprintf(buf, sizeof(buf), "$pwm:0,%d,0,%d#\r\n",
                     (int)r->target_pwm_right, (int)r->target_pwm_left);
    if (n > 0 && (size_t)n < sizeof(buf)) motor_uart_send(buf);
}

void flush_stop_to_device(MotorDriverReg *r)
{
    r->target_speed_left = r->target_speed_right = 0;
    r->target_pwm_left   = r->target_pwm_right   = 0;
    flush_speed_to_device(r);
    flush_pwm_to_device(r);
}

void motor_uart_putchar(char c)
{
    if (g_motor_uart) {
        size_t w;
        UART_write(g_motor_uart, &c, 1, &w);
    }
}
