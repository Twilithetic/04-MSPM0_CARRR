/*
 *  ======== lsm6dsv16x_platform.c ========
 *  I²C transport layer for the STMems official LSM6DSV16X driver.
 *
 *  Implements stmdev_read_ptr / stmdev_write_ptr callbacks over TI Drivers
 *  I²C, and inits the stmdev_ctx_t global instance.
 */

#include "include/lsm6dsv16x_platform.h"
#include "../chip/ti_drivers_i2c_config.h"

#include <stdbool.h>
#include <string.h>

#include <FreeRTOS.h>
#include <task.h>

/* ---- global ctx ---- */
stmdev_ctx_t g_imu_ctx;

/* ---- RTOS delay for ST driver multi-byte reads ---- */
static void platform_mdelay(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

/* ===================================================================
 *  I²C callbacks
 * =================================================================== */

/*
 *  Platform write: single-byte reg → multi-byte data.
 *  TI Drivers I2C_transfer always does a repeated-start for write+read.
 *  For a write-only transfer we send {reg, data[0..len-1]} in one writeBuf.
 *
 *  The `handle` parameter is g_i2cHandle cast to void*.
 */
int32_t lsm6dsv16x_platform_write(void *handle, uint8_t reg,
                                  const uint8_t *buf, uint16_t len)
{
    I2C_Handle i2c = (I2C_Handle)handle;
    if (i2c == NULL) { return -1; }

    /*
     * Combine reg + data into a single write buffer so TI Drivers issues
     * one write transaction: START | ADDR+W | reg | data[0..len-1] | STOP.
     * Most STMems register writes are 1-2 bytes beyond the reg byte,
     * so a 16-byte static buffer safely covers all practical cases.
     */
    uint8_t wbuf[16];
    if (len + 1U > sizeof(wbuf)) { return -1; }

    wbuf[0] = reg;
    (void)memcpy(&wbuf[1], buf, len);

    I2C_Transaction txn = {0};
    txn.targetAddress = 0x6BU;
    txn.writeBuf      = wbuf;
    txn.writeCount    = (uint16_t)(len + 1U);
    txn.readBuf       = NULL;
    txn.readCount     = 0;

    return I2C_transfer(i2c, &txn) ? 0 : -1;
}

/*
 *  Platform read: send reg → repeated-start → read len bytes.
 */
int32_t lsm6dsv16x_platform_read(void *handle, uint8_t reg,
                                 uint8_t *buf, uint16_t len)
{
    I2C_Handle i2c = (I2C_Handle)handle;
    if (i2c == NULL) { return -1; }

    I2C_Transaction txn = {0};
    txn.targetAddress = 0x6BU;
    txn.writeBuf      = &reg;
    txn.writeCount    = 1;
    txn.readBuf       = buf;
    txn.readCount     = (uint16_t)len;

    return I2C_transfer(i2c, &txn) ? 0 : -1;
}

/* ===================================================================
 *  ctx initialisation
 * =================================================================== */

bool lsm6dsv16x_platform_init(void)
{
    g_imu_ctx.handle    = g_i2cHandle;
    g_imu_ctx.read_reg  = lsm6dsv16x_platform_read;
    g_imu_ctx.write_reg = lsm6dsv16x_platform_write;
    g_imu_ctx.mdelay    = platform_mdelay;

    /* Quick sanity: read WHO_AM_I */
    uint8_t whoami = 0;
    if (lsm6dsv16x_read_reg(&g_imu_ctx, 0x0FU, &whoami, 1) != 0) {
        return false;
    }
    if (whoami != LSM6DSV16X_ID) {
        return false;
    }
    return true;
}
