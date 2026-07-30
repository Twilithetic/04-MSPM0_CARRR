/*
 *  ======== task_control.c ========
 *  Control tasks — car speed / steering.
 *
 *  vCarCtrlTask — PWM deadzone sweep test.
 *    Ramps PWM from 0 by +100 every 5s on both wheels (M2 & M4).
 *    Watch the logger's 10ms encoder delta + speed column to find the
 *    PWM value where wheels actually start turning (= deadzone boundary).
 */

#include "include/app_tasks.h"
#include "include/motor_driver_uart.h"
#include "include/motor_driver_reg.h"
#include "include/XDS110_cdc.h"

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <stdio.h>

/* ---- Semaphores ---- */
extern SemaphoreHandle_t g_ctrlSyncSem;

#define DEADZONE_STEP_PWM   100     /* +100 PWM per step */
#define DEADZONE_STEP_SEC   3U      /* seconds per step  */

/* ── Car Speed Control Task (prio 3): 100Hz ── */
void vCarCtrlTask(void *pvParameters)
{
    (void) pvParameters;

    xSemaphoreTake(g_ctrlSyncSem, portMAX_DELAY);

    if (!motor_is_initialized()) {
        vTaskDelete(NULL);
    }

    TickType_t xLastWakeTime = xTaskGetTickCount();
    int16_t    pwm           = 0;
    int16_t    prev_pwm      = -1;

    /* Stop speed-control loop so it doesn't fight our PWM */
    motor_send_speed(0, 0, 0, 0);

    for (;;) {
        /* ── Ramp: step every DEADZONE_STEP_SEC seconds ── */
        uint32_t second = (uint32_t)(xTaskGetTickCount() / configTICK_RATE_HZ);
        pwm = (int16_t)((second / DEADZONE_STEP_SEC) * DEADZONE_STEP_PWM);

        /* Clamp to max PWM */
        if (pwm > 3600) pwm = 3600;

        /* Only print & send when PWM changes (once per step) */
        if (pwm != prev_pwm) {
            char buf[64];
            int n = snprintf(buf, sizeof(buf),
                             "\r\n[CTRL] PWM → %+d (%d%%)\r\n",
                             (int)pwm,
                             (int)((int32_t)pwm * 100 / 3600));
            if (n > 0 && (size_t)n < sizeof(buf)) {
                uart_send_async((const uint8_t *)buf, (size_t)n, 0);
            }
            prev_pwm = pwm;
        }

        /* M2=RIGHT, M4=LEFT */
        motor_send_pwm(0, pwm, 0, pwm);

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(10));
    }
}
