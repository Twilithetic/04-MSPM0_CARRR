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

#define DELAY_100MS_CYCLES  (3200000U)

extern void i2c_scan_bus(void);
extern void i2c_test_init(void);

int main(void)
{
    /* ---- Hardware init ---- */
    SYSCFG_DL_init();
    uart_init();
    delay_cycles(DELAY_100MS_CYCLES);

    /* ---- I2C bus scan (startup, before scheduler) ---- */
    // i2c_test_init();
    // i2c_scan_bus();

    /* ---- Create application tasks ---- */
    xTaskCreate(vBlueTask,   "BlueLED",  configMINIMAL_STACK_SIZE,
                NULL,        1,          NULL);
    xTaskCreate(vGreenTask,  "GreenLED", configMINIMAL_STACK_SIZE,
                NULL,        1,          NULL);
    xTaskCreate(vImuTask,   "ImuPoll",  configMINIMAL_STACK_SIZE * 3,
                NULL,        2,          NULL);
    xTaskCreate(vLoggerTask, "Logger",   configMINIMAL_STACK_SIZE * 2,
                NULL,        1,          NULL);

    /* ---- Start FreeRTOS scheduler (never returns) ---- */
    vTaskStartScheduler();

    /* Should never reach here */
    for (;;) {}
}
