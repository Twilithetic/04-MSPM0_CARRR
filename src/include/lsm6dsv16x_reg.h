/*
 *  ======== lsm6dsv16x_reg.h ========
 *  LSM6DSV16X IMU — register map, bit definitions, and shadow-register type.
 *
 *  Based on lsm6dsv16x datasheet (STMicroelectronics).
 *  I2C 7-bit address: 0x6A (SA0=0) or 0x6B (SA0=1).
 *
 *  This file is the single source of truth for register addresses and
 *  the opaque shadow-register type.  The full struct lives in registers.c.
 */

#ifndef LSM6DSV16X_REG_H
#define LSM6DSV16X_REG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 *  I2C address
 * ================================================================ */

#define LSM6DSV16X_I2C_ADDR_SA0_0    0x6AU   /* SA0 pin = GND */
#define LSM6DSV16X_I2C_ADDR_SA0_1    0x6BU   /* SA0 pin = VDD */

#define LSM6DSV16X_WHO_AM_I_VALUE    0x70U

/* ================================================================
 *  Main-page registers
 * ================================================================ */

#define LSM6DSV16X_FUNC_CFG_ACCESS    0x01U
#define LSM6DSV16X_FIFO_CTRL1         0x07U
#define LSM6DSV16X_FIFO_CTRL2         0x08U
#define LSM6DSV16X_FIFO_CTRL3         0x09U
#define LSM6DSV16X_FIFO_CTRL4         0x0AU
#define LSM6DSV16X_WHO_AM_I           0x0FU

#define LSM6DSV16X_CTRL1_XL           0x10U   /* accel ODR + mode */
#define LSM6DSV16X_CTRL2_G            0x11U   /* gyro  ODR + mode */
#define LSM6DSV16X_CTRL3              0x12U   /* sw_reset, if_inc, bdu, boot */
#define LSM6DSV16X_CTRL4              0x13U   /* drdy_pulsed, int2_on_int1 */
#define LSM6DSV16X_CTRL5              0x14U   /* bus_act_sel */
#define LSM6DSV16X_CTRL6_G            0x15U   /* gyro FS + LPF1 */
#define LSM6DSV16X_CTRL7_G            0x16U   /* lpf1_g_en */
#define LSM6DSV16X_CTRL8_XL           0x17U   /* accel FS + LPF2 */
#define LSM6DSV16X_CTRL9_XL           0x18U   /* lpf2_xl_en */
#define LSM6DSV16X_CTRL10             0x19U   /* st_xl, st_g, emb_func_debug */

#define LSM6DSV16X_FIFO_STATUS1       0x1BU
#define LSM6DSV16X_FIFO_STATUS2       0x1CU
#define LSM6DSV16X_STATUS_REG         0x1EU

#define LSM6DSV16X_OUT_TEMP_L         0x20U
#define LSM6DSV16X_OUT_TEMP_H         0x21U
#define LSM6DSV16X_OUTX_L_G           0x22U
#define LSM6DSV16X_OUTX_H_G           0x23U
#define LSM6DSV16X_OUTY_L_G           0x24U
#define LSM6DSV16X_OUTY_H_G           0x25U
#define LSM6DSV16X_OUTZ_L_G           0x26U
#define LSM6DSV16X_OUTZ_H_G           0x27U
#define LSM6DSV16X_OUTX_L_A           0x28U
#define LSM6DSV16X_OUTX_H_A           0x29U
#define LSM6DSV16X_OUTY_L_A           0x2AU
#define LSM6DSV16X_OUTY_H_A           0x2BU
#define LSM6DSV16X_OUTZ_L_A           0x2CU
#define LSM6DSV16X_OUTZ_H_A           0x2DU

/* FIFO data output */
#define LSM6DSV16X_FIFO_DATA_OUT_TAG  0x78U
#define LSM6DSV16X_FIFO_DATA_OUT_X_L  0x79U
#define LSM6DSV16X_FIFO_DATA_OUT_X_H  0x7AU
#define LSM6DSV16X_FIFO_DATA_OUT_Y_L  0x7BU
#define LSM6DSV16X_FIFO_DATA_OUT_Y_H  0x7CU
#define LSM6DSV16X_FIFO_DATA_OUT_Z_L  0x7DU
#define LSM6DSV16X_FIFO_DATA_OUT_Z_H  0x7EU

/* ================================================================
 *  Embedded-function-page registers  (bank = EMB_FUNC_REG_ACCESS)
 * ================================================================ */

#define LSM6DSV16X_PAGE_SEL            0x02U
#define LSM6DSV16X_EMB_FUNC_EN_B       0x05U
#define LSM6DSV16X_EMB_FUNC_INIT_B     0x67U
#define LSM6DSV16X_EMB_FUNC_FIFO_EN_A  0x44U
#define LSM6DSV16X_EMB_FUNC_FIFO_EN_B  0x45U
#define LSM6DSV16X_EMB_FUNC_STATUS     0x12U
#define LSM6DSV16X_SFLP_ODR            0x5EU
#define LSM6DSV16X_FSM_ENABLE          0x46U

/* SFLP game rotation vector registers (embedded page 0) */
#define LSM6DSV16X_SFLP_GAME_GBIASX_L  0x6EU
#define LSM6DSV16X_SFLP_GAME_GBIASX_H  0x6FU
#define LSM6DSV16X_SFLP_GAME_GBIASY_L  0x70U
#define LSM6DSV16X_SFLP_GAME_GBIASY_H  0x71U
#define LSM6DSV16X_SFLP_GAME_GBIASZ_L  0x72U
#define LSM6DSV16X_SFLP_GAME_GBIASZ_H  0x73U

/* ================================================================
 *  Bit definitions
 * ================================================================ */

/* CTRL1_XL — accel ODR (bits 3:0) */
#define LSM6DSV16X_ODR_XL_OFF          0x00U
#define LSM6DSV16X_ODR_XL_60HZ         0x06U
#define LSM6DSV16X_ODR_XL_120HZ        0x07U
#define LSM6DSV16X_ODR_XL_240HZ        0x08U
#define LSM6DSV16X_ODR_XL_480HZ        0x09U
#define LSM6DSV16X_ODR_XL_960HZ        0x0AU

/* CTRL2_G — gyro ODR (bits 3:0) */
#define LSM6DSV16X_ODR_G_OFF           0x00U
#define LSM6DSV16X_ODR_G_60HZ          0x06U
#define LSM6DSV16X_ODR_G_120HZ         0x07U
#define LSM6DSV16X_ODR_G_240HZ         0x08U
#define LSM6DSV16X_ODR_G_480HZ         0x09U
#define LSM6DSV16X_ODR_G_960HZ         0x0AU

/* CTRL3 */
#define LSM6DSV16X_SW_RESET            (1U << 0)
#define LSM6DSV16X_IF_INC              (1U << 2)
#define LSM6DSV16X_BDU                 (1U << 6)
#define LSM6DSV16X_BOOT                (1U << 7)

/* CTRL4 */
#define LSM6DSV16X_DRDY_PULSED         (1U << 1)
#define LSM6DSV16X_INT2_ON_INT1        (1U << 4)

/* CTRL6_G — gyro full-scale (bits 3:0) */
#define LSM6DSV16X_FS_G_2000DPS         0x03U

/* CTRL8_XL — accel full-scale (bits 1:0) */
#define LSM6DSV16X_FS_XL_16G            0x03U

/* FUNC_CFG_ACCESS — memory bank switching */
#define LSM6DSV16X_EMB_FUNC_REG_ACCESS   (1U << 2)

/* EMB_FUNC_EN_B */
#define LSM6DSV16X_FSM_EN              (1U << 0)
#define LSM6DSV16X_FIFO_COMPR_EN       (1U << 3)
#define LSM6DSV16X_MLC_EN              (1U << 4)

/* EMB_FUNC_INIT_B */
#define LSM6DSV16X_FSM_INIT            (1U << 0)
#define LSM6DSV16X_FIFO_COMPR_INIT     (1U << 3)
#define LSM6DSV16X_MLC_INIT            (1U << 4)

/* EMB_FUNC_FIFO_EN_A */
#define LSM6DSV16X_SFLP_GAME_FIFO_EN   (1U << 1)

/* STATUS_REG */
#define LSM6DSV16X_XLDA                (1U << 0)
#define LSM6DSV16X_GDA                 (1U << 1)
#define LSM6DSV16X_TDA                 (1U << 2)

/* FIFO_STATUS2 */
#define LSM6DSV16X_FIFO_WTM_IA         (1U << 7)

/* FIFO_CTRL4 — mode (bits 2:0) */
#define LSM6DSV16X_FIFO_MODE_BYPASS     0x00U
#define LSM6DSV16X_FIFO_MODE_FIFO       0x01U
#define LSM6DSV16X_FIFO_MODE_CONTINUOUS 0x03U

/* FIFO tag values */
#define LSM6DSV16X_SFLP_GAME_ROTATION_VECTOR_TAG  0x13U
#define LSM6DSV16X_SFLP_GYROSCOPE_BIAS_TAG        0x16U
#define LSM6DSV16X_SFLP_GRAVITY_VECTOR_TAG        0x17U

/* ================================================================
 *  SFLP quaternion conversion factor
 *  Raw int16 * 0.061f → unit quaternion component.
 *  Fixed-point: raw * 61 / 1000.
 * ================================================================ */

#define LSM6DSV16X_SFLP_Q_SCALE_NUM    61
#define LSM6DSV16X_SFLP_Q_SCALE_DEN    1000

/* ================================================================
 *  Shadow register — opaque type
 *  Full struct definition is private to registers.c.
 * ================================================================ */

typedef struct Lsm6dsv16xReg Lsm6dsv16xReg;

extern Lsm6dsv16xReg g_lsm6dsv16x_reg;

/* ── Read access (Client / Logger) ── */

/** most recent quaternion component (fixed-point, scale *61/1000) */
int16_t imu_get_qw(void);
int16_t imu_get_qx(void);
int16_t imu_get_qy(void);
int16_t imu_get_qz(void);

/** most recent gyroscope data (mdps — raw milli-dps) */
int16_t imu_get_gx(void);
int16_t imu_get_gy(void);
int16_t imu_get_gz(void);

/** most recent accelerometer data (mg — raw milli-g) */
int16_t imu_get_ax(void);
int16_t imu_get_ay(void);
int16_t imu_get_az(void);

/** temperature (tenths of °C) */
int16_t imu_get_temp(void);

/** timestamp of last successful sync (ms since boot) */
uint32_t imu_get_timestamp_ms(void);

/** how many samples have been synced since power-on */
uint32_t imu_get_sample_count(void);

/** non-zero if the IMU has been initialised and is producing data */
uint8_t  imu_is_ready(void);

/* ── Write access (Driver / Proxy only) ── */

void imu_set_quaternion(int16_t qw, int16_t qx, int16_t qy, int16_t qz);
void imu_set_gyro(int16_t gx, int16_t gy, int16_t gz);
void imu_set_accel(int16_t ax, int16_t ay, int16_t az);
void imu_set_temp(int16_t temp);
void imu_set_timestamp_ms(uint32_t ts);
void imu_inc_sample_count(void);
void imu_set_ready(uint8_t ready);

#ifdef __cplusplus
}
#endif

#endif /* LSM6DSV16X_REG_H */
