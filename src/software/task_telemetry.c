/*
 *  ======== task_telemetry.c ========
 *  Telemetry / observer tasks — poll sensors, sync shadow registers, compute stats.
 *
 *  vLSM6DSV16XSyncTask — IMU poll (prio 3), syncs LSM6DSV16X → shadow @ 10Hz
 *  vMotorSyncTask      — motor encoder sync (prio 3), reads UART → shadow @ 100Hz
 *  vStatsTask          — EMA-smooth QPS + motor sync rate @ 1Hz
 */

#include "include/app_tasks.h"
#include "include/imu_shadow.h"
#include "include/motor_driver_uart.h"

/* I2C / IMU externs */
extern bool lsm6dsv16x_is_present(void);
extern bool lsm6dsv16x_init(void);
extern void lsm6dsv16x_sync_from_device(void);

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

        /* convert encoder total → travel distance (mm) */
        motor_update_distance();

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(10));
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
