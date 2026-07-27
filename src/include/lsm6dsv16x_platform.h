/*
 *  ======== lsm6dsv16x_platform.h ========
 *  Platform abstraction for the ST official LSM6DSV16X driver.
 *
 *  Provides:
 *    - stmdev_ctx_t instance (I²C read/write callbacks + handle)
 *    - ctx initialisation
 *    - declaration of the I²C handle used by the callback
 *
 *  The official driver is #included as lsm6dsv16x_reg_official.h.
 *  This file lives in "include/" so the driver's #include "lsm6dsv16x_reg.h"
 *  resolves correctly — we renamed the official header accordingly.
 */

#ifndef LSM6DSV16X_PLATFORM_H
#define LSM6DSV16X_PLATFORM_H

#include <stdint.h>
#include <stdbool.h>

/*
 * The official driver's stmdev_ctx_t is defined in lsm6dsv16x_reg_official.h.
 * We need to pull it in so we can declare the ctx variable.
 *
 * We include our own lsm6dsv16x_reg.h first (shadow register + accessors),
 * then the official register/bit definitions + stmdev_ctx_t.
 */
#include "lsm6dsv16x_reg.h"           /* shadow type + accessors */
#include "lsm6dsv16x_reg.h"  /* official reg map + stmdev_ctx_t + APIs */

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Global I²C handle (defined in I2C_test.c) ---- */
extern void *g_i2cHandle;

/* ---- Platform I²C callbacks ---- */

/** Write `len` bytes starting at register `reg`.  Returns 0 on success. */
int32_t lsm6dsv16x_platform_write(void *handle, uint8_t reg,
                                  const uint8_t *buf, uint16_t len);

/** Read `len` bytes starting at register `reg` into `buf`.  Returns 0 on success. */
int32_t lsm6dsv16x_platform_read(void *handle, uint8_t reg,
                                 uint8_t *buf, uint16_t len);

/* ---- ctx instance ---- */

/** Single ctx shared by all driver API calls.  Initialised by platform_init. */
extern stmdev_ctx_t g_imu_ctx;

/** Wire up the ctx and verify the device is present.  Call once at startup. */
bool lsm6dsv16x_platform_init(void);

#ifdef __cplusplus
}
#endif

#endif /* LSM6DSV16X_PLATFORM_H */
