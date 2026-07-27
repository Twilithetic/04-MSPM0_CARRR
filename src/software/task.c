/*
 *  ======== task.c ========
 *  Application task implementations.
 *
 *  vBlueTask     — blink blue LED (PB3) @ 750ms
 *  vGreenTask    — blink green LED (PB2) @ 1000ms
 *  vI2CScanTask  — I2C bus scan (prio 2), runs once, signals logger
 *  vImuPollTask  — IMU poll (prio 3), reads LSM6DSV16X → shadow register
 *  vLoggerTask   — print LED stats + IMU data via DMA-UART @ 1 Hz
 */

#include "include/app_tasks.h"
#include "include/build_in_led.h"
#include "include/led_reg.h"
#include "include/XDS110_cdc.h"
#include "include/imu_shadow.h"           /* shadow register accessors */

/* I2C functions (in src/driver/chip/I2C_test.c) */
extern void i2c_test_init(void);
extern void i2c_scan_bus(void);
extern void i2c_scan_print_results(void);

/* IMU Proxy (in src/driver/board/LSM6DSV16X.c) */
extern bool lsm6dsv16x_is_present(void);
extern bool lsm6dsv16x_init(void);
extern void lsm6dsv16x_sync_from_device(void);

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <stdio.h>

/* ---- Semaphore (created in main.c before scheduler starts) ---- */
extern SemaphoreHandle_t g_scanDoneSem;

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

/* ── I2C scan task (prio 2): runs first, signals logger when done ── */
void vI2CScanTask(void *pvParameters)
{
    (void) pvParameters;
    vTaskDelay(pdMS_TO_TICKS(300));// 等那些芯片先启动
    i2c_scan_bus();

    /* Counting semaphore — give twice: one for ImuPoll, one for Logger */
    xSemaphoreGive(g_scanDoneSem);
    xSemaphoreGive(g_scanDoneSem);

    vTaskDelete(NULL);
}

/* ── IMU poll task (prio 3): runs FOREVER, syncs sensor → shadow ── */
void vImuPollTask(void *pvParameters)
{
    (void) pvParameters;

    /* Wait for I2C scan to finish.  vI2CScanTask (prio 2) gives
     * the semaphore after i2c_scan_bus(), so this is safe. */
    xSemaphoreTake(g_scanDoneSem, portMAX_DELAY);

    /* Check if the IMU was found on the bus */
    if (!lsm6dsv16x_is_present()) {
        /* IMU not found — silently exit */
        vTaskDelete(NULL);
    }

    /* Initialize IMU (I2C is up, bus scan found it) */
    (void) lsm6dsv16x_init();

    TickType_t xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        lsm6dsv16x_sync_from_device();
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(5));  /* 200 Hz */
    }
}

/* ── Logger task (prio 1): prints IMU stats @ ~8 Hz (123 ms period) ── */
void vLoggerTask(void *pvParameters)
{
    (void) pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();

    uart_send_async((const uint8_t *)
        "MSPM0G3507 FreeRTOS — I2C Scan\r\n", 35, 0);

    xSemaphoreTake(g_scanDoneSem, portMAX_DELAY);
    i2c_scan_print_results();

    for (;;) {
        char buf[UART_TX_BUF_SIZE];
        unsigned long ticks = xTaskGetTickCount();
        unsigned long secs  = ticks / 1000;
        unsigned long ms    = ticks % 1000;

        uint16_t qps   = imu_get_qps();
        int16_t  yaw   = imu_get_yaw_deg100();
        int16_t  pitch = imu_get_pitch_deg100();
        int16_t  roll  = imu_get_roll_deg100();

        int n = snprintf(buf, sizeof(buf),
                         "[%lu.%03lus] B:%lu G:%lu | qps:%-3u yaw:%7.2f° pitch:%7.2f° roll:%7.2f°\r\n",
                         secs, ms,
                         (unsigned long) led_get_blue(),
                         (unsigned long) led_get_green(),
                         (unsigned int) qps,
                         (double) yaw   / 100.0,
                         (double) pitch / 100.0,
                         (double) roll  / 100.0);

        if (n > 0 && (size_t) n < sizeof(buf)) {
            uart_send_async((const uint8_t *) buf, (size_t) n, 0);
        }
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(100));
    }
}
