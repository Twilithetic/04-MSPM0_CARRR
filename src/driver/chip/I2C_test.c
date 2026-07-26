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
    /* I2C0 peripheral was removed from empty.syscfg, so SYSCFG_DL_init()
     * no longer resets or powers I2C0.  Do it manually here. */
    DL_I2C_reset(I2C0);
    DL_I2C_enablePower(I2C0);
    delay_cycles(16);  /* POWER_STARTUP_DELAY */

    I2C_init();

    I2C_Params params;
    I2C_Params_init(&params);
    params.bitRate = I2C_100kHz;

    g_i2cHandle = I2C_open(CONFIG_I2C_0, &params);
}

/*
 *  Probe a single 7-bit address by writing 1 dummy byte.
 *  Returns true if device ACKed its address (present on bus).
 *  The written data is discarded — we only care about ACK vs NACK.
 *
 *  NOTE: We use a write (not read) for probing because TI Drivers'
 *  I2CMSPM0_primeReadBurst has a bug: the isReadInProgress flag is
 *  never cleared on NACK, causing every subsequent read transaction
 *  to silently nop and hang forever on the transferComplete semaphore.
 *  The write path (I2CMSPM0_primeWriteBurst) resets its state properly
 *  in I2CSupport_primeTransfer on every call.
 */
static bool i2c_probe_addr(uint8_t addr_7bit)
{
    uint8_t dummy = 0;
    I2C_Transaction txn = {0};
    txn.targetAddress = addr_7bit;
    txn.writeBuf      = &dummy;
    txn.writeCount    = 1;
    txn.readBuf       = NULL;
    txn.readCount     = 0;

    bool ok = I2C_transfer(g_i2cHandle, &txn);
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
