/*
 *  ======== task.c ========
 *  Application task implementations.
 *
 *  vBlueTask   — blink blue LED (PB3) @ 750ms
 *  vGreenTask  — blink green LED (PB2) @ 1000ms
 *  vLoggerTask — print LED stats + I2C scan results via DMA-UART @ 500ms
 */

#include "include/app_tasks.h"
#include "include/build_in_led.h"
#include "include/led_reg.h"
#include "include/XDS110_cdc.h"
#include "include/i2c_scanner_reg.h"

/* I2C functions (in src/driver/chip/I2C_test.c) */
extern void i2c_test_init(void);
extern void i2c_scan_bus(void);

#include <stdio.h>

/* ── Blue LED blink ── */
void vBlueTask(void *pvParameters)
{
    (void) pvParameters;
    for (;;) {
        blue_led_toggle();
        led_inc_blue();
        vTaskDelay(pdMS_TO_TICKS(750));
    }
}

/* ── Green LED blink ── */
void vGreenTask(void *pvParameters)
{
    (void) pvParameters;
    for (;;) {
        green_led_toggle();
        led_inc_green();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* ── IMU / I2C init task: run once, then yield forever ── */
void vImuTask(void *pvParameters)
{
    (void) pvParameters;

    /* I2C scan — must run AFTER scheduler starts (TI Drivers uses semaphores) */
    i2c_test_init();
    i2c_scan_bus();

    /* TODO: LSM6DSV16X init + SFLP quaternion polling loop */
    vTaskSuspend(NULL);
}

/* ── Logger task: print LED stats + I2C scan results @ 1 Hz ── */
void vLoggerTask(void *pvParameters)
{
    (void) pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();

    /* Startup greeting */
    uart_send_async((const uint8_t *)
        "MSPM0G3507 FreeRTOS — TI Drivers I2C Scan\r\n", 47, 0);

    /* ── Print I2C bus scan results (from g_i2c_scan_reg, populated at boot) ── */
    {
        uint8_t cnt = i2c_scan_get_count();
        char scan_buf[64];
        int n2 = snprintf(scan_buf, sizeof(scan_buf),
                          "I2C scan: %u device(s) found\r\n",
                          (unsigned int) cnt);
        if (n2 > 0 && (size_t) n2 < sizeof(scan_buf)) {
            uart_send_async((const uint8_t *) scan_buf, (size_t) n2, 0);
        }
        for (uint8_t idx = 0; idx < cnt; idx++) {
            uint8_t addr   = i2c_scan_get_addr(idx);
            uint8_t whoami = i2c_scan_get_whoami(idx);
            n2 = snprintf(scan_buf, sizeof(scan_buf),
                          "  [%u] 0x%02X  WHO_AM_I=0x%02X\r\n",
                          (unsigned int) idx,
                          (unsigned int) addr,
                          (unsigned int) whoami);
            if (n2 > 0 && (size_t) n2 < sizeof(scan_buf)) {
                uart_send_async((const uint8_t *) scan_buf, (size_t) n2, 0);
            }
        }
    }

    for (;;) {
        char buf[64];
        unsigned long ticks = xTaskGetTickCount();
        unsigned long secs  = ticks / 1000;
        unsigned long ms    = ticks % 1000;

        int n = snprintf(buf, sizeof(buf),
                         "[%lu.%03lus] B:%lu G:%lu\r\n",
                         secs, ms,
                         (unsigned long) led_get_blue(),
                         (unsigned long) led_get_green());

        if (n > 0 && (size_t) n < sizeof(buf)) {
            uart_send_async((const uint8_t *) buf, (size_t) n, 0);
        }
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1000));
    }
}
