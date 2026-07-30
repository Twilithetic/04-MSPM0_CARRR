/*
 *  ======== motor_driver_reg.h ========
 *  4-Way Motor Driver Board — shadow register (opaque type).
 *
 *  The full struct definition is in src/proxy/motor_reg.c.
 *  Client code uses only the accessor functions declared below.
 *
 *  Data flow:
 *    sync_encoder_from_device → UART read  → shadow register fields
 *    motor_send_speed / motor_send_pwm → UART write (direct, no shadow flush)
 *
 *  One global instance: g_motor_driver_reg (defined in src/proxy/motor_reg.c)
 */

#ifndef MOTOR_DRIVER_REG_H
#define MOTOR_DRIVER_REG_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Opaque type (full struct in motor_reg.c) ---- */
typedef struct MotorDriverReg MotorDriverReg;

extern MotorDriverReg g_motor_driver_reg;

/*
 *  Physical motors: M2 (right wheel) and M4 (left wheel) only.
 *  The UART protocol always sends/receives 4 motor slots —
 *  M1/M3 are filled with 0 on write and their encoder reads are ignored.
 *
 *  M2  = RIGHT wheel on the 4-way board
 *  M4  = LEFT wheel on the 4-way board
 */

/* ================================================================
 *  Read access (Client / Logger)
 * ================================================================ */

int32_t motor_get_encoder_left(void);
int32_t motor_get_encoder_right(void);
int16_t motor_get_encoder_10ms_left(void);
int16_t motor_get_encoder_10ms_right(void);
int16_t motor_get_speed_left(void);
int16_t motor_get_speed_right(void);
uint8_t  motor_get_comm_status(void);
uint8_t  motor_get_motor_type(void);
uint16_t motor_get_pulse_line(void);
uint16_t motor_get_reduction_ratio(void);
float    motor_get_wheel_diameter(void);
uint16_t motor_get_deadzone(void);
bool     motor_is_initialized(void);
uint16_t motor_get_sync_rate(void);       /* frames/sec from sync_encoder_from_device */
uint16_t motor_get_smooth_sync_rate(void); /* EMA-smoothed sync rate (by vStatsTask) */
float    motor_get_distance_left_mm(void);  /* encoder_total_left → travel distance (mm) */
float    motor_get_distance_right_mm(void); /* encoder_total_right → travel distance (mm) */
float    motor_get_speed_left_mm_s(void);   /* encoder 10ms delta → real-time speed (mm/s) */
float    motor_get_speed_right_mm_s(void);  /* encoder 10ms delta → real-time speed (mm/s) */

/* ================================================================
 *  I2C health check (motor_driver.c)
 * ================================================================ */

uint16_t motor_read_battery_voltage(void);

/* ================================================================
 *  UART1 direct output (for printf redirection, uart_init.c)
 * ================================================================ */

void motor_uart_putchar(char c);

/* ================================================================
 *  Write access (Proxy only — declared for motor_reg.c linkage)
 * ================================================================ */

void motor_set_encoder_left(int32_t val);
void motor_set_encoder_right(int32_t val);
void motor_set_encoder_10ms_left(int16_t val);
void motor_set_encoder_10ms_right(int16_t val);
void motor_set_speed_left(int16_t val);
void motor_set_speed_right(int16_t val);
void motor_set_comm_status(uint8_t status);
void motor_set_motor_type(uint8_t val);
void motor_set_pulse_line(uint16_t val);
void motor_set_reduction_ratio(uint16_t val);
void motor_set_wheel_diameter(float val);
void motor_set_deadzone(uint16_t val);
void motor_set_initialized(bool val);
void motor_set_distance_left_mm(float val);
void motor_set_distance_right_mm(float val);
void motor_set_speed_left_mm_s(float val);
void motor_set_speed_right_mm_s(float val);
void motor_set_sync_count(uint16_t val);
void motor_add_sync_count(uint16_t n);

/* encoder → travel distance + speed mm/s (defined in src/driver/board/motor_driver_uart.c) */
void motor_update_derived(void);

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_DRIVER_REG_H */
