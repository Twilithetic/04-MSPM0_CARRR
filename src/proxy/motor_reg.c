/*
 *  ======== motor_reg.c ========
 *  4-Way Motor Driver Board shadow register — struct definition,
 *  global instance, and all accessors in one file for easy reading.
 */

#include "include/motor_driver_reg.h"

#include <FreeRTOS.h>
#include <task.h>
#include <stdint.h>

/* ---- Full struct (private — opaque to client) ----
 *
 *  Physical motors: M2 (right wheel) and M4 (left wheel) only.
 *  The UART protocol always sends/receives 4 motor slots —
 *  M1/M3 are filled with 0 on write and their encoder reads are ignored.
 *
 *  M2  = RIGHT wheel on the 4-way board
 *  M4  = LEFT wheel on the 4-way board
 */
struct MotorDriverReg {
    /* sync_encoder_from_device: updated by UART read ---- */
    volatile int32_t encoder_total_left;   // ticks, accumulated (M4)
    volatile int32_t encoder_total_right;  // ticks, accumulated (M2)
    volatile int16_t encoder_10ms_left;    // ticks/10ms, delta (M4)
    volatile int16_t encoder_10ms_right;   // ticks/10ms, delta (M2)
    volatile int16_t speed_left;           // actual speed (M4), from $MSPD
    volatile int16_t speed_right;          // actual speed (M2), from $MSPD
    volatile uint8_t  comm_status;         // 0 = OK, >0 = error step

    /* ---- Config readback ---- */
    volatile uint8_t  motor_type;          // 3 = TT encoder
    volatile uint16_t pulse_line;          // encoder lines per revolution (13)
    volatile uint16_t reduction_ratio;     // gear reduction ratio * 1 (45)
    volatile float    wheel_diameter;      // mm (67.0)
    volatile uint16_t deadzone;            // PWM deadzone threshold (1250)

    /* flags */
    volatile bool    initialized;          // true after init succeeds

    /* derived: encoder → travel distance (computed in vMotorSyncTask) */
    volatile float    distance_left_mm;     // travel distance (mm), left wheel
    volatile float    distance_right_mm;    // travel distance (mm), right wheel

    /* sync statistics (qps-style) */
    volatile uint16_t sync_count;         // total frame count since init
    volatile uint32_t last_sync_tick;     // FreeRTOS tick of last rate snap
    volatile uint16_t last_sync_count;    // sync_count at last snap
    volatile uint16_t sync_rate;          // frames/second, computed
};

MotorDriverReg g_motor_driver_reg = {0};

/* ── Read access ── */

int32_t motor_get_encoder_left(void)   { return g_motor_driver_reg.encoder_total_left; }
int32_t motor_get_encoder_right(void)  { return g_motor_driver_reg.encoder_total_right; }
int16_t motor_get_encoder_10ms_left(void)   { return g_motor_driver_reg.encoder_10ms_left; }
int16_t motor_get_encoder_10ms_right(void)  { return g_motor_driver_reg.encoder_10ms_right; }
int16_t motor_get_speed_left(void)          { return g_motor_driver_reg.speed_left; }
int16_t motor_get_speed_right(void)         { return g_motor_driver_reg.speed_right; }

uint8_t  motor_get_comm_status(void)         { return g_motor_driver_reg.comm_status; }
uint8_t  motor_get_motor_type(void)          { return g_motor_driver_reg.motor_type; }
uint16_t motor_get_pulse_line(void)          { return g_motor_driver_reg.pulse_line; }
uint16_t motor_get_reduction_ratio(void)     { return g_motor_driver_reg.reduction_ratio; }
float    motor_get_wheel_diameter(void)      { return g_motor_driver_reg.wheel_diameter; }
uint16_t motor_get_deadzone(void)            { return g_motor_driver_reg.deadzone; }
bool     motor_is_initialized(void)          { return g_motor_driver_reg.initialized; }

uint16_t motor_get_sync_rate(void)
{
    uint32_t now = xTaskGetTickCount();
    uint32_t dt  = now - g_motor_driver_reg.last_sync_tick;
    uint16_t ds  = g_motor_driver_reg.sync_count - g_motor_driver_reg.last_sync_count;

    g_motor_driver_reg.last_sync_tick  = now;
    g_motor_driver_reg.last_sync_count = g_motor_driver_reg.sync_count;

    if (dt == 0) return 0;
    return (uint16_t)(((uint32_t)ds * configTICK_RATE_HZ) / dt);
}

float motor_get_distance_left_mm(void)
{
    return g_motor_driver_reg.distance_left_mm;
}

float motor_get_distance_right_mm(void)
{
    return g_motor_driver_reg.distance_right_mm;
}

/* ── Write access (Proxy only) ── */

void motor_set_encoder_left(int32_t val)    { g_motor_driver_reg.encoder_total_left = val; }
void motor_set_encoder_right(int32_t val)   { g_motor_driver_reg.encoder_total_right = val; }
void motor_set_encoder_10ms_left(int16_t val)   { g_motor_driver_reg.encoder_10ms_left = val; }
void motor_set_encoder_10ms_right(int16_t val)  { g_motor_driver_reg.encoder_10ms_right = val; }
void motor_set_speed_left(int16_t val)          { g_motor_driver_reg.speed_left = val; }
void motor_set_speed_right(int16_t val)         { g_motor_driver_reg.speed_right = val; }

void motor_set_comm_status(uint8_t status)    { g_motor_driver_reg.comm_status = status; }
void motor_set_motor_type(uint8_t val)        { g_motor_driver_reg.motor_type = val; }
void motor_set_pulse_line(uint16_t val)       { g_motor_driver_reg.pulse_line = val; }
void motor_set_reduction_ratio(uint16_t val)  { g_motor_driver_reg.reduction_ratio = val; }
void motor_set_wheel_diameter(float val)      { g_motor_driver_reg.wheel_diameter = val; }
void motor_set_deadzone(uint16_t val)         { g_motor_driver_reg.deadzone = val; }
void motor_set_initialized(bool val)          { g_motor_driver_reg.initialized = val; }

void motor_set_distance_left_mm(float val)    { g_motor_driver_reg.distance_left_mm = val; }
void motor_set_distance_right_mm(float val)   { g_motor_driver_reg.distance_right_mm = val; }

void motor_set_sync_count(uint16_t val)       { g_motor_driver_reg.sync_count = val; }
void motor_add_sync_count(uint16_t n)         { g_motor_driver_reg.sync_count += n; }
