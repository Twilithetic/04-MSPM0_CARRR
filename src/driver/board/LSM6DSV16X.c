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

#include "include/lsm6dsv16x_platform.h"  /* g_imu_ctx, ST official reg.h */
#include "include/imu_shadow.h"           /* shadow register accessors */
#include "include/i2c_scanner_reg.h"      /* bus-scan presence map */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

/* ---- SFLP quaternion conversion (official driver scale) ---- */
static void sflp2q(float quat[4], const uint16_t sflp[3])
{
    float sx = lsm6dsv16x_from_sflp_to_mg((int16_t)sflp[0]);
    float sy = lsm6dsv16x_from_sflp_to_mg((int16_t)sflp[1]);
    float sz = lsm6dsv16x_from_sflp_to_mg((int16_t)sflp[2]);
    float qx = sx * 0.061f;
    float qy = sy * 0.061f;
    float qz = sz * 0.061f;
    float qw_sq = 1.0f - (qx*qx + qy*qy + qz*qz);
    if (qw_sq < 0.0f) { qw_sq = 0.0f; }
    quat[0] = qx;
    quat[1] = qy;
    quat[2] = qz;
    quat[3] = sqrtf(qw_sq);
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

static uint8_t s_imu_addr = 0x6BU;  /* resolved by is_present */

bool lsm6dsv16x_init(void)
{
    uint8_t whoami = 0;

    lsm6dsv16x_platform_init();

    /* ---- 0. Resolve I2C address ---- */
    if (i2c_scan_get_ack(0x6AU)) {
        s_imu_addr = 0x6AU;
    } else {
        s_imu_addr = 0x6BU;
    }

    /* ---- 1. Platform ctx already wired by lsm6dsv16x_platform_init() ---- */
    /* ---- 2. Device ID ---- */
    if (lsm6dsv16x_device_id_get(&g_imu_ctx, &whoami) != 0) {
        imu_set_init_err(1);
        return false;
    }
    imu_set_whoami(whoami);
    if (whoami != LSM6DSV16X_ID) {
        imu_set_init_err(1);
        return false;
    }

    /* ---- 3. Software reset ---- */
    lsm6dsv16x_sw_reset(&g_imu_ctx);
    {
        volatile uint32_t d = 4800000U;  /* ~150 ms @ 32 MHz */
        while (d--) { __asm__ volatile(""); }
    }

    /* ---- 4. CTRL3: BDU ---- */
    lsm6dsv16x_block_data_update_set(&g_imu_ctx, PROPERTY_ENABLE);

    /* ---- 5. Full scale ---- */
    lsm6dsv16x_xl_full_scale_set(&g_imu_ctx, LSM6DSV16X_16g);
    lsm6dsv16x_gy_full_scale_set(&g_imu_ctx, LSM6DSV16X_2000dps);

    /* ---- 6. ODR: 30 Hz (matching SFLP 30 Hz per official example) ---- */
    lsm6dsv16x_xl_data_rate_set(&g_imu_ctx, LSM6DSV16X_ODR_AT_30Hz);
    lsm6dsv16x_gy_data_rate_set(&g_imu_ctx, LSM6DSV16X_ODR_AT_30Hz);
    lsm6dsv16x_sflp_data_rate_set(&g_imu_ctx, LSM6DSV16X_SFLP_30Hz);

    /* ---- 7. SFLP: batch game rotation + gravity + gbias into FIFO ---- */
    {
        lsm6dsv16x_fifo_sflp_raw_t sflp = {0};
        sflp.game_rotation = 1;
        sflp.gravity = 1;
        sflp.gbias = 1;
        lsm6dsv16x_fifo_sflp_batch_set(&g_imu_ctx, sflp);
    }

    /* ---- 8. SFLP: enable the game rotation algorithm ---- */
    lsm6dsv16x_sflp_game_rotation_set(&g_imu_ctx, PROPERTY_ENABLE);

    /* ---- 9. Zero gyro bias (app should restore NVM values later) ---- */
    {
        lsm6dsv16x_sflp_gbias_t gb = {0};
        lsm6dsv16x_sflp_game_gbias_set(&g_imu_ctx, &gb);
    }

    /* ---- 10. FIFO continuous (STREAM) mode ---- */
    lsm6dsv16x_fifo_mode_set(&g_imu_ctx, LSM6DSV16X_STREAM_MODE);

    /* ---- 11. Config readback (debug) ---- */
    {
        uint8_t c3 = 0, c1 = 0, c2 = 0;
        lsm6dsv16x_read_reg(&g_imu_ctx, LSM6DSV16X_CTRL3, &c3, 1);
        lsm6dsv16x_read_reg(&g_imu_ctx, LSM6DSV16X_CTRL1, &c1, 1);
        lsm6dsv16x_read_reg(&g_imu_ctx, LSM6DSV16X_CTRL2, &c2, 1);
        imu_set_cfg_readback(c3, c1, c2);
    }

    /* ---- 12. SFLP diag readback ---- */
    {
        uint8_t en_a = 0, init_a = 0, exec_s = 0, fifo_ena = 0;
        uint8_t fs1 = 0, fs2 = 0, odr = 0, pg = 0;

        lsm6dsv16x_mem_bank_set(&g_imu_ctx, LSM6DSV16X_EMBED_FUNC_MEM_BANK);
        lsm6dsv16x_read_reg(&g_imu_ctx, LSM6DSV16X_EMB_FUNC_EN_A, &en_a, 1);
        lsm6dsv16x_read_reg(&g_imu_ctx, LSM6DSV16X_EMB_FUNC_INIT_A, &init_a, 1);
        lsm6dsv16x_read_reg(&g_imu_ctx, LSM6DSV16X_EMB_FUNC_EXEC_STATUS, &exec_s, 1);
        lsm6dsv16x_read_reg(&g_imu_ctx, LSM6DSV16X_EMB_FUNC_FIFO_EN_A, &fifo_ena, 1);
        lsm6dsv16x_read_reg(&g_imu_ctx, LSM6DSV16X_SFLP_ODR, &odr, 1);
        lsm6dsv16x_read_reg(&g_imu_ctx, LSM6DSV16X_PAGE_SEL, &pg, 1);
        lsm6dsv16x_mem_bank_set(&g_imu_ctx, LSM6DSV16X_MAIN_MEM_BANK);
        lsm6dsv16x_read_reg(&g_imu_ctx, LSM6DSV16X_FIFO_STATUS1, &fs1, 1);
        lsm6dsv16x_read_reg(&g_imu_ctx, LSM6DSV16X_FIFO_STATUS2, &fs2, 1);
        imu_set_sflp_diag(en_a, init_a, exec_s, fifo_ena, fs1, fs2, odr, pg);
        imu_set_cfg_fca(0);  /* not read here; official mem_bank API handles it */
    }

    /* ---- 13. Post-SFLP main-page readback ---- */
    {
        uint8_t c1 = 0, c2 = 0, c8 = 0, c6 = 0;
        lsm6dsv16x_read_reg(&g_imu_ctx, LSM6DSV16X_CTRL1, &c1, 1);
        lsm6dsv16x_read_reg(&g_imu_ctx, LSM6DSV16X_CTRL2, &c2, 1);
        lsm6dsv16x_read_reg(&g_imu_ctx, LSM6DSV16X_CTRL8, &c8, 1);
        lsm6dsv16x_read_reg(&g_imu_ctx, LSM6DSV16X_CTRL6, &c6, 1);
        imu_set_cfg_post_sflp(c1, c2, c8, c6);
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

    /* ---- SFLP quaternion from FIFO ---- */
    {
        lsm6dsv16x_fifo_status_t fs = {0};
        lsm6dsv16x_fifo_status_get(&g_imu_ctx, &fs);

        /* Live diag */
        {
            uint8_t en_a = 0, init_a = 0, exec_s = 0, fifo_ena = 0;
            uint8_t odr = 0, pg = 0;
            lsm6dsv16x_mem_bank_set(&g_imu_ctx, LSM6DSV16X_EMBED_FUNC_MEM_BANK);
            lsm6dsv16x_read_reg(&g_imu_ctx, LSM6DSV16X_EMB_FUNC_EN_A, &en_a, 1);
            lsm6dsv16x_read_reg(&g_imu_ctx, LSM6DSV16X_EMB_FUNC_INIT_A, &init_a, 1);
            lsm6dsv16x_read_reg(&g_imu_ctx, LSM6DSV16X_EMB_FUNC_EXEC_STATUS, &exec_s, 1);
            lsm6dsv16x_read_reg(&g_imu_ctx, LSM6DSV16X_EMB_FUNC_FIFO_EN_A, &fifo_ena, 1);
            lsm6dsv16x_read_reg(&g_imu_ctx, LSM6DSV16X_SFLP_ODR, &odr, 1);
            lsm6dsv16x_read_reg(&g_imu_ctx, LSM6DSV16X_PAGE_SEL, &pg, 1);
            lsm6dsv16x_mem_bank_set(&g_imu_ctx, LSM6DSV16X_MAIN_MEM_BANK);
            imu_set_sflp_diag(en_a, init_a, exec_s, fifo_ena,
                              (uint8_t)(fs.fifo_level & 0xFF),
                              (uint8_t)((fs.fifo_level >> 8) & 0xFF),
                              odr, pg);
        }

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
                    (int16_t)(quat[3] / 0.061f),  /* W */
                    (int16_t)(quat[0] / 0.061f),  /* X */
                    (int16_t)(quat[1] / 0.061f),  /* Y */
                    (int16_t)(quat[2] / 0.061f)); /* Z */
            }
        }
    }

    imu_inc_sample_count();
    imu_set_timestamp_ms(0);  /* TODO: use real tick */
}
