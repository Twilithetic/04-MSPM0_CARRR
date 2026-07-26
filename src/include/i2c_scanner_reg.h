/*
 *  ======== i2c_scanner_reg.h ========
 *  I2C bus-scan result shadow register — opaque type.
 *  Full struct definition is private to registers.c.
 */

#ifndef I2C_SCANNER_REG_H
#define I2C_SCANNER_REG_H

#include <stdbool.h>
#include <stdint.h>

#define I2C_SCAN_MAX_DEVICES  16U      /* max devices to record */
#define I2C_SCAN_ADDR_RANGE   128U     /* 7-bit: 0x00–0x7F */

typedef struct I2cScanReg I2cScanReg;

extern I2cScanReg g_i2c_scan_reg;

/* ── Read access (Client / Logger) ── */

uint8_t  i2c_scan_get_count(void);                  /* how many found */
uint8_t  i2c_scan_get_addr(uint8_t idx);             /* address of device[idx] */
uint8_t  i2c_scan_get_whoami(uint8_t idx);           /* WHO_AM_I byte (0 if N/A) */
bool     i2c_scan_get_ack(uint8_t addr);             /* did <addr> ACK? */

/* ── Write access (Driver / Proxy only) ── */

void i2c_scan_set_count(uint8_t cnt);
void i2c_scan_add_device(uint8_t addr, uint8_t whoami);  /* push one device */
void i2c_scan_clear_all(void);                            /* reset scan results */
void i2c_scan_set_ack(uint8_t addr, bool present);

#endif /* I2C_SCANNER_REG_H */
