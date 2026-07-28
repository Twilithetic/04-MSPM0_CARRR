/*
 *  ======== motor_driver_uart.c ========
 *  4-Way Motor Driver Board Proxy — UART command/response → shadow register.
 *
 *  Hardware:
 *    UART1: PA8 TX, PA9 RX @ 115200 8N1  (SysConfig DriverLib init)
 *
 *  UART I/O (TI Drivers CALLBACK mode + semaphore, per SDK uart_callback example):
 *    RX — UART_read() in CALLBACK mode → callback posts to FreeRTOS queue
 *    TX — UART_write() in CALLBACK mode → callback gives binary semaphore
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

#include "include/motor_driver_reg.h"
#include "ti_msp_dl_config.h"

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <semphr.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include <ti/drivers/UART.h>

/* ====================================================================
 *  TI Drivers UART objects (in this TU to avoid .data conflicts)
 * ==================================================================== */

/*
 *  We need a ti_drivers_config.o for UART_config[] / UART_count.
 *  The existing ti_drivers_i2c_config.o already provides GPIO_config etc.
 *  We use DriverLib UART_init directly + our own UARTMSPM0_HWAttrs.
 */

#include <ti/drivers/uart/UARTMSPM0.h>

/* Number of TI-Drivers UART instances we define here */
#define MOTOR_UART_COUNT  1

/* ---- UART1 hardware attributes (PA8/PA9) ---- */
static const UARTMSP_HWAttrs g_uart1_hw = {
    .regs          = UART_1_INST,
    .irq           = UART_1_INST_INT_IRQN,
    .rxPin         = GPIO_UART_1_IOMUX_RX,
    .rxPinFunction = GPIO_UART_1_IOMUX_RX_FUNC,
    .txPin         = GPIO_UART_1_IOMUX_TX,
    .txPinFunction = GPIO_UART_1_IOMUX_TX_FUNC,
    .mode          = DL_UART_MODE_NORMAL,
    .direction     = DL_UART_DIRECTION_TX_RX,
    .flowControl   = DL_UART_FLOW_CONTROL_NONE,
    .clockSource   = DL_UART_CLOCK_BUSCLK,
    .clockDivider  = DL_UART_CLOCK_DIVIDE_RATIO_4,
    .rxIntFifoThr  = DL_UART_RX_FIFO_LEVEL_ONE_ENTRY,
    .txIntFifoThr  = DL_UART_TX_FIFO_LEVEL_EMPTY,
};

/*
 *  The TI Drivers UART module needs an array of UART_Config entries,
 *  each pointing to a UART_Data_Object + UARTMSP_HWAttrs.
 */
static UART_Data_Object  g_uart1_obj;
static const UART_Config g_uart1_cfg = {
    .object  = &g_uart1_obj,
    .hwAttrs = &g_uart1_hw,
};
const UART_Config UART_config[MOTOR_UART_COUNT] = { { .object = &g_uart1_obj, .hwAttrs = &g_uart1_hw } };
const uint_least8_t UART_count = MOTOR_UART_COUNT;

/* ====================================================================
 *  UART1 via TI Drivers (CALLBACK mode for RX and TX)
 * ==================================================================== */

static UART_Handle    g_motor_uart  = NULL;
static QueueHandle_t  g_rx_queue    = NULL;  /* byte queue filled by RX callback */
static SemaphoreHandle_t g_tx_sem   = NULL;  /* signaled by TX callback when write done */

/* DMA-safe persistent buffers for CALLBACK mode */
#define RX_BUF_SIZE 128
#define TX_BUF_SIZE 128
static uint8_t g_rx_buf[RX_BUF_SIZE];
static uint8_t g_tx_buf[TX_BUF_SIZE];

/* ---- RX callback (ISR context → push bytes into FreeRTOS queue) ---- */
static void motor_rx_callback(UART_Handle handle, void *buf, size_t count,
                               void *userArg, int_fast16_t status)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (status == UART_STATUS_SUCCESS && count > 0) {
        const uint8_t *p = (const uint8_t *)buf;
        for (size_t i = 0; i < count; i++) {
            xQueueSendFromISR(g_rx_queue, &p[i], &xHigherPriorityTaskWoken);
        }
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/* ---- TX callback (ISR context → signal semaphore that write is done) ---- */
static void motor_tx_callback(UART_Handle handle, void *buf, size_t count,
                               void *userArg, int_fast16_t status)
{
    (void)handle; (void)buf; (void)count; (void)userArg; (void)status;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(g_tx_sem, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/* ---- ISR handler (SysConfig already generates UART1_IRQHandler) ---- */
void UART1_IRQHandler(void)
{
    UARTMSP_interruptHandler((UART_Handle)&g_uart1_cfg);
}

/* ---- Init UART1 with TI Drivers ---- */
static bool motor_uart_init(void)
{
    if (g_motor_uart != NULL) return true;

    g_rx_queue = xQueueCreate(256, sizeof(uint8_t));
    g_tx_sem   = xSemaphoreCreateBinary();
    if (g_rx_queue == NULL || g_tx_sem == NULL) return false;

    UART_Params params;
    UART_Params_init(&params);
    params.baudRate      = 115200;
    params.readMode      = UART_Mode_CALLBACK;
    params.writeMode     = UART_Mode_CALLBACK;
    params.readCallback  = motor_rx_callback;
    params.writeCallback = motor_tx_callback;
    params.readReturnMode = UART_ReadReturnMode_FULL;

    g_motor_uart = UART_open(0, &params);  /* UART_config[0] */
    if (g_motor_uart == NULL) return false;

    /* Start background read */
    UART_read(g_motor_uart, g_rx_buf, sizeof(g_rx_buf), NULL);

    return true;
}

/* ====================================================================
 *  TX — CALLBACK mode with semaphore
 * ==================================================================== */

static void motor_uart_send(const char *s)
{
    if (!g_motor_uart || !s || !*s) return;

    size_t len = strlen(s);
    if (len > sizeof(g_tx_buf)) len = sizeof(g_tx_buf);
    memcpy(g_tx_buf, s, len);

    /* UART_write() in CALLBACK mode returns immediately;
     * tx callback signals g_tx_sem when done. */
    UART_write(g_motor_uart, g_tx_buf, len, NULL);

    /* Wait for TX to complete */
    xSemaphoreTake(g_tx_sem, pdMS_TO_TICKS(500));
}

/* ====================================================================
 *  RX — CALLBACK mode with FreeRTOS queue
 * ==================================================================== */

static int motor_uart_getc_timeout(TickType_t timeout_ticks)
{
    uint8_t byte;
    if (xQueueReceive(g_rx_queue, &byte, timeout_ticks) == pdTRUE) {
        return (int)byte;
    }
    return -1;
}

static void motor_uart_drain(void)
{
    uint8_t byte;
    while (xQueueReceive(g_rx_queue, &byte, 0) == pdTRUE) { }
    /* Re-arm background read */
    if (g_motor_uart) {
        UART_read(g_motor_uart, g_rx_buf, sizeof(g_rx_buf), NULL);
    }
}

/* ====================================================================
 *  Line reader
 * ==================================================================== */

#define MOTOR_RESP_BUF_SIZE 128
static char g_motor_rx_line[MOTOR_RESP_BUF_SIZE];

static int motor_uart_recv_line(TickType_t byte_timeout_ticks)
{
    size_t idx = 0;
    while (idx < sizeof(g_motor_rx_line) - 1) {
        int c = motor_uart_getc_timeout(byte_timeout_ticks);
        if (c < 0) break;          /* timeout */
        if (c == '\n') break;
        if (c != '\r') g_motor_rx_line[idx++] = (char)c;
    }
    g_motor_rx_line[idx] = '\0';
    return (int)idx;
}

/* ====================================================================
 *  Command helper
 * ==================================================================== */

static const char *motor_send_cmd(const char *cmd, unsigned long timeout_ms)
{
    if (!motor_uart_init()) return NULL;
    motor_uart_drain();
    motor_uart_send(cmd);
    motor_uart_recv_line(pdMS_TO_TICKS(timeout_ms));
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
    if (!resp) return false;  vTaskDelay(pdMS_TO_TICKS(100));

    resp = motor_send_cmd("$deadzone:1250#\r\n", 200);
    if (!resp) return false;  vTaskDelay(pdMS_TO_TICKS(100));

    resp = motor_send_cmd("$mline:13#\r\n", 200);
    if (!resp) return false;  vTaskDelay(pdMS_TO_TICKS(100));

    resp = motor_send_cmd("$mphase:45#\r\n", 200);
    if (!resp) return false;  vTaskDelay(pdMS_TO_TICKS(100));

    resp = motor_send_cmd("$wdiameter:67#\r\n", 200);
    if (!resp) return false;  vTaskDelay(pdMS_TO_TICKS(100));

    resp = motor_send_cmd("$read_vol#\r\n", 200);
    if (!resp) return false;

    r->motor_type = 3;  r->pulse_line = 13;  r->reduction_ratio = 45;
    r->wheel_diameter = 67.0f;  r->deadzone = 1250;
    r->comm_status = 0;  r->initialized = true;
    return true;
}

uint16_t motor_read_battery_voltage(void)
{
    const char *resp = motor_send_cmd("$read_vol#\r\n", 200);
    if (!resp) return 0;
    float volts = 0.0f;
    if (sscanf(resp, "$Battery:%fV", &volts) == 1)
        return (uint16_t)(volts * 10.0f);
    return 0;
}

void sync_encoder_from_device(MotorDriverReg *r)
{
    const char *resp = motor_send_cmd("$upload:0,1,0#\r\n", 100);
    if (!resp) { r->comm_status = 0xFF; return; }

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
