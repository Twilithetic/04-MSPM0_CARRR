/*
 *  ======== task.c ========
 *  Application task implementations.
 *
 *  vBlueTask  — blink blue LED (PB2) @ 500ms
 *  vGreenTask — blink green LED (PB3) @ 500ms, 250ms phase offset
 */

#include "include/app_tasks.h"
#include "include/build_in_led.h"

void vBlueTask(void *pvParameters)
{
    (void) pvParameters;
    for (;;) {
        blue_led_toggle();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void vGreenTask(void *pvParameters)
{
    (void) pvParameters;
    /* Phase offset: delay first toggle by 250 ms */
    vTaskDelay(pdMS_TO_TICKS(250));
    for (;;) {
        green_led_toggle();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
