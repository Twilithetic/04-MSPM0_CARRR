/*
 *  ======== task_control.c ========
 *  Control tasks — car speed / steering.
 *
 *  vCarCtrlTask — line-following with two independent PIDs @ 100Hz.
 *    Left and right sides each have their own PID loop:
 *
 *      err_left   = SIDE_TARGET - left_white_val
 *      err_right  = SIDE_TARGET - right_white_val
 *      speed_left  = BASE - PID_left(err_left)
 *      speed_right = BASE - PID_right(err_right)
 *
 *    white_val is 0-30 per side (higher = whiter = less line).
 *    SIDE_TARGET is the white level when the line is centered
 *    (inner sensors, weight 16, sit on the line → 30-16 = 14).
 *
 *    Line drifts left → left side darker → left_white_val drops →
 *    err_left > 0 → left wheel slows; right side gets whiter →
 *    err_right < 0 → right wheel speeds up → car turns left.
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

/* ---- Controller state (read by logger) ---- */
float g_ctrl_speed_left  = 0.0f;
float g_ctrl_speed_right = 0.0f;
float g_ctrl_err_left    = 0.0f;
float g_ctrl_err_right   = 0.0f;

/* ---- Line-follow setpoint & limits ---- */
#define LF_SIDE_TARGET  0.0f   /* white-val per side when line centered */
#define LF_BASE_SPEED   500.0f  /* mm/s cruise speed                     */
#define LF_MAX_SPEED    1000.0f  /* mm/s per-wheel clamp                  */

/* ---- Left PID gains (left_white_val → speed_left) ---- */
#define LF_L_KP  0.0f
#define LF_L_KI  1.0f
#define LF_L_KD  0.0f

/* ---- Right PID gains (right_white_val → speed_right) ---- */
#define LF_R_KP  0.0f
#define LF_R_KI  1.0f
#define LF_R_KD  0.0f

/* Independent PID state, one per side */
typedef struct {
    float err_prev;
    float err_integ;
} LinePid;

static float line_pid_run(LinePid *p, float err, float kp, float ki, float kd)
{
    p->err_integ += err;
    float out = kp * err
              + ki * p->err_integ
              + kd * (err - p->err_prev);
    p->err_prev = err;
    return out;
}

static float clamp_speed(float mm_s)
{
    if (mm_s < 0.0f)         return 0.0f;
    if (mm_s > LF_MAX_SPEED) return LF_MAX_SPEED;
    return mm_s;
}

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

    LinePid pid_left  = {0};
    LinePid pid_right = {0};

    for (;;) {
        // /* Read side-encoded white values from the shadow register */
        // g_ctrl_err_left  = LF_SIDE_TARGET - (float) line8_get_left_white_val();
        // g_ctrl_err_right = LF_SIDE_TARGET - (float) line8_get_right_white_val();

        // /* Two independent PIDs — one per wheel */
        // g_ctrl_speed_left  = LF_BASE_SPEED
        //                    - line_pid_run(&pid_left,  g_ctrl_err_left,  LF_L_KP, LF_L_KI, LF_L_KD);
        // g_ctrl_speed_right = LF_BASE_SPEED
        //                    - line_pid_run(&pid_right, g_ctrl_err_right, LF_R_KP, LF_R_KI, LF_R_KD);

        // motor_send_speed_mm_s(clamp_speed(g_ctrl_speed_left) / 3, clamp_speed(g_ctrl_speed_right) / 3);
        motor_send_speed_mm_s(500,500);
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(10));
    }
}
