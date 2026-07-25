/*
 *  ======== motor.h ========
 *  Motor driver (I2C bit-bang) hardware proxy.
 *
 *  sync_*_from_device(): I2C read → write motor shadow register (no return)
 *  flush_*_to_device():   read motor shadow target_* → I2C write (no params)
 *  cmd_*():              fixed-value device commands (init etc.)
 *
 *  PRIVATE (raw_*) layer is file-scope static in motor.c — NOT exposed.
 */

#ifndef MOTOR_H
#define MOTOR_H

#include "include/motor_reg.h"

/// Motor proxy — carries only I2C addressing info (no state)
typedef struct {
    uint8_t i2c_addr;   // I2C slave address (0x26)
} MotorProxy;

/* ---- SYNC: read I2C → write shadow (no return value) ---- */

/// Read encoder totals + 10ms deltas from I2C → g_motor_reg
void sync_encoder_from_device(const MotorProxy *p, MotorReg *r);

/// Read config registers from motor driver → g_motor_reg
void sync_config_from_device(const MotorProxy *p, MotorReg *r);

/* ---- FLUSH: read g_motor_reg target_* → write I2C (no value params) ---- */
/* Caller MUST write r->target_* fields BEFORE calling flush.            */

/// Read target_speed_m1..m4 → I2C write speed register
void flush_speed_to_device(const MotorProxy *p, MotorReg *r);

/// Read target_pwm_m1..m4 → I2C write PWM register
void flush_pwm_to_device(const MotorProxy *p, MotorReg *r);

/// Zero all target_* in shadow + flush both speed & PWM
void flush_stop_to_device(const MotorProxy *p, MotorReg *r);

/* ---- CMD: device commands (fixed values — one-time init) ---- */

/// Full TT encoder init sequence (5 register writes with delays)
void cmd_config_tt_encoder(const MotorProxy *p);

/* ---- Init: I2C bus idle + stop motors ---- */

void motor_proxy_init(const MotorProxy *p);

#endif /* MOTOR_H */
