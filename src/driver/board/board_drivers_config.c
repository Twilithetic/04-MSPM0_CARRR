/*
 *  ======== board_drivers_config.c ========
 *  TI Driver configuration objects (GPIO_config, I2C_config, I2C_count).
 *
 *  Minimal config — only what I2C_transfer / I2C_open needs at link time.
 *  This replaces the auto-generated ti_drivers_config.c.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include <ti/drivers/I2C.h>
#include <ti/drivers/i2c/I2CMSPM0.h>
#include <ti/drivers/GPIO.h>
#include <ti/drivers/gpio/GPIOMSPM0.h>

/* ── GPIO stub config (not needed by I2C after DL_SYSCFG init) ── */

GPIO_PinConfig gpioPinConfigs[60] = {0};
GPIO_CallbackFxn gpioCallbackFunctions[60];
void *gpioUserArgs[60];

const uint_least8_t GPIO_pinLowerBound = 0;
const uint_least8_t GPIO_pinUpperBound = 60;

const GPIO_Config GPIO_config = {
    .configs   = (GPIO_PinConfig *) gpioPinConfigs,
    .callbacks = gpioCallbackFunctions,
    .userArgs  = gpioUserArgs,
    .intPriority = (~0),
};

/* ── I2C config — PA0/SDA, PA1/SCL, I2C0, 400 kHz ── */

#define I2C_CLOCK_MHZ  32

static I2CMSPM0_Object i2cObject;

static const I2CMSPM0_HWAttrs i2cHWAttrs = {
    .i2c                     = I2C0,
    .intNum                  = I2C0_INT_IRQn,
    .intPriority             = (~0),
    .sdaPincm                = IOMUX_PINCM1,       /* PA0 */
    .sdaPinIndex             = 0,
    .sdaPinMux               = IOMUX_PINCM1_PF_I2C0_SDA,
    .sclPincm                = IOMUX_PINCM2,       /* PA1 */
    .sclPinIndex             = 1,
    .sclPinMux               = IOMUX_PINCM2_PF_I2C0_SCL,
    .clockSource             = DL_I2C_CLOCK_BUSCLK,
    .clockDivider            = DL_I2C_CLOCK_DIVIDE_1,
    .txIntFifoThr            = DL_I2C_TX_FIFO_LEVEL_BYTES_1,
    .rxIntFifoThr            = DL_I2C_RX_FIFO_LEVEL_BYTES_1,
    .isClockStretchingEnabled = true,
    .i2cClk                  = I2C_CLOCK_MHZ,
};

const I2C_Config I2C_config[1] = {
    { .object = &i2cObject, .hwAttrs = &i2cHWAttrs },
};

const uint_least8_t I2C_count = 1;
