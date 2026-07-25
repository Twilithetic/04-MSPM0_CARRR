/*
 *  ======== task.c ========
 *  Application task implementations.
 *
 *  vBlueTask  — blink blue LED (PB2) @ 500ms
 *  vGreenTask — blink green LED (PB3) @ 500ms, 250ms phase offset
 */

#include "include/app_tasks.h"
#include "include/build_in_led.h"
#include "include/led_reg.h"

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
