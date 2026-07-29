/*
 *  ======== car_speed_ctrl.c ========
 *  Two-wheel speed controller — MCU-side PID, PWM output only.
 *
 *  vCarCtrlTask runs at 100 Hz (10ms), locked to the same tick as
 *  vMotorSyncTask so encoder_10ms is always fresh.
 *
 *  Control chain:
 *    target_speed (mm/s) → PID → PWM (-5000 .. +5000) → flush_pwm_to_device()
 *
 *  Constants:
 *    2340 counts/wheel-rev, wheel diameter = 68.5mm
 *    10ms-encoder → mm/s:  × 9.198
 *    mm/s → 10ms-encoder:  ÷ 9.198
 */

#include "include/car_speed_ctrl.h"
#include "include/motor_driver_reg.h"
#include "include/motor_driver_uart.h"

#include <stdbool.h>

/* ================================================================
 *  Hardware constants
 * ================================================================ */

#define ENCODER_CPR           2340U
#define WHEEL_DIAMETER_MM     68.5f
#define MM_PER_COUNT          0.09198f                       /* PI * 68.5 / 2340          */
#define COUNT10MS_TO_MM_S(c)  ((float)(c) * 9.198f)         /* cts/10ms → mm/s          */
#define MM_S_TO_COUNT10MS(v)  ((float)(v) / 9.198f)         /* mm/s → cts/10ms          */

/* ================================================================
 *  PWM limits — driver board accepts int16 [-7200, +7200]
 *  We clamp to ±5000 for safety; deadzone is 1250.
 * ================================================================ */

#define PWM_LIMIT       5000
#define PWM_DEADZONE    1250

/* ================================================================
 *  PID parameters (runtime-tunable)
 * ================================================================ */

static float g_kp = 60.0f;    /* proportional: PWM / (cts/10ms error) */
static float g_ki = 8.0f;     /* integral */
static float g_kd = 2.0f;     /* derivative */
static int16_t g_pwm_limit = PWM_LIMIT;

/* ================================================================
 *  PID state — per wheel
 * ================================================================ */

typedef struct {
    float target_mm_s;     /* desired speed in mm/s */
    float integral;        /* accumulated error * ki     */
    float prev_error;      /* last error (for D term)     */
    int16_t output;        /* last PWM output             */
    float measured_mm_s;   /* last measured speed (mm/s)  */
} WheelPID;

static WheelPID g_pid_left  = {0};
static WheelPID g_pid_right = {0};

/* ================================================================
 *  Target speed setter (called by planner / controller)
 * ================================================================ */

void car_ctrl_set_target_speed(float left_mm_s, float right_mm_s)
{
    g_pid_left.target_mm_s  = left_mm_s;
    g_pid_right.target_mm_s = right_mm_s;
}

void car_ctrl_set_target_speed_left(float mm_s)
{
    g_pid_left.target_mm_s = mm_s;
}

void car_ctrl_set_target_speed_right(float mm_s)
{
    g_pid_right.target_mm_s = mm_s;
}

float car_ctrl_get_target_speed_left(void)
{
    return g_pid_left.target_mm_s;
}

float car_ctrl_get_target_speed_right(void)
{
    return g_pid_right.target_mm_s;
}

/* ================================================================
 *  Direct PWM (bypass PID)
 * ================================================================ */

void car_ctrl_set_pwm(int16_t left, int16_t right)
{
    g_motor_driver_reg.target_pwm_left  = left;
    g_motor_driver_reg.target_pwm_right = right;
    flush_pwm_to_device(&g_motor_driver_reg);
}

/* ================================================================
 *  Stop
 * ================================================================ */

void car_ctrl_stop(void)
{
    g_pid_left.target_mm_s   = 0.0f;
    g_pid_right.target_mm_s  = 0.0f;
    g_pid_left.integral      = 0.0f;
    g_pid_right.integral     = 0.0f;
    g_pid_left.prev_error    = 0.0f;
    g_pid_right.prev_error   = 0.0f;
    flush_stop_to_device(&g_motor_driver_reg);
}

/* ================================================================
 *  PID tuning
 * ================================================================ */

void car_ctrl_set_pid(float kp, float ki, float kd)
{
    g_kp = kp;
    g_ki = ki;
    g_kd = kd;
}

void car_ctrl_set_pwm_limit(int16_t max_pwm)
{
    if (max_pwm < 0) max_pwm = -max_pwm;
    if (max_pwm > 7200) max_pwm = 7200;
    g_pwm_limit = max_pwm;
}

/* ================================================================
 *  PID step — run ONE wheel
 * ================================================================ */

static int16_t pid_step(WheelPID *w, float measured_mm_s)
{
    /* Error: positive = too slow → increase PWM (positive = forward) */
    float error = w->target_mm_s - measured_mm_s;

    /* Integral — anti-windup: clamp before accumulating */
    float p_term = g_kp * error;

    w->integral += error;
    /* Clamp integral so it + P_term doesn't exceed PWM_LIMIT */
    float i_limit = (float)(g_pwm_limit) / (g_ki > 0.001f ? g_ki : 1.0f);
    if (w->integral >  i_limit) w->integral =  i_limit;
    if (w->integral < -i_limit) w->integral = -i_limit;
    float i_term = g_ki * w->integral;

    /* Derivative on measurement (not error) to avoid derivative kick */
    float d_term = g_kd * (0.0f - (measured_mm_s - w->measured_mm_s));
    /* Alternative: D on error:  float d_term = g_kd * (error - w->prev_error); */

    /* Reset integral on zero target (prevents windup when stopped) */
    if (w->target_mm_s == 0.0f) {
        w->integral = 0.0f;
    }

    float pid_out = p_term + i_term + d_term;

    /* Clamp to PWM limits */
    if (pid_out > (float)(g_pwm_limit))  pid_out = (float)(g_pwm_limit);
    if (pid_out < (float)(-g_pwm_limit)) pid_out = (float)(-g_pwm_limit);

    /* Deadzone: if output is within deadzone, output 0 */
    int16_t pwm = (int16_t)pid_out;
    if (pwm > -PWM_DEADZONE && pwm < PWM_DEADZONE && w->target_mm_s == 0.0f) {
        pwm = 0;
    }

    w->measured_mm_s = measured_mm_s;
    w->prev_error    = error;
    w->output        = pwm;

    return pwm;
}

/* ================================================================
 *  PID tick — called every 10ms by vCarCtrlTask
 * ================================================================ */

void car_ctrl_pid_tick(void)
{
    /* Read fresh encoder data (updated by vMotorSyncTask, same 10ms tick) */
    float speed_left  = COUNT10MS_TO_MM_S(motor_get_encoder_10ms_left());
    float speed_right = COUNT10MS_TO_MM_S(motor_get_encoder_10ms_right());

    int16_t pwm_left  = pid_step(&g_pid_left,  speed_left);
    int16_t pwm_right = pid_step(&g_pid_right, speed_right);

    /* Write + flush */
    g_motor_driver_reg.target_pwm_left  = pwm_left;
    g_motor_driver_reg.target_pwm_right = pwm_right;
    flush_pwm_to_device(&g_motor_driver_reg);
}

/* ================================================================
 *  Feedback
 * ================================================================ */

float car_ctrl_get_distance_left_mm(void)
{
    return motor_get_distance_left_mm();
}

float car_ctrl_get_distance_right_mm(void)
{
    return motor_get_distance_right_mm();
}

float car_ctrl_get_speed_left_mm_s(void)
{
    return g_pid_left.measured_mm_s;
}

float car_ctrl_get_speed_right_mm_s(void)
{
    return g_pid_right.measured_mm_s;
}

int32_t car_ctrl_get_encoder_total_left(void)
{
    return motor_get_encoder_left();
}

int32_t car_ctrl_get_encoder_total_right(void)
{
    return motor_get_encoder_right();
}

int16_t car_ctrl_get_encoder_10ms_left(void)
{
    return motor_get_encoder_10ms_left();
}

int16_t car_ctrl_get_encoder_10ms_right(void)
{
    return motor_get_encoder_10ms_right();
}
