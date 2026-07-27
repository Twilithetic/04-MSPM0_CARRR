/*
 *  ======== main.c ========
 *  Minimal FreeRTOS demo — two LEDs blinking asynchronously.
 *
 *  Blue LED (PB3): 500ms period (task priority 1)
 *  Green LED (PB2): 500ms period, 250ms phase offset (task priority 1)
 *
 *  Uses DriverLib DL_GPIO_* directly via build_in_led.c.
 *  Tasks are implemented in src/software/task.c.
 */

#include "ti_msp_dl_config.h"
#include "include/app_tasks.h"
#include "include/app_hooks.h"
#include "include/XDS110_cdc.h"
#include "include/i2c_scanner_reg.h"

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

#define DELAY_100MS_CYCLES  (3200000U)

/* ---- Semaphore: I2C scan done → consumers can proceed ---- */
SemaphoreHandle_t g_scanDoneSem = NULL;

int main(void)
{
    /* ---- Hardware init ---- */
    SYSCFG_DL_init();
    uart_init();
    delay_cycles(DELAY_100MS_CYCLES);

    /* Create semaphore BEFORE scheduler starts — two tasks will wait on it.
     * Counting semaphore: max 2, initial 0.  vI2CScanTask gives it once
     * after scan+init; both vImuPollTask and vLoggerTask can take. */
    g_scanDoneSem = xSemaphoreCreateCounting(2, 0);

    /* ---- Create application tasks ---- */
    BaseType_t xReturn;

    xReturn = xTaskCreate(vBlueTask,   "BlueLED",  configMINIMAL_STACK_SIZE,
                          NULL,        1,          NULL);
    configASSERT(xReturn == pdPASS);

    xReturn = xTaskCreate(vGreenTask,  "GreenLED", configMINIMAL_STACK_SIZE,
                          NULL,        1,          NULL);
    configASSERT(xReturn == pdPASS);

    xReturn = xTaskCreate(vI2CScanTask, "I2CScan",  configMINIMAL_STACK_SIZE * 2,
                          NULL,        2,          NULL);
    configASSERT(xReturn == pdPASS);

    xReturn = xTaskCreate(vImuPollTask, "ImuPoll",  configMINIMAL_STACK_SIZE * 4,
                          NULL,        3,          NULL);
    configASSERT(xReturn == pdPASS);

    xReturn = xTaskCreate(vLoggerTask, "Logger",   configMINIMAL_STACK_SIZE * 3,
                          NULL,        1,          NULL);
    configASSERT(xReturn == pdPASS);

    /* ---- Start FreeRTOS scheduler (never returns) ---- */
    vTaskStartScheduler();

    /* Should never reach here */
    for (;;) {}
}
