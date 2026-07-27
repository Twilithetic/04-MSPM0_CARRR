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
#define LSM6DSV16X_PIN_CTRL            0x02U   /* SDO pull-up */
#define LSM6DSV16X_ODR_TRIG_CFG        0x06U   /* ODR-triggered mode */
#define LSM6DSV16X_FIFO_CTRL1         0x07U
#define LSM6DSV16X_FIFO_CTRL2         0x08U
#define LSM6DSV16X_FIFO_CTRL3         0x09U
#define LSM6DSV16X_FIFO_CTRL4         0x0AU
#define LSM6DSV16X_COUNTER_BDR_REG1   0x0BU   /* batch data rate counter */
#define LSM6DSV16X_COUNTER_BDR_REG2   0x0CU
#define LSM6DSV16X_INT1_CTRL           0x0DU   /* INT1 routing */
#define LSM6DSV16X_INT2_CTRL           0x0EU   /* INT2 routing */
#define LSM6DSV16X_WHO_AM_I           0x0FU

#define LSM6DSV16X_CTRL_STATUS         0x1AU   /* FSM_WR_CTRL_STATUS */
#define LSM6DSV16X_ALL_INT_SRC          0x1DU   /* all interrupt summary */

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

/* ---- Main-page status registers ---- */
#define LSM6DSV16X_WAKE_UP_SRC         0x45U   /* wake-up event source */
#define LSM6DSV16X_TAP_SRC             0x46U   /* tap event source */
#define LSM6DSV16X_D6D_SRC             0x47U   /* 6D/4D orientation source */
#define LSM6DSV16X_STATUS_MASTER_MAINPAGE 0x48U
#define LSM6DSV16X_EMB_FUNC_STATUS_MAINPAGE 0x49U
#define LSM6DSV16X_FSM_STATUS_MAINPAGE  0x4AU
#define LSM6DSV16X_MLC_STATUS_MAINPAGE  0x4BU

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

/* ---- Timestamp ---- */
#define LSM6DSV16X_TIMESTAMP0          0x40U
#define LSM6DSV16X_TIMESTAMP1          0x41U
#define LSM6DSV16X_TIMESTAMP2          0x42U
#define LSM6DSV16X_TIMESTAMP3          0x43U

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
#define LSM6DSV16X_EMB_FUNC_EN_A       0x04U   /* SFLP_GAME_EN, SIGN_MOTION_EN, etc. */
#define LSM6DSV16X_EMB_FUNC_EN_B       0x05U
#define LSM6DSV16X_EMB_FUNC_EXEC_STATUS 0x07U  /* EMB_FUNC_ENDOP, _OVR */
#define LSM6DSV16X_PAGE_RW             0x17U   /* PAGE_READ, PAGE_WRITE */
#define LSM6DSV16X_EMB_FUNC_INT1        0x0DU   /* embedded function int1 routing */
#define LSM6DSV16X_EMB_FUNC_INT2        0x0EU   /* embedded function int2 routing */
#define LSM6DSV16X_EMB_FUNC_STATUS      0x12U
#define LSM6DSV16X_EMB_FUNC_FIFO_EN_A   0x44U
#define LSM6DSV16X_EMB_FUNC_FIFO_EN_B   0x45U
#define LSM6DSV16X_EMB_FUNC_INIT_A      0x66U   /* SFLP_GAME_INIT */
#define LSM6DSV16X_EMB_FUNC_INIT_B      0x67U
#define LSM6DSV16X_SFLP_ODR             0x5EU
#define LSM6DSV16X_FSM_ENABLE           0x46U

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

/* CTRL6_G — gyro full-scale FS_G[3:0]: 0011=±1000dps, 0100=±2000dps (DS13510 Table 63) */
#define LSM6DSV16X_FS_G_1000DPS         0x03U
#define LSM6DSV16X_FS_G_2000DPS         0x04U

/* CTRL8_XL — accel full-scale (bits 1:0) */
#define LSM6DSV16X_FS_XL_16G            0x03U

/* FUNC_CFG_ACCESS (DS13510 Table 25) — memory bank switching
 *
 * WARNING: bit 2 is SW_POR (global device reset), NOT emb_func_reg_access!
 * Writing 0x04 to FUNC_CFG_ACCESS triggers a full chip reset.
 *
 *   bit 7  EMB_FUNC_REG_ACCESS = unlocks embedded-function page
 *   bit 6  SHUB_REG_ACCESS     = unlocks sensor-hub page
 *   bit 3  FSM_WR_CTRL_EN
 *   bit 2  SW_POR              = GLOBAL RESET (do NOT set unless intended)
 *   bit 1  SPI2_RESET
 *   bit 0  OIS_CTRL_FROM_UI
 */
#define LSM6DSV16X_EMB_FUNC_REG_ACCESS   (1U << 7)

/* EMB_FUNC_EN_A (DS13510 Table 265) */
#define LSM6DSV16X_SFLP_GAME_EN         (1U << 1)  /* enable SFLP game rotation vector */
#define LSM6DSV16X_PEDO_EN              (1U << 3)
#define LSM6DSV16X_TILT_EN              (1U << 4)
#define LSM6DSV16X_SIGN_MOTION_EN       (1U << 5)

/* EMB_FUNC_EN_B */
#define LSM6DSV16X_FSM_EN              (1U << 0)
#define LSM6DSV16X_FIFO_COMPR_EN       (1U << 3)
#define LSM6DSV16X_MLC_EN              (1U << 4)

/* EMB_FUNC_INIT_A (DS13510 Table 335) */
#define LSM6DSV16X_SFLP_GAME_INIT       (1U << 1)  /* init SFLP algorithm */
#define LSM6DSV16X_STEP_DET_INIT        (1U << 3)
#define LSM6DSV16X_TILT_INIT            (1U << 4)
#define LSM6DSV16X_SIG_MOT_INIT         (1U << 5)

/* EMB_FUNC_INIT_B */
#define LSM6DSV16X_FSM_INIT            (1U << 0)
#define LSM6DSV16X_FIFO_COMPR_INIT     (1U << 3)
#define LSM6DSV16X_MLC_INIT            (1U << 4)

/* PAGE_RW */
#define LSM6DSV16X_PAGE_READ            (1U << 2)
#define LSM6DSV16X_PAGE_WRITE           (1U << 3)
#define LSM6DSV16X_EMB_FUNC_LIR         (1U << 5)

/* EMB_FUNC_FIFO_EN_A */
#define LSM6DSV16X_SFLP_GAME_FIFO_EN   (1U << 1)

/* SFLP_ODR (5Eh) — SFLP_GAME_ODR[2:0] at bits [4:2] (DS13510 Table 323).
 * Bits 5/1/0 are reserved-must-be-1 → always read-modify-write this register. */
#define LSM6DSV16X_SFLP_ODR_MASK        (0x07U << 2)
#define LSM6DSV16X_SFLP_ODR_15HZ        (0x00U << 2)
#define LSM6DSV16X_SFLP_ODR_30HZ        (0x01U << 2)
#define LSM6DSV16X_SFLP_ODR_60HZ        (0x02U << 2)
#define LSM6DSV16X_SFLP_ODR_120HZ       (0x03U << 2)
#define LSM6DSV16X_SFLP_ODR_240HZ       (0x04U << 2)

/* STATUS_REG */
#define LSM6DSV16X_XLDA                (1U << 0)
#define LSM6DSV16X_GDA                 (1U << 1)
#define LSM6DSV16X_TDA                 (1U << 2)
#define LSM6DSV16X_AH_QVARDA           (1U << 3)
#define LSM6DSV16X_GDA_EIS             (1U << 4)
#define LSM6DSV16X_OIS_DRDY            (1U << 5)
#define LSM6DSV16X_TIMESTAMP_ENDCOUNT  (1U << 7)

/* FIFO_STATUS1 — DIFF_FIFO[7:0] */

/* FIFO_STATUS2 */
#define LSM6DSV16X_DIFF_FIFO_8         (1U << 0)
#define LSM6DSV16X_FIFO_OVR_LATCHED    (1U << 4)
#define LSM6DSV16X_COUNTER_BDR_IA      (1U << 5)
#define LSM6DSV16X_FIFO_FULL_IA        (1U << 6)
#define LSM6DSV16X_FIFO_OVR_IA         (1U << 7)
#define LSM6DSV16X_FIFO_WTM_IA         (1U << 7)

/* CTRL_STATUS */
#define LSM6DSV16X_FSM_WR_CTRL_STATUS  (1U << 2)

/* EMB_FUNC_EXEC_STATUS */
#define LSM6DSV16X_EMB_FUNC_ENDOP      (1U << 0)
#define LSM6DSV16X_EMB_FUNC_EXEC_OVR   (1U << 1)

/* EMB_FUNC_STATUS */
#define LSM6DSV16X_IS_STEP_DET         (1U << 4)
#define LSM6DSV16X_IS_TILT             (1U << 5)
#define LSM6DSV16X_IS_SIGMOT           (1U << 6)

/* FIFO_CTRL4 — FIFO_MODE[2:0] (DS13510 Table 40) */
#define LSM6DSV16X_FIFO_MODE_BYPASS     0x00U
#define LSM6DSV16X_FIFO_MODE_FIFO       0x01U
#define LSM6DSV16X_FIFO_MODE_CONTINUOUS 0x06U

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

/** WHO_AM_I byte read during init (0x70 expected, 0 = not read/failed) */
uint8_t  imu_get_whoami(void);

/** init result: 0 = OK, 1 = WHO_AM_I mismatch, 2 = BOOT bit timeout */
uint8_t  imu_get_init_err(void);

/** failed I2C transfer counter in periodic sync — resetting rapidly = bus trouble */
uint32_t imu_get_bus_err(void);

/** readback of config registers after init (verify writes took effect) */
uint8_t  imu_get_cfg_ctrl3_boot(void);   /* CTRL3 right after boot-wait loop */
uint8_t  imu_get_cfg_ctrl3(void);
uint8_t  imu_get_cfg_ctrl1_xl(void);    /* before SFLP */
uint8_t  imu_get_cfg_ctrl2_g(void);     /* before SFLP */
uint8_t  imu_get_cfg_post_ctrl1(void);  /* after SFLP */
uint8_t  imu_get_cfg_post_ctrl2(void);  /* after SFLP */
uint8_t  imu_get_cfg_post_ctrl8(void);
uint8_t  imu_get_cfg_post_ctrl6(void);
uint8_t  imu_get_cfg_fca(void);         /* FUNC_CFG_ACCESS (0x01) after init */
uint8_t  imu_get_sflp_en_a(void);       /* EMB_FUNC_EN_A readback */
uint8_t  imu_get_sflp_init_a(void);     /* EMB_FUNC_INIT_A readback (self-clears) */
uint8_t  imu_get_sflp_exec_status(void);/* EMB_FUNC_EXEC_STATUS: ENDOP check */
uint8_t  imu_get_sflp_fifo_en_a(void);  /* EMB_FUNC_FIFO_EN_A readback */
uint8_t  imu_get_fifo_status1(void);    /* FIFO unread entries LSB */
uint8_t  imu_get_fifo_status2(void);    /* FIFO overflow/empty flags */

/** last STATUS_REG (0x1E) raw byte — sync_from_device updates */
uint8_t  imu_get_status_raw(void);

/** STATUS_REG data-ready flags — sync_from_device updates */
uint8_t  imu_get_xlda(void);    /* accelerometer new data available */
uint8_t  imu_get_gda(void);     /* gyroscope new data available */
uint8_t  imu_get_tda(void);     /* temperature new data available */

/* ── Write access (Driver / Proxy only) ── */

void imu_set_quaternion(int16_t qw, int16_t qx, int16_t qy, int16_t qz);
void imu_set_gyro(int16_t gx, int16_t gy, int16_t gz);
void imu_set_accel(int16_t ax, int16_t ay, int16_t az);
void imu_set_temp(int16_t temp);
void imu_set_timestamp_ms(uint32_t ts);
void imu_inc_sample_count(void);
void imu_set_ready(uint8_t ready);
void imu_set_whoami(uint8_t whoami);
void imu_set_init_err(uint8_t err);
void imu_inc_bus_err(void);
void imu_set_cfg_readback(uint8_t ctrl3, uint8_t ctrl1_xl, uint8_t ctrl2_g);
void imu_set_cfg_post_sflp(uint8_t ctrl1_xl, uint8_t ctrl2_g, uint8_t ctrl8_xl, uint8_t ctrl6_g);
void imu_set_cfg_ctrl3_boot(uint8_t val);
void imu_set_cfg_fca(uint8_t val);
void imu_set_sflp_diag(uint8_t en_a, uint8_t init_a, uint8_t exec,
                       uint8_t fifo_ena, uint8_t fs1, uint8_t fs2);
void imu_set_status(uint8_t status_raw);

#ifdef __cplusplus
}
#endif

#endif /* LSM6DSV16X_REG_H */
