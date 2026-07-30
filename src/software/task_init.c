/*
 *  ======== task_init.c ========
 *  One-shot initialization tasks — run once, signal semaphores, then delete.
 *
 *  vI2CScanTask   — I2C bus scan (prio 2), signals vLSM6DSV16XSyncTask + vLoggerTask
 *  vMotorInitTask — motor driver init + TT encoder config (prio 2),
 *                   signals vLoggerTask / vMotorSyncTask / vCarCtrlTask
 */

#include "include/app_tasks.h"
#include "include/XDS110_cdc.h"
#include "include/motor_driver_uart.h"

/* I2C functions (in src/driver/chip/I2C_test.c) */
extern void i2c_scan_bus(void);

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

/* ---- Semaphore (created in main.c before scheduler starts) ---- */
extern SemaphoreHandle_t g_scanDoneSem;
extern SemaphoreHandle_t g_motorDoneSem;
extern SemaphoreHandle_t g_motorSyncSem;

/* ── I2C scan task (prio 2): runs first, signals logger when done ── */
void vI2CScanTask(void *pvParameters)
{
    (void) pvParameters;
    vTaskDelay(pdMS_TO_TICKS(300)); /* wait for other chips to boot */
    i2c_scan_bus();

    /* Counting semaphore — give twice: one for ImuPoll, one for Logger */
    xSemaphoreGive(g_scanDoneSem);
    xSemaphoreGive(g_scanDoneSem);

    vTaskDelete(NULL);
}

/* ── Motor Init Task (prio 2): one-shot config, signals Logger, then delete ── */
void vMotorInitTask(void *pvParameters)
{
    (void) pvParameters;

    /* Init UART1 + send stop commands */
    motor_driver_init();

    /* Run the TT encoder config sequence */
    bool ok = cmd_config_tt_encoder(&g_motor_driver_reg);

    motor_print_config(ok);

    /* Release Logger — motor init is done, encoder data is safe to read */
    xSemaphoreGive(g_motorDoneSem);

    /* Release MotorSync — motor init is done, sync can start */
    xSemaphoreGive(g_motorSyncSem);

    vTaskDelete(NULL);
}
