/*
 *  ======== ti_drivers_i2c_config.h ========
 *  Minimal TI Drivers I2C configuration.
 *  Adapted from MSPM0 SDK i2c_controller example.
 *  I2C0: PA0=SDA, PA1=SCL @ 32 MHz BUSCLK.
 */

#ifndef TI_DRIVERS_I2C_CONFIG_H
#define TI_DRIVERS_I2C_CONFIG_H

#include <stdint.h>

#include <ti/devices/msp/msp.h>
#include <ti/driverlib/driverlib.h>
#include <ti/drivers/I2C.h>
#include <ti/drivers/i2c/I2CMSPM0.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- I2C0: PA0=SDA, PA1=SCL ---- */
/* PA0 = pin index 0,  IOMUX_PINCM1  */
/* PA1 = pin index 1,  IOMUX_PINCM2  */

#define CONFIG_I2C_0              0
#define I2C_COUNT                 1

#define I2C_INST                  I2C0
#define I2C_INST_INT_IRQN         I2C0_INT_IRQn

#define GPIO_I2C_SDA_PIN          (0)                     /* PA0 */
#define GPIO_I2C_IOMUX_SDA        (IOMUX_PINCM1)
#define GPIO_I2C_IOMUX_SDA_FUNC   IOMUX_PINCM1_PF_I2C0_SDA

#define GPIO_I2C_SCL_PIN          (1)                     /* PA1 */
#define GPIO_I2C_IOMUX_SCL        (IOMUX_PINCM2)
#define GPIO_I2C_IOMUX_SCL_FUNC   IOMUX_PINCM2_PF_I2C0_SCL

/* ---- Clock (MCLK = SYSOSC = 32 MHz) ---- */
#define I2C_CLOCK_MHZ             32

/* ---- Max speed ---- */
#define CONFIG_I2C_MAXSPEED       400U   /* kbps */
#define CONFIG_I2C_MAXBITRATE     ((I2C_BitRate) I2C_400kHz)

/* ---- I2C config (defined in ti_drivers_i2c_config.c) ---- */
extern const I2C_Config I2C_config[I2C_COUNT];
extern const uint_least8_t I2C_count;

#ifdef __cplusplus
}
#endif

#endif /* TI_DRIVERS_I2C_CONFIG_H */
