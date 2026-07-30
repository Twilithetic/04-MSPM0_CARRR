/*
 *  ======== task_telemetry.c ========
 *  Telemetry / observer tasks — poll sensors, sync shadow registers, compute stats.
 *
 *  vLSM6DSV16XSyncTask — IMU poll (prio 3), syncs LSM6DSV16X → shadow @ 10Hz
 *  vLine8SyncTask      — line8 sensor sync (prio 3), reads GPIO → shadow @ 100Hz
 *  vMotorSyncTask      — motor encoder sync (prio 3), reads UART1 → shadow @ 100Hz
 *  vStatsTask          — EMA-smooth QPS + motor sync rate @ 1Hz
 */

#include "include/app_tasks.h"
#include "include/imu_shadow.h"
#include "include/motor_driver_uart.h"
#include "include/line8_reg.h"

/* I2C / IMU externs */
extern bool lsm6dsv16x_is_present(void);
extern bool lsm6dsv16x_init(void);
extern void lsm6dsv16x_sync_from_device(void);

/* Line8 driver extern (in src/driver/board/line8_gpio.c) */
extern void sync_line8_from_device(Line8Reg *r);

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

/* ---- Semaphores (created in main.c) ---- */
extern SemaphoreHandle_t g_scanDoneSem;
extern SemaphoreHandle_t g_motorSyncSem;
extern SemaphoreHandle_t g_ctrlSyncSem;

/* ── IMU sync task (prio 3): runs FOREVER, syncs sensor → shadow @ 10Hz ── */
void vLSM6DSV16XSyncTask(void *pvParameters)
{
    (void) pvParameters;

    /* Wait for I2C scan to finish */
    xSemaphoreTake(g_scanDoneSem, portMAX_DELAY);

    /* Check if the IMU was found on the bus */
    if (!lsm6dsv16x_is_present()) {
        vTaskDelete(NULL);
    }

    /* Initialize IMU (I2C is up, bus scan found it) */
    (void) lsm6dsv16x_init();

    TickType_t xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        lsm6dsv16x_sync_from_device();
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(100));  /* 10 Hz */
    }
}

/* ── Line8 Sync Task (prio 3): periodic line-sensor read @ 100Hz ── */
void vLine8SyncTask(void *pvParameters)
{
    (void) pvParameters;

    /* GPIO pins are configured by SYSCFG_DL_init() — nothing else to wait for */
    TickType_t xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        /* sync: read the 8 IR GPIO inputs → write shadow register */
        sync_line8_from_device(&g_line8_reg);

        /* compute derived white-values */
        line8_compute_white_vals();

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(10));
    }
}

/* ── Motor Sync Task (prio 3): periodic encoder read @ 100Hz ── */
void vMotorSyncTask(void *pvParameters)
{
    (void) pvParameters;

    /* Wait for motor init to complete */
    xSemaphoreTake(g_motorSyncSem, portMAX_DELAY);

    if (!motor_is_initialized()) {
        vTaskDelete(NULL);
    }

    xSemaphoreGive(g_ctrlSyncSem);

    TickType_t xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        /* sync: read UART encoders → write shadow register */
        sync_encoder_from_device(&g_motor_driver_reg);

        /* convert encoder → travel distance (mm) + speed (mm/s) */
        motor_update_derived();

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(100));
    }
}

/* ── Stats Task (prio 1): EMA-smooth QPS & msync @ 1Hz ── */
void vStatsTask(void *pvParameters)
{
    (void) pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        stats_update();
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1000));
    }
}
