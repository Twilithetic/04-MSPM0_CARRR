/*
 *  ======== I2C_test.c ========
 *  I2C bus scanner — probes every 7-bit address on I2C0,
 *  reads WHO_AM_I where available, stores results in g_i2c_scan_reg.
 *
 *  Hardware:
 *    I2C0  PA0/SDA  PA1/SCL @ 400 kHz  (ti_drivers_i2c_config.h)
 *
 *  Uses TI Drivers I2C API (not DriverLib).
 *  Non-blocking: I2C_transfer() runs to completion but yields on semaphore
 *  internally, so it's safe in a FreeRTOS task.
 */

#include "include/i2c_scanner_reg.h"
#include "ti_drivers_i2c_config.h"

#include <stdbool.h>
#include <stdint.h>

/* ---- Global I2C handle (initialized once) ---- */
static I2C_Handle g_i2cHandle = NULL;

/* ====================================================================
 *  Public API
 * ==================================================================== */

void i2c_test_init(void)
{
    I2C_init();

    I2C_Params params;
    I2C_Params_init(&params);
    params.bitRate = I2C_400kHz;

    g_i2cHandle = I2C_open(CONFIG_I2C_0, &params);
}

/*
 *  Probe a single 7-bit address.
 *  Returns true if device ACKed (present on bus).
 */
static bool i2c_probe_addr(uint8_t addr_7bit)
{
    I2C_Transaction txn = {0};
    txn.targetAddress = addr_7bit;
    txn.writeBuf      = NULL;
    txn.writeCount    = 0;
    txn.readBuf       = NULL;
    txn.readCount     = 0;

    bool ok = I2C_transfer(g_i2cHandle, &txn);
    /* I2C_transfer returns true on success; ADDR_NACK → false */
    return ok;
}

/*
 *  Read one register from a given 7-bit address.
 *  Only called on known-present devices.
 */
static uint8_t i2c_read_reg(uint8_t addr_7bit, uint8_t reg)
{
    uint8_t val = 0;

    I2C_Transaction txn = {0};
    txn.targetAddress = addr_7bit;
    txn.writeBuf      = &reg;
    txn.writeCount    = 1;
    txn.readBuf       = &val;
    txn.readCount     = 1;

    I2C_transfer(g_i2cHandle, &txn);
    return val;
}

/*
 *  Scan entire 7-bit address space (0x08–0x77).
 *  For each ACKed device: record address + WHO_AM_I (reg 0x0F) into shadow.
 */
void i2c_scan_bus(void)
{
    i2c_scan_clear_all();

    for (uint16_t addr = 0x08U; addr < 0x78U; addr++) {
        if (i2c_probe_addr((uint8_t) addr)) {
            uint8_t whoami = 0;
            whoami = i2c_read_reg((uint8_t) addr, 0x0FU);
            i2c_scan_add_device((uint8_t) addr, whoami);
        }
    }
}
