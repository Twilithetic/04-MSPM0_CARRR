/*
 *  ======== task.c ========
 *  Application task implementations.
 *
 *  vBlueTask    — blink blue LED (PB3) @ 750ms
 *  vGreenTask   — blink green LED (PB2) @ 1000ms
 *  vI2CScanTask — I2C bus scan (prio 2), runs once, signals logger
 *  vLoggerTask  — print LED stats + I2C scan results via DMA-UART @ 1 Hz
 */

#include "include/app_tasks.h"
#include "include/build_in_led.h"
#include "include/led_reg.h"
#include "include/XDS110_cdc.h"

/* I2C functions (in src/driver/chip/I2C_test.c) */
extern void i2c_test_init(void);
extern void i2c_scan_bus(void);
extern void i2c_scan_print_results(void);

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <stdio.h>

/* ---- Semaphore: I2C scan done → Logger can print ---- */
static SemaphoreHandle_t g_scanDoneSem = NULL;

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

    /*
     * Create binary semaphore in "taken" state (count=0).
     * vI2CScanTask (prio 2) ALWAYS runs before vLoggerTask (prio 1),
     * so g_scanDoneSem is guaranteed valid before vLoggerTask tries to take it.
     */
    g_scanDoneSem = xSemaphoreCreateBinary();

    i2c_test_init();
    i2c_scan_bus();

    /* Signal logger: scan done.  vLoggerTask unblocks and prints. */
    xSemaphoreGive(g_scanDoneSem);

    vTaskDelete(NULL);
}

/* ── Logger task (prio 1): waits for scan, then prints @ 1 Hz ── */
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
