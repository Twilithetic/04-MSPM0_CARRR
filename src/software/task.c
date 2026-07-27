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
#include "include/lsm6dsv16x_reg.h"

/* I2C functions (in src/driver/chip/I2C_test.c) */
extern void i2c_test_init(void);
extern void i2c_scan_bus(void);
extern void i2c_scan_print_results(void);

/* IMU Proxy (in src/driver/board/LSM6DSV16X.c) */
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

    i2c_test_init();
    i2c_scan_bus();

    /* Init IMU (must be after I2C is up) */
    (void) lsm6dsv16x_init();

    /* Counting semaphore — give twice: one for ImuPoll, one for Logger */
    xSemaphoreGive(g_scanDoneSem);
    xSemaphoreGive(g_scanDoneSem);

    vTaskDelete(NULL);
}

/* ── IMU poll task (prio 3): runs FOREVER, syncs sensor → shadow ── */
void vImuPollTask(void *pvParameters)
{
    (void) pvParameters;

    /* Wait for I2C scan + IMU init to finish.  vI2CScanTask (prio 2)
     * creates the semaphore and gives it after init, so this is safe. */
    xSemaphoreTake(g_scanDoneSem, portMAX_DELAY);

    if (!imu_is_ready()) {
        /* IMU init failed — silently exit */
        vTaskDelete(NULL);
    }

    TickType_t xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        lsm6dsv16x_sync_from_device();
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(10));  /* 100 Hz */
    }
}

/* ── Logger task (prio 1): waits for scan, then prints IMU data @ 1 Hz ── */
void vLoggerTask(void *pvParameters)
{
    (void) pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();

    /* Startup greeting */
    uart_send_async((const uint8_t *)
        "MSPM0G3507 FreeRTOS — TI Drivers I2C Scan\r\n", 47, 0);

    /*
     * Block until vI2CScanTask gives the semaphore.
     * vI2CScanTask (prio 2) creates g_scanDoneSem before this task
     * (prio 1) ever runs, so the pointer is guaranteed non-NULL.
     * If however the scan hangs forever, this waits forever too —
     * put a timeout if that's not acceptable.
     */
    xSemaphoreTake(g_scanDoneSem, portMAX_DELAY);

    /* Print I2C bus scan results */
    i2c_scan_print_results();

    for (;;) {
        char buf[96];
        unsigned long ticks = xTaskGetTickCount();
        unsigned long secs  = ticks / 1000;
        unsigned long ms    = ticks % 1000;

        int n = snprintf(buf, sizeof(buf),
                         "[%lu.%03lus] B:%lu G:%lu | IMU q:%d %d %d %d | g:%d %d %d | a:%d %d %d\r\n",
                         secs, ms,
                         (unsigned long) led_get_blue(),
                         (unsigned long) led_get_green(),
                         (int) imu_get_qw(), (int) imu_get_qx(),
                         (int) imu_get_qy(), (int) imu_get_qz(),
                         (int) imu_get_gx(), (int) imu_get_gy(), (int) imu_get_gz(),
                         (int) imu_get_ax(), (int) imu_get_ay(), (int) imu_get_az());

        if (n > 0 && (size_t) n < sizeof(buf)) {
            uart_send_async((const uint8_t *) buf, (size_t) n, 0);
        }
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1000));
    }
}
