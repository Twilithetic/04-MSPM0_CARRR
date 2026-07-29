/*
 *  ======== imu_shadow.h ========
 *  LSM6DSV16X IMU — shadow-register type + accessor declarations.
 *
 *  This is the ONLY application-facing header for the IMU.
 *  Client code includes this, never the driver implementation.
 *
 *  The full struct definition is private to registers.c.
 *  Register addresses and bit defs now come from the ST official
 *  lsm6dsv16x_reg.h (lsm6dsv16x-pid repo).
 */

#ifndef IMU_SHADOW_H
#define IMU_SHADOW_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Opaque type ---- */
typedef struct LSM6DSV16XReg LSM6DSV16XReg;

extern LSM6DSV16XReg g_lsm6dsv16x_reg;

/* ================================================================
 *  Read access (Client / Logger)
 * ================================================================ */

/** Game rotation vector — quaternion (int16, scale *61/1000) */
int16_t imu_get_qw(void);
int16_t imu_get_qx(void);
int16_t imu_get_qy(void);
int16_t imu_get_qz(void);

/** Raw gyroscope (mdps) */
int16_t imu_get_gx(void);
int16_t imu_get_gy(void);
int16_t imu_get_gz(void);

/** Raw accelerometer (mg) */
int16_t imu_get_ax(void);
int16_t imu_get_ay(void);
int16_t imu_get_az(void);

/** Temperature (0.1 °C) */
int16_t imu_get_temp(void);

/** Timestamp of last sync (ms) */
uint32_t imu_get_timestamp_ms(void);

/** Total samples synced */
uint32_t imu_get_sample_count(void);

/** Get quaternion rate (samples/second), computed from delta since last call */
uint16_t imu_get_qps(void);

/** Exponentially-smoothed QPS (updated by vStatsTask @ 1Hz) */
uint16_t imu_get_smooth_qps(void);

/** Update stats EMA — called by vStatsTask */
void stats_update(void);

/** Yaw/Pitch/Roll angles from quaternion (*100 to keep 2 decimal places as int) */
int16_t imu_get_yaw_deg100(void);
int16_t imu_get_pitch_deg100(void);
int16_t imu_get_roll_deg100(void);

/** Nonzero if IMU initialised OK */
uint8_t  imu_is_ready(void);

/** WHO_AM_I byte */
uint8_t  imu_get_whoami(void);

/** 0=OK, 1=whoami, 2=boot timeout */
uint8_t  imu_get_init_err(void);

/** Failed I2C transfer count */
uint32_t imu_get_bus_err(void);

/** STATUS_REG raw byte */
uint8_t  imu_get_status_raw(void);

/* STATUS_REG bit defs (not in official _reg.h — they use a bitfield struct) */
#define IMU_XLDA  (1U << 0)
#define IMU_GDA   (1U << 1)
#define IMU_TDA   (1U << 2)

/* Convenience: data-ready flags */
uint8_t  imu_get_xlda(void);
uint8_t  imu_get_gda(void);
uint8_t  imu_get_tda(void);

/* ================================================================
 *  Write access (Proxy only — declared for registers.c linkage)
 * ================================================================ */

void imu_set_quaternion(int16_t qw, int16_t qx, int16_t qy, int16_t qz);
void imu_inc_q_count(void);
void imu_set_euler(int16_t yaw, int16_t pitch, int16_t roll);
void imu_set_gyro(int16_t gx, int16_t gy, int16_t gz);
void imu_set_accel(int16_t ax, int16_t ay, int16_t az);
void imu_set_temp(int16_t temp);
void imu_set_timestamp_ms(uint32_t ts);
void imu_inc_sample_count(void);
void imu_set_ready(uint8_t ready);
void imu_set_whoami(uint8_t whoami);
void imu_set_init_err(uint8_t err);
void imu_inc_bus_err(void);
void imu_set_status(uint8_t status_raw);
void imu_set_sflp_diag(uint8_t en_a, uint8_t init_a, uint8_t exec,
                       uint8_t fifo_ena, uint8_t fs1, uint8_t fs2,
                       uint8_t odr_rdbk, uint8_t pg_sel);

/* ---- Config readback (debug) ---- */
void imu_set_cfg_readback(uint8_t ctrl3, uint8_t ctrl1_xl, uint8_t ctrl2_g);
void imu_set_cfg_post_sflp(uint8_t ctrl1_xl, uint8_t ctrl2_g,
                           uint8_t ctrl8_xl, uint8_t ctrl6_g);
void imu_set_cfg_ctrl3_boot(uint8_t val);
void imu_set_cfg_fca(uint8_t val);

uint8_t  imu_get_cfg_ctrl3(void);
uint8_t  imu_get_cfg_ctrl3_boot(void);
uint8_t  imu_get_cfg_ctrl1_xl(void);
uint8_t  imu_get_cfg_ctrl2_g(void);
uint8_t  imu_get_cfg_post_ctrl1(void);
uint8_t  imu_get_cfg_post_ctrl2(void);
uint8_t  imu_get_cfg_post_ctrl8(void);
uint8_t  imu_get_cfg_post_ctrl6(void);
uint8_t  imu_get_cfg_fca(void);
uint8_t  imu_get_sflp_en_a(void);
uint8_t  imu_get_sflp_init_a(void);
uint8_t  imu_get_sflp_exec_status(void);
uint8_t  imu_get_sflp_fifo_en_a(void);
uint8_t  imu_get_fifo_status1(void);
uint8_t  imu_get_fifo_status2(void);
uint8_t  imu_get_sflp_odr_rdbk(void);
uint8_t  imu_get_page_sel_rdbk(void);

#ifdef __cplusplus
}
#endif

#endif /* IMU_SHADOW_H */
