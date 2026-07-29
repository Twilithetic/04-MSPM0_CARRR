/*
 *  ======== task_control.c ========
 *  Control tasks — car speed / steering.
 *
 *  vCarCtrlTask — sets board PID params + target speed @ 100Hz.
 *                 The driver board runs its own PID internally; this task
 *                 periodically refreshes the speed target so the board loop
 *                 stays current (for acceleration ramps / line-following).
 */

#include "include/app_tasks.h"
#include "include/motor_driver_uart.h"

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

/* ---- Semaphores ---- */
extern SemaphoreHandle_t g_ctrlSyncSem;

/* ── Car Speed Control Task (prio 3): 100Hz ── */
void vCarCtrlTask(void *pvParameters)
{
    (void) pvParameters;

    /* Wait for motor init */
    xSemaphoreTake(g_ctrlSyncSem, portMAX_DELAY);

    if (!motor_is_initialized()) {
        vTaskDelete(NULL);
    }

    /* Set board PID parameters (tune these for your car) */
    motor_send_pid(0.5f, 0.02f, 0.0f);

    /* Set target speed: 500 mm/s for both wheels (~ moderate speed) */
    motor_send_speed_mm_s(500.0f, 500.0f);

    TickType_t xLastWakeTime = xTaskGetTickCount();
    for (;;) {
        /* Update target speed each tick.  As the line-following controller
         * (or other planner) sets new speed commands, this loop sends them
         * to the board to keep its PID target current. */
        // TODO: replace hardcoded speed with line-following controller output
        // motor_send_speed_mm_s(target_left, target_right);
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(10));
    }
}
