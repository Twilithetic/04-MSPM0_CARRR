/*
 *  ======== motor_driver_reg.h ========
 *  4-Way Motor Driver Board — shadow register types.
 *
 *  Data flow:
 *    sync_encoder_from_device -> motor proxy (UART read)  writes these fields
 *    flush_*_to_device        -> motor proxy (UART write) reads  target_* fields
 *
 *  One global instance: g_motor_driver_reg (defined in src/proxy/registers.c)
 */

#ifndef MOTOR_DRIVER_REG_H
#define MOTOR_DRIVER_REG_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Motor Driver Shadow Register ----
 *
 *  Physical motors: M4 (left wheel) and M2 (right wheel) only.
 *  The I2C board protocol always sends/receives 4 motor slots —
 *  M1/M3 are filled with 0 on write and their encoder reads are ignored.
 *
 *  M_LEFT   = M4 on the 4-way board
 *  M_RIGHT  = M2 on the 4-way board
 */
typedef struct {
    /* sync_encoder_from_device: updated by motor proxy reading UART ---- */
    volatile int32_t encoder_total_left;   // ticks, accumulated (M4)
    volatile int32_t encoder_total_right;  // ticks, accumulated (M2)
    volatile int16_t encoder_10ms_left;    // ticks/10ms, delta (M4)
    volatile int16_t encoder_10ms_right;   // ticks/10ms, delta (M2)
    volatile uint8_t  comm_status;         // 0 = OK, >0 = error step

    /* ---- Config readback (sync_encoder_from_device) ---- */
    volatile uint8_t  motor_type;          // 3 = TT encoder
    volatile uint16_t pulse_line;          // encoder lines per revolution (13)
    volatile uint16_t reduction_ratio;     // gear reduction ratio * 1 (45)
    volatile float    wheel_diameter;      // mm (67.0)
    volatile uint16_t deadzone;            // PWM deadzone threshold (1250)

    /* flush_*_to_device: written by controller, flushed by motor proxy ---- */
    volatile int16_t target_speed_left;    // M2 target speed
    volatile int16_t target_speed_right;   // M4 target speed
    volatile int16_t target_pwm_left;      // M2 target PWM
    volatile int16_t target_pwm_right;     // M4 target PWM

    /* flags */
    volatile bool    initialized;          // true after init succeeds
} MotorDriverReg;

extern MotorDriverReg g_motor_driver_reg;

/* ================================================================
 *  Read access (Client / Logger)
 * ================================================================ */

int32_t motor_get_encoder_left(void);
int32_t motor_get_encoder_right(void);
int16_t motor_get_encoder_10ms_left(void);
int16_t motor_get_encoder_10ms_right(void);
uint8_t  motor_get_comm_status(void);
uint8_t  motor_get_motor_type(void);
uint16_t motor_get_pulse_line(void);
uint16_t motor_get_reduction_ratio(void);
float    motor_get_wheel_diameter(void);
uint16_t motor_get_deadzone(void);
bool     motor_is_initialized(void);

/* ================================================================
 *  I2C health check (motor_driver.c)
 * ================================================================ */

uint16_t motor_read_battery_voltage(void);

/* ================================================================
 *  UART1 direct output (for printf redirection, uart_init.c)
 * ================================================================ */

void motor_uart_putchar(char c);

/* ================================================================
 *  Write access (Proxy only — declared for registers.c linkage)
 * ================================================================ */

void motor_set_encoder_left(int32_t val);
void motor_set_encoder_right(int32_t val);
void motor_set_encoder_10ms_left(int16_t val);
void motor_set_encoder_10ms_right(int16_t val);
void motor_set_comm_status(uint8_t status);
void motor_set_motor_type(uint8_t val);
void motor_set_pulse_line(uint16_t val);
void motor_set_reduction_ratio(uint16_t val);
void motor_set_wheel_diameter(float val);
void motor_set_deadzone(uint16_t val);
void motor_set_initialized(bool val);

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_DRIVER_REG_H */
