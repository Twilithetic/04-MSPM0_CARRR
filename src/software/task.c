/*
 *  ======== task.c ========
 *  Application task implementations.
 *
 *  vBlueTask   — blink blue LED (PB3) @ 750ms
 *  vGreenTask  — blink green LED (PB2) @ 1000ms
 *  vLoggerTask — send LedReg stats via DMA-UART @ 500ms
 */

#include "include/app_tasks.h"
#include "include/build_in_led.h"
#include "include/led_reg.h"
#include "include/XDS110_cdc.h"

#include <stdio.h>

void vBlueTask(void *pvParameters)
{
    (void) pvParameters;
    for (;;) {
        blue_led_toggle();
        led_inc_blue();
        vTaskDelay(pdMS_TO_TICKS(750));
    }
}

void vGreenTask(void *pvParameters)
{
    (void) pvParameters;
    for (;;) {
        green_led_toggle();
        led_inc_green();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void vLoggerTask(void *pvParameters)
{
    (void) pvParameters;

    /* Startup greeting */
    uart_send_async((const uint8_t *)
        "MSPM0G3507 FreeRTOS — DMA UART0 (PA10/PA11) 115200\r\n", 52, 0);

    for (;;) {
        char buf[64];
        int n = snprintf(buf, sizeof(buf),
                         "[%5lu] B:%lu G:%lu\r\n",
                         (unsigned long) xTaskGetTickCount(),
                         (unsigned long) led_get_blue(),
                         (unsigned long) led_get_green());
        if (n > 0 && (size_t) n < sizeof(buf)) {
            uart_send_async((const uint8_t *) buf, (size_t) n, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
