/*
 *  ======== registers.c ========
 *  Single compilation unit that defines all shadow register global instances,
 *  their full struct layouts, and accessors.
 */

#include "include/motor_reg.h"
#include "include/line_reg.h"
#include "include/status_reg.h"
#include "include/led_reg.h"
#include "include/i2c_scanner_reg.h"

#include <stdbool.h>
#include <stdint.h>

// =====================================================================
//  MotorReg   (full definition in motor_reg.h — not changed)
// =====================================================================
MotorReg  g_motor_reg  = {0};

// =====================================================================
//  LineReg    (full definition in line_reg.h — not changed)
// =====================================================================
LineReg   g_line_reg   = {0};

// =====================================================================
//  StatusReg  FULL struct definition — private to this TU
// =====================================================================
struct StatusReg {
    /* ---- Status flags set by controller / main loop ---- */
    bool    line_lost;           // all sensors see white (lost)
    bool    line_all_black;      // >=4 sensors see black (cross/stop)
    bool    motor_error;         // true if last motor I2C failed
    uint8_t motor_error_code;    // detail: I2C error step (>0 = fail)
    uint16_t loop_count;         // main loop iteration counter
    bool    initialized;         // motor config completed successfully
    bool    button_pressed;      // KEY pin (PA18) state

    /* ---- Target / command flags ---- */
    bool    target_motor_stop;   // emergency motor stop request
};

StatusReg g_status_reg = {0};

// =====================================================================
//  LedReg  — LED debug counters, standalone
// =====================================================================
struct LedReg {
    uint32_t blue_toggle_count;
    uint32_t green_toggle_count;
};

LedReg g_led_reg = {0};

// =====================================================================
//  I2cScanReg  — I2C bus-scan result shadow register
// =====================================================================
struct I2cScanReg {
    uint8_t  count;                              /* number of devices found */
    uint8_t  addr[I2C_SCAN_MAX_DEVICES];         /* 7-bit address */
    uint8_t  whoami[I2C_SCAN_MAX_DEVICES];       /* WHO_AM_I byte (0xFF if N/A) */
    volatile bool ack_map[I2C_SCAN_ADDR_RANGE];  /* true if addr ACKed */
};

I2cScanReg g_i2c_scan_reg = {0};

/* ── count ── */
uint8_t i2c_scan_get_count(void)                     { return g_i2c_scan_reg.count; }
void    i2c_scan_set_count(uint8_t cnt)              { g_i2c_scan_reg.count = cnt; }

/* ── per-device ── */
uint8_t i2c_scan_get_addr(uint8_t idx)
{
    if (idx >= I2C_SCAN_MAX_DEVICES) { return 0; }
    return g_i2c_scan_reg.addr[idx];
}

uint8_t i2c_scan_get_whoami(uint8_t idx)
{
    if (idx >= I2C_SCAN_MAX_DEVICES) { return 0xFF; }
    return g_i2c_scan_reg.whoami[idx];
}

void i2c_scan_add_device(uint8_t addr, uint8_t whoami)
{
    uint8_t idx = g_i2c_scan_reg.count;
    if (idx >= I2C_SCAN_MAX_DEVICES) { return; }
    g_i2c_scan_reg.addr[idx]   = addr;
    g_i2c_scan_reg.whoami[idx] = whoami;
    g_i2c_scan_reg.ack_map[addr] = true;
    g_i2c_scan_reg.count = (uint8_t)(idx + 1);
}

void i2c_scan_clear_all(void)
{
    uint16_t i;
    g_i2c_scan_reg.count = 0;
    for (i = 0; i < I2C_SCAN_MAX_DEVICES; i++) {
        g_i2c_scan_reg.addr[i]   = 0;
        g_i2c_scan_reg.whoami[i] = 0;
    }
    for (i = 0; i < I2C_SCAN_ADDR_RANGE; i++) {
        g_i2c_scan_reg.ack_map[i] = false;
    }
}

bool i2c_scan_get_ack(uint8_t addr)
{
    if (addr >= I2C_SCAN_ADDR_RANGE) { return false; }
    return g_i2c_scan_reg.ack_map[addr];
}

void i2c_scan_set_ack(uint8_t addr, bool present)
{
    if (addr >= I2C_SCAN_ADDR_RANGE) { return; }
    g_i2c_scan_reg.ack_map[addr] = present;
}

// =====================================================================
//  StatusReg accessors  (unchanged)
// =====================================================================

// ---- line_lost ----
void status_set_line_lost(StatusReg *reg, bool val)          { reg->line_lost = val; }
bool status_get_line_lost(const StatusReg *reg)              { return reg->line_lost; }

// ---- line_all_black ----
void status_set_line_all_black(StatusReg *reg, bool val)     { reg->line_all_black = val; }
bool status_get_line_all_black(const StatusReg *reg)         { return reg->line_all_black; }

// ---- motor_error ----
void status_set_motor_error(StatusReg *reg, bool val)        { reg->motor_error = val; }
bool status_get_motor_error(const StatusReg *reg)            { return reg->motor_error; }

// ---- motor_error_code ----
void status_set_motor_error_code(StatusReg *reg, uint8_t val) { reg->motor_error_code = val; }
uint8_t status_get_motor_error_code(const StatusReg *reg)     { return reg->motor_error_code; }

// ---- loop_count ----
void     status_set_loop_count(StatusReg *reg, uint16_t val)  { reg->loop_count = val; }
uint16_t status_get_loop_count(const StatusReg *reg)          { return reg->loop_count; }

// ---- initialized ----
void status_set_initialized(StatusReg *reg, bool val)         { reg->initialized = val; }
bool status_get_initialized(const StatusReg *reg)             { return reg->initialized; }

// ---- button_pressed ----
void status_set_button_pressed(StatusReg *reg, bool val)      { reg->button_pressed = val; }
bool status_get_button_pressed(const StatusReg *reg)          { return reg->button_pressed; }

// ---- target_motor_stop ----
void status_set_target_motor_stop(StatusReg *reg, bool val)   { reg->target_motor_stop = val; }
bool status_get_target_motor_stop(const StatusReg *reg)       { return reg->target_motor_stop; }

// =====================================================================
//  LedReg accessors  (unchanged)
// =====================================================================

// ---- blue ----
void     led_inc_blue(void)             { g_led_reg.blue_toggle_count++; }
uint32_t led_get_blue(void)             { return g_led_reg.blue_toggle_count; }

// ---- green ----
void     led_inc_green(void)            { g_led_reg.green_toggle_count++; }
uint32_t led_get_green(void)            { return g_led_reg.green_toggle_count; }
