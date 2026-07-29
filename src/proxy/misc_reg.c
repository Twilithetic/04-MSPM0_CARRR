/*
 *  ======== misc_reg.c ========
 *  Miscellaneous shared registers:
 *    StatusReg  — system status flags
 *    LineReg    — line sensor (opaque to client, defined in line_reg.h)
 *    I2cScanReg — I2C bus-scan result
 *    Stats      — EMA-smoothing for QPS and motor sync rate
 */

#include "include/status_reg.h"
#include "include/line_reg.h"
#include "include/i2c_scanner_reg.h"
#include "include/imu_shadow.h"           /* imu_get_qps */
#include "include/motor_driver_reg.h"     /* motor_get_sync_rate */

#include <stdbool.h>
#include <stdint.h>

// =====================================================================
//  LineReg  (full definition in line_reg.h — not opaque)
// =====================================================================
LineReg   g_line_reg   = {0};

// =====================================================================
//  StatusReg  FULL struct definition — private to this TU
// =====================================================================
struct StatusReg {
    bool    line_lost;
    bool    line_all_black;
    bool    motor_error;
    uint8_t motor_error_code;
    uint16_t loop_count;
    bool    initialized;
    bool    button_pressed;
    bool    target_motor_stop;
};

StatusReg g_status_reg = {0};

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
//  I2cScanReg  — I2C bus-scan result shadow register
// =====================================================================
struct I2cScanReg {
    uint8_t  count;
    uint8_t  addr[I2C_SCAN_MAX_DEVICES];
    uint8_t  whoami[I2C_SCAN_MAX_DEVICES];
    volatile bool ack_map[I2C_SCAN_ADDR_RANGE];
};

I2cScanReg g_i2c_scan_reg = {0};

uint8_t i2c_scan_get_count(void)                     { return g_i2c_scan_reg.count; }
void    i2c_scan_set_count(uint8_t cnt)              { g_i2c_scan_reg.count = cnt; }

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
//  Stats smoothing (exponential moving average)
// =====================================================================
#define STATS_ALPHA  0.75f

static float   g_smooth_qps   = 0.0f;
static float   g_smooth_msync = 0.0f;
static bool    g_stats_inited = false;

void stats_update(void)
{
    uint16_t raw_qps   = imu_get_qps();
    uint16_t raw_msync = motor_get_sync_rate();

    if (!g_stats_inited) {
        g_smooth_qps   = (float)raw_qps;
        g_smooth_msync = (float)raw_msync;
        g_stats_inited = true;
    } else {
        g_smooth_qps   = g_smooth_qps   * (1.0f - STATS_ALPHA) + (float)raw_qps   * STATS_ALPHA;
        g_smooth_msync = g_smooth_msync * (1.0f - STATS_ALPHA) + (float)raw_msync * STATS_ALPHA;
    }
}

uint16_t imu_get_smooth_qps(void)
{
    return (uint16_t)g_smooth_qps;
}

uint16_t motor_get_smooth_sync_rate(void)
{
    return (uint16_t)g_smooth_msync;
}
