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
#include <string.h>  /* not that you actually need it after your first draft — but
                         here because you mentioned the pattern */

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

    /* One-shot: un-gate DMA ISR → semaphore path */
    uart_init_post_scheduler();

    /* Startup greeting */
    uart_send("MSPM0G3507 FreeRTOS — DMA UART0 (PA10/PA11) 115200\r\n");

    for (;;) {
        char buf[64];
        int n = snprintf(buf, sizeof(buf),
                         "[%5lu] B:%lu G:%lu\r\n",
                         (unsigned long) xTaskGetTickCount(),
                         (unsigned long) led_get_blue(),
                         (unsigned long) led_get_green());
        if (n > 0 && (size_t) n < sizeof(buf)) {
            uart_send(buf);
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
