/*
 *  ======== task_control.c ========
 *  Control tasks — car speed / steering.
 *
 *  vCarCtrlTask — speed step-response test.
 *    100Hz tick.  Speed target alternates 500 ↔ 200 mm/s every 1s.
 *    Speed command is sent only when target changes (not every tick),
 *    letting the driver board's own PID maintain speed between switches.
 */

#include "include/app_tasks.h"
#include "include/motor_driver_uart.h"
#include "include/motor_driver_reg.h"
#include "include/XDS110_cdc.h"

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <stdbool.h>
#include <stdio.h>

/* ---- Semaphores ---- */
extern SemaphoreHandle_t g_ctrlSyncSem;

/* ── Speed step-test: alternate two targets every 1 s ── */
typedef enum {
    SPEED_FAST = 0,
    SPEED_SLOW = 1,
    SPEED_COUNT
} speed_state_t;

static const float speed_targets[SPEED_COUNT] = {
    [SPEED_FAST] = 500.0f,
    [SPEED_SLOW] = 200.0f,
};

/* ── Car Speed Control Task (prio 3): 100Hz ── */
void vCarCtrlTask(void *pvParameters)
{
    (void) pvParameters;

    xSemaphoreTake(g_ctrlSyncSem, portMAX_DELAY);

    if (!motor_is_initialized()) {
        vTaskDelete(NULL);
    }

    /* Board PID — conservative defaults */
    motor_send_pid(0.5f, 0.02f, 0.0f);

    TickType_t     xLastWakeTime = xTaskGetTickCount();
    speed_state_t  state         = SPEED_FAST;
    speed_state_t  prev_state    = SPEED_COUNT;  /* force first TX */

    for (;;) {
        /* ── Decide state from elapsed seconds ── */
        uint32_t second = (uint32_t)(xTaskGetTickCount() / configTICK_RATE_HZ);
        state = (second & 2U) ? SPEED_SLOW : SPEED_FAST;

        /* ── Only send when state actually changed ── */
        if (state != prev_state) {
            float target = speed_targets[state];
            motor_send_speed_mm_s(target, target);

            char buf[64];
            int n = snprintf(buf, sizeof(buf),
                             "\r\n[CTRL] target → %.0f mm/s\r\n",
                             (double)target);
            if (n > 0 && (size_t)n < sizeof(buf)) {
                uart_send_async((const uint8_t *)buf, (size_t)n, 0);
            }
            prev_state = state;
        }

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(10));
    }
}
