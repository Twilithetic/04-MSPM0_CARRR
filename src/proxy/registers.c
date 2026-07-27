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
#include "include/imu_shadow.h"           /* shadow type + accessors */
#include "include/lsm6dsv16x_reg.h"       /* ST official register / bit defs */

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
//  LSM6DSV16XReg  — IMU shadow register
// =====================================================================

struct LSM6DSV16XReg {
    /* SFLP game rotation vector — quaternion (fixed-point *61/1000) */
    int16_t qw, qx, qy, qz;

    /* raw gyroscope (mdps) — sync from FIFO SFLP */
    int16_t gx, gy, gz;

    /* raw accelerometer (mg) — sync from OUTX_L_A */
    int16_t ax, ay, az;

    /* temperature in 0.1 °C — sync from OUT_TEMP_L */
    int16_t temp;

    /* timestamp of last successful sync (ms since boot) */
    uint32_t timestamp_ms;

    /* total sample count since init */
    uint32_t sample_count;

    /* quaternion count in current 1s window */
    uint16_t q_count;

    /* timestamp of last qps snap (FreeRTOS tick) */
    uint32_t last_qps_tick;

    /* q_count at last qps snap */
    uint16_t last_qps_count;

    /* computed quaternion rate (samples / second) */
    uint16_t qps;

    /* Euler angles from quaternion (degrees * 100) */
    int16_t yaw_deg100;
    int16_t pitch_deg100;
    int16_t roll_deg100;

    /* WHO_AM_I byte read during init (0x70 = genuine LSM6DSV16X) */
    uint8_t whoami;

    /* init result: 0 = OK, 1 = WHO_AM_I mismatch, 2 = BOOT bit timeout */
    uint8_t init_err;

    /* last STATUS_REG (0x1E) raw byte: XLDA|GDA|TDA data-ready flags */
    uint8_t status_raw;

    /* readback of config registers (verify writes took effect) */
    uint8_t cfg_ctrl3_boot; /* CTRL3 right after boot-wait loop — BOOT should be 0 */
    uint8_t cfg_ctrl3;     /* CTRL3 (0x12) after core init: should be 0x44 (IF_INC|BDU) */
    uint8_t cfg_ctrl1_xl;  /* CTRL1_XL (0x10) after core init, before SFLP: 0x08 */
    uint8_t cfg_ctrl2_g;   /* CTRL2_G (0x11) after core init, before SFLP: 0x08 */
    uint8_t cfg_post_ctrl1; /* CTRL1_XL after SFLP + FIFO: should still be 0x08 */
    uint8_t cfg_post_ctrl2; /* CTRL2_G  after SFLP + FIFO: should still be 0x08 */
    uint8_t cfg_post_ctrl8; /* CTRL8_XL after SFLP + FIFO: should be 0x03 */
    uint8_t cfg_post_ctrl6; /* CTRL6_G  after SFLP + FIFO: should be 0x04 */
    uint8_t cfg_fca;        /* FUNC_CFG_ACCESS (0x01) after init */

    /* SFLP enable/status diagnostics */
    uint8_t sflp_en_a;         /* EMB_FUNC_EN_A readback */
    uint8_t sflp_init_a;       /* EMB_FUNC_INIT_A readback (self-clears: 0=init done) */
    uint8_t sflp_exec_status;  /* EMB_FUNC_EXEC_STATUS: ENDOP=bit0 */
    uint8_t sflp_fifo_en_a;    /* EMB_FUNC_FIFO_EN_A readback */
    uint8_t fifo_status1;      /* FIFO_STATUS1 — number of unread entries LSB */
    uint8_t fifo_status2;      /* FIFO_STATUS2 — top bit + empty/overflow flags */
    uint8_t sflp_odr_rdbk;     /* SFLP_ODR readback — verify 60Hz write took effect */
    uint8_t page_sel_rdbk;     /* PAGE_SEL readback */

    /* failed I2C transfer counter during periodic sync */
    uint32_t bus_err;

    /* ready flag: set by Proxy after successful init */
    uint8_t ready;
};

LSM6DSV16XReg g_lsm6dsv16x_reg = {0};

/* ── Read access ── */

int16_t  imu_get_qw(void)              { return g_lsm6dsv16x_reg.qw; }
int16_t  imu_get_qx(void)              { return g_lsm6dsv16x_reg.qx; }
int16_t  imu_get_qy(void)              { return g_lsm6dsv16x_reg.qy; }
int16_t  imu_get_qz(void)              { return g_lsm6dsv16x_reg.qz; }

int16_t  imu_get_gx(void)              { return g_lsm6dsv16x_reg.gx; }
int16_t  imu_get_gy(void)              { return g_lsm6dsv16x_reg.gy; }
int16_t  imu_get_gz(void)              { return g_lsm6dsv16x_reg.gz; }

int16_t  imu_get_ax(void)              { return g_lsm6dsv16x_reg.ax; }
int16_t  imu_get_ay(void)              { return g_lsm6dsv16x_reg.ay; }
int16_t  imu_get_az(void)              { return g_lsm6dsv16x_reg.az; }

int16_t  imu_get_temp(void)            { return g_lsm6dsv16x_reg.temp; }

uint32_t imu_get_timestamp_ms(void)    { return g_lsm6dsv16x_reg.timestamp_ms; }
uint32_t imu_get_sample_count(void)    { return g_lsm6dsv16x_reg.sample_count; }
uint16_t imu_get_qps(void)
{
    uint32_t now  = xTaskGetTickCount();
    uint32_t dt   = now - g_lsm6dsv16x_reg.last_qps_tick;
    uint16_t dq   = (uint16_t)(g_lsm6dsv16x_reg.q_count - g_lsm6dsv16x_reg.last_qps_count);

    g_lsm6dsv16x_reg.last_qps_tick  = now;
    g_lsm6dsv16x_reg.last_qps_count = g_lsm6dsv16x_reg.q_count;

    if (dt == 0) { return 0; }
    /* qps = dq * 1000 / dt   (dt in ms) */
    return (uint16_t)(((uint32_t)dq * 1000U) / dt);
}
int16_t  imu_get_yaw_deg100(void)        { return g_lsm6dsv16x_reg.yaw_deg100; }
int16_t  imu_get_pitch_deg100(void)      { return g_lsm6dsv16x_reg.pitch_deg100; }
int16_t  imu_get_roll_deg100(void)       { return g_lsm6dsv16x_reg.roll_deg100; }
uint8_t  imu_is_ready(void)            { return g_lsm6dsv16x_reg.ready; }
uint8_t  imu_get_whoami(void)          { return g_lsm6dsv16x_reg.whoami; }
uint8_t  imu_get_init_err(void)        { return g_lsm6dsv16x_reg.init_err; }
uint32_t imu_get_bus_err(void)         { return g_lsm6dsv16x_reg.bus_err; }
uint8_t  imu_get_cfg_ctrl3(void)       { return g_lsm6dsv16x_reg.cfg_ctrl3; }
uint8_t  imu_get_cfg_ctrl1_xl(void)    { return g_lsm6dsv16x_reg.cfg_ctrl1_xl; }
uint8_t  imu_get_cfg_ctrl2_g(void)     { return g_lsm6dsv16x_reg.cfg_ctrl2_g; }
uint8_t  imu_get_cfg_post_ctrl1(void)  { return g_lsm6dsv16x_reg.cfg_post_ctrl1; }
uint8_t  imu_get_cfg_post_ctrl2(void)  { return g_lsm6dsv16x_reg.cfg_post_ctrl2; }
uint8_t  imu_get_cfg_post_ctrl8(void)  { return g_lsm6dsv16x_reg.cfg_post_ctrl8; }
uint8_t  imu_get_cfg_post_ctrl6(void)  { return g_lsm6dsv16x_reg.cfg_post_ctrl6; }
uint8_t  imu_get_cfg_fca(void)         { return g_lsm6dsv16x_reg.cfg_fca; }
uint8_t  imu_get_sflp_en_a(void)       { return g_lsm6dsv16x_reg.sflp_en_a; }
uint8_t  imu_get_sflp_init_a(void)     { return g_lsm6dsv16x_reg.sflp_init_a; }
uint8_t  imu_get_sflp_exec_status(void){ return g_lsm6dsv16x_reg.sflp_exec_status; }
uint8_t  imu_get_sflp_fifo_en_a(void)  { return g_lsm6dsv16x_reg.sflp_fifo_en_a; }
uint8_t  imu_get_fifo_status1(void)    { return g_lsm6dsv16x_reg.fifo_status1; }
uint8_t  imu_get_fifo_status2(void)    { return g_lsm6dsv16x_reg.fifo_status2; }
uint8_t  imu_get_sflp_odr_rdbk(void)   { return g_lsm6dsv16x_reg.sflp_odr_rdbk; }
uint8_t  imu_get_page_sel_rdbk(void)   { return g_lsm6dsv16x_reg.page_sel_rdbk; }
uint8_t  imu_get_cfg_ctrl3_boot(void)  { return g_lsm6dsv16x_reg.cfg_ctrl3_boot; }
uint8_t  imu_get_status_raw(void)      { return g_lsm6dsv16x_reg.status_raw; }
uint8_t  imu_get_xlda(void)            { return g_lsm6dsv16x_reg.status_raw & IMU_XLDA; }
uint8_t  imu_get_gda(void)             { return g_lsm6dsv16x_reg.status_raw & IMU_GDA; }
uint8_t  imu_get_tda(void)             { return g_lsm6dsv16x_reg.status_raw & IMU_TDA; }

/* ── Write access (Proxy only) ── */

void imu_set_quaternion(int16_t qw, int16_t qx, int16_t qy, int16_t qz)
{
    g_lsm6dsv16x_reg.qw = qw;
    g_lsm6dsv16x_reg.qx = qx;
    g_lsm6dsv16x_reg.qy = qy;
    g_lsm6dsv16x_reg.qz = qz;
}

void imu_set_gyro(int16_t gx, int16_t gy, int16_t gz)
{
    g_lsm6dsv16x_reg.gx = gx;
    g_lsm6dsv16x_reg.gy = gy;
    g_lsm6dsv16x_reg.gz = gz;
}

void imu_set_accel(int16_t ax, int16_t ay, int16_t az)
{
    g_lsm6dsv16x_reg.ax = ax;
    g_lsm6dsv16x_reg.ay = ay;
    g_lsm6dsv16x_reg.az = az;
}

void imu_inc_q_count(void)                { g_lsm6dsv16x_reg.q_count++; }
void imu_set_euler(int16_t y, int16_t p, int16_t r)
{
    g_lsm6dsv16x_reg.yaw_deg100   = y;
    g_lsm6dsv16x_reg.pitch_deg100 = p;
    g_lsm6dsv16x_reg.roll_deg100  = r;
}
void imu_set_temp(int16_t temp)               { g_lsm6dsv16x_reg.temp = temp; }
void imu_set_timestamp_ms(uint32_t ts)         { g_lsm6dsv16x_reg.timestamp_ms = ts; }
void imu_inc_sample_count(void)                { g_lsm6dsv16x_reg.sample_count++; }
void imu_set_ready(uint8_t ready)             { g_lsm6dsv16x_reg.ready = ready; }
void imu_set_whoami(uint8_t whoami)           { g_lsm6dsv16x_reg.whoami = whoami; }
void imu_set_init_err(uint8_t err)            { g_lsm6dsv16x_reg.init_err = err; }
void imu_inc_bus_err(void)                    { g_lsm6dsv16x_reg.bus_err++; }
void imu_set_cfg_readback(uint8_t c3, uint8_t c1, uint8_t c2)
{
    g_lsm6dsv16x_reg.cfg_ctrl3   = c3;
    g_lsm6dsv16x_reg.cfg_ctrl1_xl = c1;
    g_lsm6dsv16x_reg.cfg_ctrl2_g  = c2;
}
void imu_set_cfg_post_sflp(uint8_t c1, uint8_t c2, uint8_t c8, uint8_t c6)
{
    g_lsm6dsv16x_reg.cfg_post_ctrl1 = c1;
    g_lsm6dsv16x_reg.cfg_post_ctrl2 = c2;
    g_lsm6dsv16x_reg.cfg_post_ctrl8 = c8;
    g_lsm6dsv16x_reg.cfg_post_ctrl6 = c6;
}
void imu_set_cfg_ctrl3_boot(uint8_t val)   { g_lsm6dsv16x_reg.cfg_ctrl3_boot = val; }
void imu_set_cfg_fca(uint8_t val)           { g_lsm6dsv16x_reg.cfg_fca = val; }
void imu_set_sflp_diag(uint8_t en_a, uint8_t init_a, uint8_t exec, uint8_t fifo_ena,
                       uint8_t fs1, uint8_t fs2, uint8_t odr_rdbk, uint8_t pg_sel)
{
    g_lsm6dsv16x_reg.sflp_en_a        = en_a;
    g_lsm6dsv16x_reg.sflp_init_a      = init_a;
    g_lsm6dsv16x_reg.sflp_exec_status  = exec;
    g_lsm6dsv16x_reg.sflp_fifo_en_a   = fifo_ena;
    g_lsm6dsv16x_reg.fifo_status1     = fs1;
    g_lsm6dsv16x_reg.fifo_status2     = fs2;
    g_lsm6dsv16x_reg.sflp_odr_rdbk   = odr_rdbk;
    g_lsm6dsv16x_reg.page_sel_rdbk   = pg_sel;
}
void imu_set_status(uint8_t status_raw)       { g_lsm6dsv16x_reg.status_raw = status_raw; }

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
