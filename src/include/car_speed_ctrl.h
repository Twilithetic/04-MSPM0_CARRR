/*
 *  ======== car_speed_ctrl.h ========
 *  Two-wheel speed controller — MCU-side PID, PWM output only.
 *
 *  Control chain:
 *    car_ctrl_set_target_speed()  →  sets target speed (mm/s)
 *    vCarCtrlTask (10ms period)   →  reads encoder_10ms → PID → PWM
 *                                  → writes target_pwm_* → flush_pwm_to_device()
 *
 *  PWM output range: -7200 .. +7200  (driver board int16, deadzone 1250)
 *  PWM_MAX limits output to ±5000 for safety.
 */

#ifndef CAR_SPEED_CTRL_H
#define CAR_SPEED_CTRL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Target speed (called by planner / controller) ---- */
void car_ctrl_set_target_speed(float left_mm_s, float right_mm_s);
void car_ctrl_set_target_speed_left(float mm_s);
void car_ctrl_set_target_speed_right(float mm_s);

/* ---- Read current target ---- */
float car_ctrl_get_target_speed_left(void);
float car_ctrl_get_target_speed_right(void);

/* ---- Direct PWM override (bypasses PID, for testing) ---- */
void car_ctrl_set_pwm(int16_t left, int16_t right);

/* ---- Stop ---- */
void car_ctrl_stop(void);

/* ---- PID tuning (runtime) ---- */
void car_ctrl_set_pid(float kp, float ki, float kd);
void car_ctrl_set_pwm_limit(int16_t max_pwm);

/* ---- Feedback ---- */
float    car_ctrl_get_distance_left_mm(void);
float    car_ctrl_get_distance_right_mm(void);
float    car_ctrl_get_speed_left_mm_s(void);
float    car_ctrl_get_speed_right_mm_s(void);
int16_t  car_ctrl_get_pwm_left(void);
int16_t  car_ctrl_get_pwm_right(void);
int32_t  car_ctrl_get_encoder_total_left(void);
int32_t  car_ctrl_get_encoder_total_right(void);
int16_t  car_ctrl_get_encoder_10ms_left(void);
int16_t  car_ctrl_get_encoder_10ms_right(void);

/* ---- Periodic PID step (called by vCarCtrlTask every 10ms) ---- */
void car_ctrl_pid_tick(void);

#ifdef __cplusplus
}
#endif

#endif /* CAR_SPEED_CTRL_H */
