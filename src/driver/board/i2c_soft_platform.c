/*
 *  ======== i2c_soft_platform.c ========
 *  Platform overrides for I2C_soft.c weak functions.
 *
 *  I2C_soft.c declares __weak defaults for I2C_SDA_Set/I2C_delay_us etc.
 *  This file provides STRONG (non-weak) definitions that override them,
 *  adapted for MSPM0G3507 + FreeRTOS.
 *
 *  Pin mapping (from SysConfig MOTOR_I2C in ti_msp_dl_config.h):
 *    PA15 = SCL, PA16 = SDA
 *
 *  Delay: vTaskDelay(pdMS_TO_TICKS(1)) — 1ms minimum for FreeRTOS.
 */

#include "ti_msp_dl_config.h"

#include <FreeRTOS.h>
#include <task.h>
#include <stdint.h>
#include <stdbool.h>

/* ---- Pin defines from SysConfig ---- */

#define I2C_PORT     MOTOR_I2C_PORT
#define I2C_SCL_PIN  MOTOR_I2C_SCL_PIN
#define I2C_SDA_PIN  MOTOR_I2C_SDA_PIN

/* ---- Strong overrides (replace I2C_soft.c weak defaults) ---- */

void I2C_SDA_Set(void)   { DL_GPIO_setPins(I2C_PORT, I2C_SDA_PIN); }
void I2C_SDA_Clr(void)   { DL_GPIO_clearPins(I2C_PORT, I2C_SDA_PIN); }
void I2C_SCL_Set(void)   { DL_GPIO_setPins(I2C_PORT, I2C_SCL_PIN); }
void I2C_SCL_Clr(void)   { DL_GPIO_clearPins(I2C_PORT, I2C_SCL_PIN); }

uint8_t I2C_SDA_Read(void) {
    return (DL_GPIO_readPins(I2C_PORT, I2C_SDA_PIN) & I2C_SDA_PIN) ? 1 : 0;
}

void I2C_delay_us(uint8_t us) {
    /* delay_cycles() is TI DriverLib, 1 cycle = 31.25ns @ 32MHz.
     * For 100kHz I2C: half period = 5µs → delay_cycles(160). */
    delay_cycles((uint32_t)us * 32U);
}
