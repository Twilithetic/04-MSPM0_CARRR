/*
 *  ======== ti_drivers_i2c_config.c ========
 *  Minimal TI Drivers I2C + DPL GPIO configuration.
 *  Adapted from MSPM0 SDK i2c_controller example.
 *  I2C0: PA0=SDA, PA1=SCL @ 32 MHz BUSCLK, 400 kHz.
 */

#include "ti_drivers_i2c_config.h"

#include <stddef.h>

/* ================================================================
 *  DPL GPIO — required by TI Drivers I2C internally
 * ================================================================ */

#include <ti/drivers/GPIO.h>
#include <ti/drivers/gpio/GPIOMSPM0.h>

const uint_least8_t GPIO_pinLowerBound = 0;
const uint_least8_t GPIO_pinUpperBound = 60;

GPIO_PinConfig gpioPinConfigs[60] = {
    /* PA0  */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PA1  */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PA2  */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PA3  */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PA4  */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PA5  */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PA6  */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PA7  */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PA8  */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PA9  */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PA10 */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PA11 */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PA12 */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PA13 */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PA14 */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PA15 */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PA16 */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PA17 */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PA18 */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PA19 */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PA20 */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PA21 */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PA22 */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PA23 */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PA24 */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PA25 */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PA26 */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PA27 */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PA28 */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PA29 */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PA30 */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PA31 */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PB0  */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PB1  */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PB2  */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PB3  */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PB4  */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PB5  */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PB6  */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PB7  */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PB8  */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PB9  */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PB10 */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PB11 */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PB12 */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PB13 */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PB14 */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PB15 */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PB16 */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PB17 */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PB18 */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PB19 */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PB20 */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PB21 */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PB22 */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PB23 */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PB24 */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PB25 */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PB26 */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
    /* PB27 */ GPIO_CFG_INPUT | GPIO_DO_NOT_CONFIG,
};

GPIO_CallbackFxn gpioCallbackFunctions[60];
void *gpioUserArgs[60];

const GPIO_Config GPIO_config = {
    .configs    = (GPIO_PinConfig *) gpioPinConfigs,
    .callbacks  = (GPIO_CallbackFxn *) gpioCallbackFunctions,
    .userArgs   = gpioUserArgs,
    .intPriority = (~0),
};

/* ================================================================
 *  TI Drivers I2C
 * ================================================================ */

/* ---- I2C driver object ---- */
I2CMSPM0_Object i2cMSPM0Objects[I2C_COUNT];

/* ---- I2C hardware attributes ---- */
const I2CMSPM0_HWAttrs i2cMSPM0HWAttrs[I2C_COUNT] = {
    /* CONFIG_I2C_0: I2C0 @ 32 MHz, PA0/PA1 */
    {
        .i2c                        = I2C_INST,
        .intNum                     = I2C_INST_INT_IRQN,
        .intPriority                = (~0),

        .sdaPincm                   = GPIO_I2C_IOMUX_SDA,
        .sdaPinIndex                = GPIO_I2C_SDA_PIN,
        .sdaPinMux                  = GPIO_I2C_IOMUX_SDA_FUNC,

        .sclPincm                   = GPIO_I2C_IOMUX_SCL,
        .sclPinIndex                = GPIO_I2C_SCL_PIN,
        .sclPinMux                  = GPIO_I2C_IOMUX_SCL_FUNC,

        .clockSource                = DL_I2C_CLOCK_BUSCLK,
        .clockDivider               = DL_I2C_CLOCK_DIVIDE_1,
        .txIntFifoThr               = DL_I2C_TX_FIFO_LEVEL_BYTES_1,
        .rxIntFifoThr               = DL_I2C_RX_FIFO_LEVEL_BYTES_1,
        .isClockStretchingEnabled   = true,
        .i2cClk                     = I2C_CLOCK_MHZ,
    },
};

/* ---- I2C config table ---- */
const I2C_Config I2C_config[I2C_COUNT] = {
    { .object = &i2cMSPM0Objects[CONFIG_I2C_0],
      .hwAttrs = &i2cMSPM0HWAttrs[CONFIG_I2C_0] },
};

const uint_least8_t I2C_count = I2C_COUNT;
