/*
 *  ======== task_control.c ========
 *  Control tasks — car speed / steering.
 *
 *  vCarCtrlTask — line-following PID @ 100Hz.
 *    Single PID on the left/right lit-LED difference:
 *
 *      err         = left_white_val - right_white_val
 *      corr        = Kp*err + Ki*∫err + Kd*Δerr
 *      speed_left  = BASE - corr
 *      speed_right = BASE + corr
 *
 *    white_val: 0-30 per side, counts LIT LEDs (raw==1, off line).
 *    Centered on track: both sides equally lit → err = 0 → straight.
 *
 *    Car drifts LEFT off track:
 *      left side darker → left_white_val drops, right brighter →
 *      err < 0 → corr < 0 → left wheel speeds up, right wheel slows
 *      → car pulls back to the RIGHT, onto the line.
 *
 *    Wheel speed closed loop runs on the motor driver board
 *    (board PID set via motor_send_pid).
 */

#include "include/app_tasks.h"
#include "include/motor_driver_uart.h"
#include "include/motor_driver_reg.h"
#include "include/line8_reg.h"
#include "include/XDS110_cdc.h"

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <stdio.h>

/* ---- Semaphores ---- */
extern SemaphoreHandle_t g_ctrlSyncSem;

/* ---- Controller state (read by logger task) ---- */
float g_ctrl_speed_left  = 0.0f;
float g_ctrl_speed_right = 0.0f;
float g_ctrl_err_left    = 0.0f;
float g_ctrl_err_right   = 0.0f;

/* ---- Line-follow PID (tune on track) ---- */
#define LF_KP          5.0f    /* mm/s per unit of left/right imbalance */
#define LF_KI          0.5f
#define LF_KD          0.0f
#define LF_BASE_SPEED  200.0f  /* mm/s cruise speed                     */
#define LF_MAX_SPEED   500.0f  /* mm/s per-wheel clamp                  */

/* ── Car Speed Control Task (prio 3): 100Hz ── */
void vCarCtrlTask(void *pvParameters)
{
    (void) pvParameters;

    xSemaphoreTake(g_ctrlSyncSem, portMAX_DELAY);

    if (!motor_is_initialized()) {
        vTaskDelete(NULL);
    }

    /* Board PID */
    motor_send_pid(0.8f, 0.06f, 0.5f);

    /* Kill any PWM override — use speed loop only */
    motor_send_pwm(0, 0, 0, 0);

    TickType_t xLastWakeTime = xTaskGetTickCount();

    float err_prev  = 0.0f;
    float err_integ = 0.0f;

    for (;;) {
        /* Left/right lit-LED difference from the shadow register */
        float err = (float) line8_get_left_white_val()
                  - (float) line8_get_right_white_val();

        /* PID on the imbalance */
        err_integ += err / 100.0f;  /* integral over 1 sec (100Hz) */
        float corr = LF_KP * err
                   + LF_KI * err_integ
                   + LF_KD * (err - err_prev);
        err_prev = err;

        float speed_left  = LF_BASE_SPEED + corr;
        float speed_right = LF_BASE_SPEED - corr;

        /* Clamp: no reverse, cap top speed */
        if (speed_left  < 0.0f)         speed_left  = 0.0f;
        if (speed_left  > LF_MAX_SPEED) speed_left  = LF_MAX_SPEED;
        if (speed_right < 0.0f)         speed_right = 0.0f;
        if (speed_right > LF_MAX_SPEED) speed_right = LF_MAX_SPEED;

        /* Publish state for the logger (err shown per side, ±same imbalance) */
        g_ctrl_err_left    = err;
        g_ctrl_err_right   = -err;
        g_ctrl_speed_left  = speed_left;
        g_ctrl_speed_right = speed_right;

        motor_send_speed_mm_s(speed_left, speed_right);

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(10));
    }
}
