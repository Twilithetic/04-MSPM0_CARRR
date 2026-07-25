/*
 *  ======== motor_reg.h ========
 *  Motor telemetry and control shadow registers.
 *  All fields are volatile — accessed by main loop and proxy.
 *
 *  Data flow:
 *    sync_from_device -> motor proxy (I2C read)  writes these fields
 *    flush_to_device  -> motor proxy (I2C write) reads  target_* fields
 */

#ifndef MOTOR_REG_H
#define MOTOR_REG_H

#include "ti_msp_dl_config.h"
#include <stdbool.h>
#include <stdint.h>

/// Motor shadow register (one global instance: g_motor_reg)
typedef struct {
    /* ---- sync_from_device: updated by motor proxy reading I2C ---- */
    volatile int32_t encoder_total[4];   // ticks, accumulated encoder count M1-M4
    volatile int16_t encoder_10ms[4];    // ticks/10ms, delta encoder count M1-M4
    volatile uint16_t motor_type;        // enum (3 = TT encoder)
    volatile uint16_t pulse_line;        // count, encoder lines per revolution
    volatile uint16_t reduction_ratio;   // ratio*1, gear reduction
    volatile float    wheel_diameter;    // mm, wheel diameter
    volatile uint16_t deadzone;          // raw, PWM deadzone threshold
    volatile uint8_t  comm_status;       // 0 = OK, >0 = I2C error step

    /* ---- flush_to_device: written by controller, flushed by motor proxy ---- */
    volatile int16_t target_speed_m1;    // target motor 1 speed
    volatile int16_t target_speed_m2;    // target motor 2 speed (left wheel)
    volatile int16_t target_speed_m3;    // target motor 3 speed
    volatile int16_t target_speed_m4;    // target motor 4 speed (right wheel)
    volatile int16_t target_pwm_m1;      // target motor 1 PWM
    volatile int16_t target_pwm_m2;      // target motor 2 PWM
    volatile int16_t target_pwm_m3;      // target motor 3 PWM
    volatile int16_t target_pwm_m4;      // target motor 4 PWM
} MotorReg;

extern MotorReg g_motor_reg;

#endif /* MOTOR_REG_H */
