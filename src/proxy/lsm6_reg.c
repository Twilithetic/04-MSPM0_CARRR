/*
 *  ======== lsm6_reg.c ========
 *  LSM6DSV16X IMU shadow register — global instance + accessors.
 */

#include "include/imu_shadow.h"
#include "include/lsm6dsv16x_reg.h"       /* ST official register / bit defs */

#include <FreeRTOS.h>
#include <task.h>
#include <stdint.h>

/* ---- Full struct definition (opaque to client) ---- */
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
    uint8_t cfg_ctrl3_boot;
    uint8_t cfg_ctrl3;
    uint8_t cfg_ctrl1_xl;
    uint8_t cfg_ctrl2_g;
    uint8_t cfg_post_ctrl1;
    uint8_t cfg_post_ctrl2;
    uint8_t cfg_post_ctrl8;
    uint8_t cfg_post_ctrl6;
    uint8_t cfg_fca;

    /* SFLP enable/status diagnostics */
    uint8_t sflp_en_a;
    uint8_t sflp_init_a;
    uint8_t sflp_exec_status;
    uint8_t sflp_fifo_en_a;
    uint8_t fifo_status1;
    uint8_t fifo_status2;
    uint8_t sflp_odr_rdbk;
    uint8_t page_sel_rdbk;

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
    return (uint16_t)(((uint32_t)dq * configTICK_RATE_HZ) / dt);
}

int16_t  imu_get_yaw_deg100(void)        { return g_lsm6dsv16x_reg.yaw_deg100; }
int16_t  imu_get_pitch_deg100(void)      { return g_lsm6dsv16x_reg.pitch_deg100; }
int16_t  imu_get_roll_deg100(void)       { return g_lsm6dsv16x_reg.roll_deg100; }
uint8_t  imu_is_ready(void)              { return g_lsm6dsv16x_reg.ready; }
uint8_t  imu_get_whoami(void)            { return g_lsm6dsv16x_reg.whoami; }
uint8_t  imu_get_init_err(void)          { return g_lsm6dsv16x_reg.init_err; }
uint32_t imu_get_bus_err(void)           { return g_lsm6dsv16x_reg.bus_err; }
uint8_t  imu_get_cfg_ctrl3(void)         { return g_lsm6dsv16x_reg.cfg_ctrl3; }
uint8_t  imu_get_cfg_ctrl1_xl(void)      { return g_lsm6dsv16x_reg.cfg_ctrl1_xl; }
uint8_t  imu_get_cfg_ctrl2_g(void)       { return g_lsm6dsv16x_reg.cfg_ctrl2_g; }
uint8_t  imu_get_cfg_post_ctrl1(void)    { return g_lsm6dsv16x_reg.cfg_post_ctrl1; }
uint8_t  imu_get_cfg_post_ctrl2(void)    { return g_lsm6dsv16x_reg.cfg_post_ctrl2; }
uint8_t  imu_get_cfg_post_ctrl8(void)    { return g_lsm6dsv16x_reg.cfg_post_ctrl8; }
uint8_t  imu_get_cfg_post_ctrl6(void)    { return g_lsm6dsv16x_reg.cfg_post_ctrl6; }
uint8_t  imu_get_cfg_fca(void)           { return g_lsm6dsv16x_reg.cfg_fca; }
uint8_t  imu_get_sflp_en_a(void)         { return g_lsm6dsv16x_reg.sflp_en_a; }
uint8_t  imu_get_sflp_init_a(void)       { return g_lsm6dsv16x_reg.sflp_init_a; }
uint8_t  imu_get_sflp_exec_status(void)  { return g_lsm6dsv16x_reg.sflp_exec_status; }
uint8_t  imu_get_sflp_fifo_en_a(void)    { return g_lsm6dsv16x_reg.sflp_fifo_en_a; }
uint8_t  imu_get_fifo_status1(void)      { return g_lsm6dsv16x_reg.fifo_status1; }
uint8_t  imu_get_fifo_status2(void)      { return g_lsm6dsv16x_reg.fifo_status2; }
uint8_t  imu_get_sflp_odr_rdbk(void)     { return g_lsm6dsv16x_reg.sflp_odr_rdbk; }
uint8_t  imu_get_page_sel_rdbk(void)     { return g_lsm6dsv16x_reg.page_sel_rdbk; }
uint8_t  imu_get_cfg_ctrl3_boot(void)    { return g_lsm6dsv16x_reg.cfg_ctrl3_boot; }
uint8_t  imu_get_status_raw(void)        { return g_lsm6dsv16x_reg.status_raw; }
uint8_t  imu_get_xlda(void)              { return g_lsm6dsv16x_reg.status_raw & IMU_XLDA; }
uint8_t  imu_get_gda(void)               { return g_lsm6dsv16x_reg.status_raw & IMU_GDA; }
uint8_t  imu_get_tda(void)               { return g_lsm6dsv16x_reg.status_raw & IMU_TDA; }

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
void imu_set_ready(uint8_t ready)              { g_lsm6dsv16x_reg.ready = ready; }
void imu_set_whoami(uint8_t whoami)            { g_lsm6dsv16x_reg.whoami = whoami; }
void imu_set_init_err(uint8_t err)             { g_lsm6dsv16x_reg.init_err = err; }
void imu_inc_bus_err(void)                     { g_lsm6dsv16x_reg.bus_err++; }
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
    g_lsm6dsv16x_reg.sflp_exec_status = exec;
    g_lsm6dsv16x_reg.sflp_fifo_en_a   = fifo_ena;
    g_lsm6dsv16x_reg.fifo_status1     = fs1;
    g_lsm6dsv16x_reg.fifo_status2     = fs2;
    g_lsm6dsv16x_reg.sflp_odr_rdbk   = odr_rdbk;
    g_lsm6dsv16x_reg.page_sel_rdbk   = pg_sel;
}
void imu_set_status(uint8_t status_raw)       { g_lsm6dsv16x_reg.status_raw = status_raw; }
