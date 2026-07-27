/*
 *  ======== LSM6DSV16X.c ========
 *  LSM6DSV16X IMU Proxy — ST official driver → shadow register.
 *
 *  Architecture: Hardware Proxy + Shadow Register pattern.
 *    - Client tasks read g_imu_shadow (via accessors).
 *    - This Proxy calls the ST official driver APIs (I2C → ctx callbacks)
 *      and writes the shadow register.
 *
 *  Hardware:
 *    I2C0  PA0/SDA  PA1/SCL @ 100 kHz
 *    LSM6DSV16X 7-bit address: auto-detected (0x6A or 0x6B).
 */

#include "include/lsm6dsv16x_reg.h"       /* ST official: stmdev_ctx_t + all APIs */
#include "include/imu_shadow.h"           /* shadow register accessors */
#include "include/i2c_scanner_reg.h"      /* bus-scan presence map */
#include "include/XDS110_cdc.h"           /* uart_send_async */
#include "../chip/ti_drivers_i2c_config.h" /* I2C_Handle, I2C_Transaction */

#include <FreeRTOS.h>
#include <task.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

/* ---- I2C handle (extern from I2C_test.c) ---- */
extern I2C_Handle g_i2cHandle;

/*
 *  Proxy addressing info (guideline §4.1: 只存寻址信息).
 *  Initialised to 0x6B; updated by is_present/init at runtime.
 */
static uint8_t s_imu_addr = 0x6BU;

/* ---- Platform ctx (local to this TU) ---- */
static stmdev_ctx_t g_imu_ctx;

/* ---- Platform I²C + delay callbacks ---- */
static int32_t platform_write(void *h, uint8_t reg, const uint8_t *buf, uint16_t len)
{
    I2C_Handle i2c = (I2C_Handle)h;
    if (!i2c || len + 1U > 16) return -1;
    uint8_t wbuf[16];
    wbuf[0] = reg;
    for (uint16_t i = 0; i < len; i++) wbuf[i + 1] = buf[i];
    I2C_Transaction txn = {0};
    txn.targetAddress = s_imu_addr;
    txn.writeBuf      = wbuf;
    txn.writeCount    = len + 1U;
    txn.readBuf       = NULL;
    txn.readCount     = 0;
    return I2C_transfer(i2c, &txn) ? 0 : -1;
}

static int32_t platform_read(void *h, uint8_t reg, uint8_t *buf, uint16_t len)
{
    I2C_Handle i2c = (I2C_Handle)h;
    if (!i2c) return -1;
    I2C_Transaction txn = {0};
    txn.targetAddress = s_imu_addr;
    txn.writeBuf      = &reg;
    txn.writeCount    = 1;
    txn.readBuf       = buf;
    txn.readCount     = len;
    return I2C_transfer(i2c, &txn) ? 0 : -1;
}

static void platform_mdelay(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

/* ====================================================================
 *  Public API
 * ==================================================================== */
#include <stdint.h>
#include <string.h>
#include <math.h>

/* ---- SFLP quaternion conversion — ST official sflp2q (half-float decode) ---- */
static void sflp2q(float quat[4], const uint16_t sflp[3])
{
    float sumsq = 0.0f;

    /* SFLP data is half-float (float16), NOT fixed-point.
     * Use the official driver's f16→f32 conversion. */
    for (int i = 0; i < 3; i++) {
        union { float f; uint32_t bits; } conv;
        conv.bits = lsm6dsv16x_from_f16_to_f32(sflp[i]);
        quat[i] = conv.f;
        sumsq  += quat[i] * quat[i];
    }

    /* Normalise if |v| > 1 (rare floating-point overshoot) */
    if (sumsq > 1.0f) {
        float n = sqrtf(sumsq);
        quat[0] /= n;
        quat[1] /= n;
        quat[2] /= n;
        sumsq = 1.0f;
    }

    /* W from unit sphere: qw = sqrt(1 - (qx² + qy² + qz²)) */
    quat[3] = sqrtf(1.0f - sumsq);
}

/*
 *  Euler angles from game rotation vector quaternion.
 *
 *  Sensor-frame quaternion {qx, qy, qz, qw} is remapped to human body frame
 *  (X=sensor-Y, Y=sensor-Z, Z=sensor-X) per ST's reference Euler code.
 *  Returns yaw/pitch/roll in degrees * 100.
 */
static void quat_to_euler(const float q[4], int16_t *yaw, int16_t *pit, int16_t *rol)
{
    /* axis remap: human X ← sensor Y, human Y ← sensor Z, human Z ← sensor X */
    float sx = q[1];  /* sensor Y */
    float sy = q[2];  /* sensor Z */
    float sz = q[0];  /* sensor X */
    float sw = q[3];

    float sqx = sx * sx, sqy = sy * sy, sqz = sz * sz;

    float y   = -atan2f(2.0f * (sy * sw + sx * sz), 1.0f - 2.0f * (sqy + sqx));
    float p   = -atan2f(2.0f * (sx * sy + sz * sw), 1.0f - 2.0f * (sqx + sqz));
    float r   = -asinf(2.0f * (sx * sw - sy * sz));

    *yaw = (int16_t)(y * 5729.578f);
    *pit = (int16_t)(p * 5729.578f);
    *rol = (int16_t)(r * 5729.578f);
}

/* ====================================================================
 *  Public API
 * ==================================================================== */

bool lsm6dsv16x_is_present(void)
{
    return i2c_scan_get_ack(0x6AU) || i2c_scan_get_ack(0x6BU);
}

/*
 *  Init — full sequence via the ST official driver.
 *  The official lsm6dsv16x_init_set() does a software-reset internally
 *  so we don't duplicate reset/boot-wait logic.
 */

bool lsm6dsv16x_init(void)
{
    uint8_t whoami = 0;

    /* ── 0. Wire ctx once ── */
    g_imu_ctx.handle    = g_i2cHandle;
    g_imu_ctx.read_reg  = platform_read;
    g_imu_ctx.write_reg = platform_write;
    g_imu_ctx.mdelay     = platform_mdelay;

    s_imu_addr = i2c_scan_get_ack(0x6AU) ? 0x6AU : 0x6BU;

    /* ── 1. Device ID ── */
    if (lsm6dsv16x_device_id_get(&g_imu_ctx, &whoami) || whoami != LSM6DSV16X_ID) {
        imu_set_init_err(1);
        return false;
    }
    imu_set_whoami(whoami);

    /* ── 2. Soft reset + wait ── */
    lsm6dsv16x_sw_reset(&g_imu_ctx);
    for (volatile uint32_t d = 4800000U; d; d--) { __asm__ volatile(""); }

    /* ── 3. Basic config ── */
    lsm6dsv16x_block_data_update_set(&g_imu_ctx, PROPERTY_ENABLE);
    lsm6dsv16x_xl_full_scale_set(&g_imu_ctx, LSM6DSV16X_16g);
    lsm6dsv16x_gy_full_scale_set(&g_imu_ctx, LSM6DSV16X_2000dps);

    /* ── 4. ODR: 240 Hz (sensors + SFLP aligned) ── */
    lsm6dsv16x_xl_data_rate_set(&g_imu_ctx, LSM6DSV16X_ODR_AT_240Hz);
    lsm6dsv16x_gy_data_rate_set(&g_imu_ctx, LSM6DSV16X_ODR_AT_240Hz);
    lsm6dsv16x_sflp_data_rate_set(&g_imu_ctx, LSM6DSV16X_SFLP_240Hz);

    /* ── 5. SFLP FIFO batching ── */
    {
        lsm6dsv16x_fifo_sflp_raw_t sflp = {0};
        sflp.game_rotation = 1;
        sflp.gravity       = 1;
        sflp.gbias         = 1;
        lsm6dsv16x_fifo_sflp_batch_set(&g_imu_ctx, sflp);
    }

    /* ── 6. SFLP enable + bias init ── */
    lsm6dsv16x_sflp_game_rotation_set(&g_imu_ctx, PROPERTY_ENABLE);
    {
        lsm6dsv16x_sflp_gbias_t gb = {0};
        lsm6dsv16x_sflp_game_gbias_set(&g_imu_ctx, &gb);
    }

    /* ── 7. FIFO stream mode ── */
    lsm6dsv16x_fifo_mode_set(&g_imu_ctx, LSM6DSV16X_STREAM_MODE);

    /* ── 8. Diag readback (init only, not in hot path) ── */
    {
        uint8_t a, b, c, d, e, f, g, h, i, j, k, l;
        lsm6dsv16x_read_reg(&g_imu_ctx, LSM6DSV16X_CTRL3, &a, 1);
        lsm6dsv16x_read_reg(&g_imu_ctx, LSM6DSV16X_CTRL1, &b, 1);
        lsm6dsv16x_read_reg(&g_imu_ctx, LSM6DSV16X_CTRL2, &c, 1);
        imu_set_cfg_readback(a, b, c);

        lsm6dsv16x_mem_bank_set(&g_imu_ctx, LSM6DSV16X_EMBED_FUNC_MEM_BANK);
        lsm6dsv16x_read_reg(&g_imu_ctx, LSM6DSV16X_EMB_FUNC_EN_A,       &d, 1);
        lsm6dsv16x_read_reg(&g_imu_ctx, LSM6DSV16X_EMB_FUNC_INIT_A,     &e, 1);
        lsm6dsv16x_read_reg(&g_imu_ctx, LSM6DSV16X_EMB_FUNC_EXEC_STATUS,&f, 1);
        lsm6dsv16x_read_reg(&g_imu_ctx, LSM6DSV16X_EMB_FUNC_FIFO_EN_A,  &g, 1);
        lsm6dsv16x_read_reg(&g_imu_ctx, LSM6DSV16X_SFLP_ODR,            &h, 1);
        lsm6dsv16x_read_reg(&g_imu_ctx, LSM6DSV16X_PAGE_SEL,            &i, 1);
        lsm6dsv16x_mem_bank_set(&g_imu_ctx, LSM6DSV16X_MAIN_MEM_BANK);
        lsm6dsv16x_read_reg(&g_imu_ctx, LSM6DSV16X_FIFO_STATUS1, &j, 1);
        lsm6dsv16x_read_reg(&g_imu_ctx, LSM6DSV16X_FIFO_STATUS2, &k, 1);
        imu_set_sflp_diag(d, e, f, g, j, k, h, i);

        lsm6dsv16x_read_reg(&g_imu_ctx, LSM6DSV16X_CTRL1, &b, 1);
        lsm6dsv16x_read_reg(&g_imu_ctx, LSM6DSV16X_CTRL2, &c, 1);
        lsm6dsv16x_read_reg(&g_imu_ctx, LSM6DSV16X_CTRL8, &l, 1);
        lsm6dsv16x_read_reg(&g_imu_ctx, LSM6DSV16X_CTRL6, &a, 1);
        imu_set_cfg_post_sflp(b, c, l, a);
    }

    imu_set_init_err(0);
    imu_set_ready(1);
    return true;
}

/* ====================================================================
 *  sync_from_device — 100 Hz periodic (called from FreeRTOS task)
 * ==================================================================== */

void lsm6dsv16x_sync_from_device(void)
{
    if (!imu_is_ready()) { return; }

    uint8_t buf[12];
    int16_t raw;

    /* ---- STATUS_REG ---- */
    {
        uint8_t st = 0;
        if (lsm6dsv16x_read_reg(&g_imu_ctx, LSM6DSV16X_STATUS_REG, &st, 1) == 0) {
            imu_set_status(st);
        } else {
            imu_inc_bus_err();
        }
    }

    /* ---- Temperature ---- */
    if (lsm6dsv16x_temperature_raw_get(&g_imu_ctx, &raw) == 0) {
        imu_set_temp(raw);
    } else {
        imu_inc_bus_err();
    }

    /* ---- Accel raw (6 bytes burst) ---- */
    if (lsm6dsv16x_read_reg(&g_imu_ctx, LSM6DSV16X_OUTX_L_A, buf, 6) == 0) {
        raw = (int16_t)(((uint16_t)buf[1] << 8) | buf[0]);
        imu_set_accel(raw,
                      (int16_t)(((uint16_t)buf[3] << 8) | buf[2]),
                      (int16_t)(((uint16_t)buf[5] << 8) | buf[4]));
    } else {
        imu_inc_bus_err();
    }

    /* ---- Gyro raw (6 bytes burst) ---- */
    if (lsm6dsv16x_read_reg(&g_imu_ctx, LSM6DSV16X_OUTX_L_G, buf, 6) == 0) {
        raw = (int16_t)(((uint16_t)buf[1] << 8) | buf[0]);
        imu_set_gyro(raw,
                     (int16_t)(((uint16_t)buf[3] << 8) | buf[2]),
                     (int16_t)(((uint16_t)buf[5] << 8) | buf[4]));
    } else {
        imu_inc_bus_err();
    }

    /* ---- SFLP quaternion from FIFO (main page only — no bank switch in hot path) ---- */
    {
        lsm6dsv16x_fifo_status_t fs = {0};
        lsm6dsv16x_fifo_status_get(&g_imu_ctx, &fs);

        /* Drain FIFO — keep last quaternion entry */
        uint16_t n = fs.fifo_level;
        uint8_t drained = 0;
        while (n-- && drained < 64) {
            lsm6dsv16x_fifo_out_raw_t fd;
            if (lsm6dsv16x_fifo_out_raw_get(&g_imu_ctx, &fd) != 0) { break; }
            drained++;
            if (fd.tag == LSM6DSV16X_SFLP_GAME_ROTATION_VECTOR_TAG) {
                float quat[4];
                uint16_t sflp_raw[3] = {
                    (uint16_t)fd.data[0] | ((uint16_t)fd.data[1] << 8),
                    (uint16_t)fd.data[2] | ((uint16_t)fd.data[3] << 8),
                    (uint16_t)fd.data[4] | ((uint16_t)fd.data[5] << 8),
                };
                sflp2q(quat, sflp_raw);
                imu_set_quaternion(
                    (int16_t)(quat[3] / 0.061f),
                    (int16_t)(quat[0] / 0.061f),
                    (int16_t)(quat[1] / 0.061f),
                    (int16_t)(quat[2] / 0.061f));
                imu_inc_q_count();

                /* Decode yaw/pitch/roll from quaternion */
                {
                    int16_t y, p, r;
                    quat_to_euler(quat, &y, &p, &r);
                    imu_set_euler(y, p, r);
                }
            }
        }
    }

    imu_inc_sample_count();
    imu_set_timestamp_ms(0);  /* TODO: use real tick */
}
