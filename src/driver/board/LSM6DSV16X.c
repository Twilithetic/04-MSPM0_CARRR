/*
 *  ======== LSM6DSV16X.c ========
 *  LSM6DSV16X IMU Proxy driver — I2C bus read/write → shadow register.
 *
 *  Architecture: Hardware Proxy + Shadow Register pattern
 *    - Client tasks read g_lsm6dsv16x_reg (opaque, via accessors).
 *    - This Proxy performs all I2C I/O and writes the shadow register.
 *
 *  Feature set (initial version):
 *    - Device ID check (WHO_AM_I = 0x70)
 *    - Accel  @ 240 Hz, ±16g
 *    - Gyro   @ 240 Hz, ±2000 dps
 *    - SFLP Game Rotation Vector (quaternion) via FIFO @ 60 Hz
 *    - sync_imu_from_device() reads accel+gyro+temp → shadow register
 *    - When SFLP FIFO is enabled, reads quaternion tag from FIFO
 *
 *  Hardware:
 *    I2C0  PA0/SDA  PA1/SCL @ 400 kHz
 *    LSM6DSV16X 7-bit address 0x6B (SA0=VDD on most breakouts)
 */

#include "include/lsm6dsv16x_reg.h"
#include "include/i2c_scanner_reg.h"
#include "../chip/ti_drivers_i2c_config.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* ---- External I2C handle (initialized in i2c_test_init) ---- */
extern I2C_Handle g_i2cHandle;

/* ---- SFLP game-specific constants ---- */

/* SFLP game ODR: 60 Hz → CTRL1_XL[3:0] = 0x06 */
#define IMU_ODR_ACCEL        LSM6DSV16X_ODR_XL_240HZ   /* 240 Hz */
#define IMU_ODR_GYRO         LSM6DSV16X_ODR_G_240HZ    /* 240 Hz */
#define IMU_SFLP_ODR_VAL     0x02U                       /* SFLP 60 Hz */

#define IMU_FS_ACCEL         LSM6DSV16X_FS_XL_16G
#define IMU_FS_GYRO          LSM6DSV16X_FS_G_2000DPS

#define IMU_I2C_ADDR         0x6BU

/* ====================================================================
 *  Low-level I2C helpers  (raw_* functions — pure protocol, no biz logic)
 * ==================================================================== */

/*
 *  Write one register byte.  Returns true on ACK.
 */
static bool imu_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    I2C_Transaction txn = {0};
    txn.targetAddress = IMU_I2C_ADDR;
    txn.writeBuf      = buf;
    txn.writeCount    = 2;
    txn.readBuf       = NULL;
    txn.readCount     = 0;
    return I2C_transfer(g_i2cHandle, &txn);
}

/*
 *  Read one register byte.  Returns the value; 0x00 on failure.
 */
static uint8_t imu_read_reg(uint8_t reg)
{
    uint8_t val = 0;
    I2C_Transaction txn = {0};
    txn.targetAddress = IMU_I2C_ADDR;
    txn.writeBuf      = &reg;
    txn.writeCount    = 1;
    txn.readBuf       = &val;
    txn.readCount     = 1;
    I2C_transfer(g_i2cHandle, &txn);
    return val;
}

/*
 *  Multi-byte read starting at `reg`.  `len` bytes → `dst`.
 *  Requires CTRL3.IF_INC = 1 (auto-increment).
 */
static bool imu_read_burst(uint8_t reg, uint8_t *dst, uint8_t len)
{
    I2C_Transaction txn = {0};
    txn.targetAddress = IMU_I2C_ADDR;
    txn.writeBuf      = &reg;
    txn.writeCount    = 1;
    txn.readBuf       = dst;
    txn.readCount     = len;
    return I2C_transfer(g_i2cHandle, &txn);
}

/* ====================================================================
 *  Memory bank switching
 * ==================================================================== */

enum { BANK_MAIN = 0, BANK_EMBED = 1 };

static bool imu_set_bank(uint8_t bank)
{
    /* Read FUNC_CFG_ACCESS: bit 2 = emb_func_reg_access */
    uint8_t cfg = imu_read_reg(LSM6DSV16X_FUNC_CFG_ACCESS);

    if (bank == BANK_EMBED) {
        cfg |= LSM6DSV16X_EMB_FUNC_REG_ACCESS;
    } else {
        cfg &= ~LSM6DSV16X_EMB_FUNC_REG_ACCESS;
    }

    return imu_write_reg(LSM6DSV16X_FUNC_CFG_ACCESS, cfg);
}

/* ====================================================================
 *  Embedded-page register helpers
 * ==================================================================== */

/*
 *  Write a register on the embedded-function page.
 */
static bool imu_embed_write_reg(uint8_t reg, uint8_t val)
{
    bool ok;
    imu_set_bank(BANK_EMBED);
    ok = imu_write_reg(reg, val);
    imu_set_bank(BANK_MAIN);
    return ok;
}

/* ====================================================================
 *  Initialisation
 * ==================================================================== */

/*
 *  Check if the IMU responded to the I2C bus scan.
 *  Uses g_i2c_scan_reg — populated by i2c_scan_bus() before this call.
 *  Returns true if the LSM6DSV16X was found at its expected address.
 */
bool lsm6dsv16x_is_present(void)
{
    return i2c_scan_get_ack(LSM6DSV16X_I2C_ADDR_SA0_0) ||
           i2c_scan_get_ack(LSM6DSV16X_I2C_ADDR_SA0_1);
}

/*
 *  Init LSM6DSV16X.
 *  Sequence:
 *    1. Software reset → wait 100 ms
 *    2. CTRL3: enable IF_INC + BDU
 *    3. CTRL1_XL: accel 240 Hz, HP mode
 *    4. CTRL2_G:  gyro  240 Hz, HP mode
 *    5. CTRL8_XL: accel ±16g
 *    6. CTRL6_G:  gyro  ±2000 dps
 *    7. Embedded bank: enable SFLP game FIFO
 *    8. FIFO_CTRL4: continuous mode
 *
 *  Returns true if WHO_AM_I matches.
 */
bool lsm6dsv16x_init(void)
{
    /* 1. Verify device identity */
    uint8_t whoami = imu_read_reg(LSM6DSV16X_WHO_AM_I);
    if (whoami != LSM6DSV16X_WHO_AM_I_VALUE) {
        return false;
    }

    /* 2. Software reset */
    imu_write_reg(LSM6DSV16X_CTRL3, LSM6DSV16X_SW_RESET);
    {
        volatile uint32_t d = 4800000U;  /* ~150 ms @ 32 MHz */
        while (d--) { __asm__ volatile(""); }
    }

    /* Wait for BOOT flag to clear */
    {
        uint32_t timeout = 100000U;
        while (imu_read_reg(LSM6DSV16X_CTRL3) & LSM6DSV16X_BOOT) {
            if (timeout-- == 0) { return false; }
        }
    }

    /* 3. CTRL3: auto-increment + block data update */
    imu_write_reg(LSM6DSV16X_CTRL3,
                  LSM6DSV16X_IF_INC | LSM6DSV16X_BDU);

    /* 4. CTRL1_XL: accel 240 Hz, HP mode */
    imu_write_reg(LSM6DSV16X_CTRL1_XL,
                  IMU_ODR_ACCEL);   /* bits 6:4 = 000 = HP, bits 3:0 = 240 Hz */

    /* 5. CTRL2_G: gyro 240 Hz, HP mode */
    imu_write_reg(LSM6DSV16X_CTRL2_G,
                  IMU_ODR_GYRO);    /* bits 6:4 = 000 = HP, bits 3:0 = 240 Hz */

    /* 6. CTRL8_XL: accel ±16g */
    imu_write_reg(LSM6DSV16X_CTRL8_XL,
                  IMU_FS_ACCEL);    /* bits 1:0 = 11 = ±16g */

    /* 7. CTRL6_G: gyro ±2000 dps */
    imu_write_reg(LSM6DSV16X_CTRL6_G,
                  IMU_FS_GYRO);     /* bits 3:0 = 0x03 = ±2000 dps */

    /* 8. Enable SFLP game rotation vector in FIFO (embedded page) */
    imu_embed_write_reg(LSM6DSV16X_EMB_FUNC_FIFO_EN_A,
                        LSM6DSV16X_SFLP_GAME_FIFO_EN);

    /* 9. FIFO_CTRL4: FIFO continuous mode */
    imu_write_reg(LSM6DSV16X_FIFO_CTRL4,
                  LSM6DSV16X_FIFO_MODE_CONTINUOUS);

    imu_set_ready(1);

    return true;
}

/* ====================================================================
 *  Data synchronisation — Proxy → Shadow Register
 * ==================================================================== */

/*
 *  Read raw accel + gyro + temp → shadow register.
 *  Uses multi-byte burst reads (CTRL3.IF_INC = auto-increment).
 */
void lsm6dsv16x_sync_raw_sensors(void)
{
    uint8_t  buf[6];
    int16_t  raw;

    /* Temperature: 2 bytes, OUT_TEMP_L */
    if (imu_read_burst(LSM6DSV16X_OUT_TEMP_L, buf, 2)) {
        raw = (int16_t)(((uint16_t)buf[1] << 8) | buf[0]);
        imu_set_temp(raw);
    }

    /* Gyroscope: 6 bytes starting at OUTX_L_G */
    if (imu_read_burst(LSM6DSV16X_OUTX_L_G, buf, 6)) {
        raw = (int16_t)(((uint16_t)buf[1] << 8) | buf[0]);
        imu_set_gyro(raw,
                     (int16_t)(((uint16_t)buf[3] << 8) | buf[2]),
                     (int16_t)(((uint16_t)buf[5] << 8) | buf[4]));
    }

    /* Accelerometer: 6 bytes starting at OUTX_L_A */
    if (imu_read_burst(LSM6DSV16X_OUTX_L_A, buf, 6)) {
        raw = (int16_t)(((uint16_t)buf[1] << 8) | buf[0]);
        imu_set_accel(raw,
                      (int16_t)(((uint16_t)buf[3] << 8) | buf[2]),
                      (int16_t)(((uint16_t)buf[5] << 8) | buf[4]));
    }

    imu_inc_sample_count();
}

/*
 *  Read one SFLP quaternion entry from FIFO (if available).
 *  Entry: 1 tag byte + 6 data bytes = 7 bytes.
 *  Tag 0x13 = game rotation vector.
 *
 *  Returns true if a quaternion was read and stored to shadow.
 */
bool lsm6dsv16x_sync_sflp_quaternion(void)
{
    uint8_t  buf[7];
    int16_t  qx, qy, qz, qw;

    /* Check if FIFO has data */
    uint8_t status2 = imu_read_reg(LSM6DSV16X_FIFO_STATUS2);

    /* Read watermark level (lower 9 bits across STATUS1 + STATUS2[0]) */
    uint8_t  status1  = imu_read_reg(LSM6DSV16X_FIFO_STATUS1);
    uint16_t fifo_lev = (uint16_t)status1 | ((uint16_t)(status2 & 0x01U) << 8);

    if (fifo_lev == 0) {
        return false;
    }

    /* Read one FIFO entry */
    if (!imu_read_burst(LSM6DSV16X_FIFO_DATA_OUT_TAG, buf, 7)) {
        return false;
    }

    uint8_t tag = buf[0];

    if (tag == LSM6DSV16X_SFLP_GAME_ROTATION_VECTOR_TAG) {
        /* Quaternion: X Y Z in FIFO.  W is computed from unit sphere. */
        qx = (int16_t)(((uint16_t)buf[2] << 8) | buf[1]);
        qy = (int16_t)(((uint16_t)buf[4] << 8) | buf[3]);
        qz = (int16_t)(((uint16_t)buf[6] << 8) | buf[5]);

        {
            float fx = (float)qx * 0.061f;
            float fy = (float)qy * 0.061f;
            float fz = (float)qz * 0.061f;
            float fw_sq = 1.0f - (fx*fx + fy*fy + fz*fz);
            if (fw_sq < 0.0f) { fw_sq = 0.0f; }
            qw = (int16_t)(sqrtf(fw_sq) / 0.061f);
        }

        imu_set_quaternion(qw, qx, qy, qz);
        imu_inc_sample_count();
        return true;
    }

    /*
     * Other tags (gyro bias 0x16, gravity 0x17, accel/gyro data)
     * are ignored for now.  They're consumed here so FIFO doesn't
     * overflow, but we don't store them.
     */
    return false;
}

/*
 *  sync_imu_from_device — top-level sync entry point.
 *  1. Pull raw accel + gyro + temp.
 *  2. Drain SFLP quaternion entries from FIFO.
 *
 *  Call this periodically (e.g. every 10 ms) from a FreeRTOS task.
 */
void lsm6dsv16x_sync_from_device(void)
{
    if (!imu_is_ready()) {
        return;
    }

    lsm6dsv16x_sync_raw_sensors();

    /*
     *  Drain ALL pending SFLP quaternion entries from FIFO.
     *  Only the last one survives in the shadow register —
     *  this gives the freshest quaternion for the consumer.
     */
    {
        uint8_t drained = 0;
        while (lsm6dsv16x_sync_sflp_quaternion()) {
            drained++;
            if (drained > 32) {
                /* FIFO is being flooded — something is wrong.
                 * Break to avoid infinite loop. */
                break;
            }
        }
    }

    /* Timestamp: crude tick-based, replace with RTC/SysTick later */
    imu_set_timestamp_ms(0);
}
