/*
 *  ======== task_control.c ========
 *  Control tasks — car speed / steering.
 *
 *  vCarCtrlTask — speed sweep test.
 *    Ramps target speed from 0 → 500 mm/s in 20 mm/s steps.
 *    5s per step.  Uses board internal PID (closed-loop speed control).
 *    Deadzone found at PWM=1900 — speed commands below ~deadzone
 *    may produce no motion; this sweep maps target → actual response.
 */

#include "include/app_tasks.h"
#include "include/motor_driver_uart.h"
#include "include/motor_driver_reg.h"
#include "include/XDS110_cdc.h"

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <stdio.h>
#include <line8_reg.h>

/* ---- Semaphores ---- */
extern SemaphoreHandle_t g_ctrlSyncSem;

#define SPEED_STEP_MM_S  20.0f   /* mm/s per step       */
#define SPEED_STEP_SEC   5U      /* seconds per step    */
#define SPEED_MAX        500.0f  /* upper limit (mm/s)  */

/* ── Car Speed Control Task (prio 3): 100Hz ── */
void vCarCtrlTask(void *pvParameters)
{
    (void) pvParameters;

    xSemaphoreTake(g_ctrlSyncSem, portMAX_DELAY);

    if (!motor_is_initialized()) {
        vTaskDelete(NULL);
    }

    /* Board PID */
    motor_send_pid(0.5f, 0.02f, 0.0f);

    /* Kill any PWM override — use speed loop only */
    motor_send_pwm(0, 0, 0, 0);

    TickType_t xLastWakeTime = xTaskGetTickCount();
    float speed_xunhang = 200.0f;

    for (;;) {
        g_line8_reg

        motor_send_speed_mm_s(target, target);

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(10));
    }
}
