/*
 *  ======== line_reg.h ========
 *  Line sensor (5-channel gray-scale) shadow registers.
 *  All fields are volatile — updated by line proxy via GPIO read.
 *
 *  Data flow:
 *    sync_from_device -> line proxy (GPIO read) writes all fields
 */

#ifndef LINE_REG_H
#define LINE_REG_H

#include "ti_msp_dl_config.h"
#include <stdint.h>

/// Line sensor shadow register (one global instance: g_line_reg)
typedef struct {
    /* ---- sync_from_device: updated by line proxy reading GPIO ---- */
    volatile uint8_t raw[5];          // 0/1, raw GPIO pin levels (S1-S5)
    volatile uint8_t line[5];         // 0/1, after threshold (0=black/line)
    volatile uint8_t active_count;    // count, number of sensors on line
    volatile uint8_t mask;            // bitmask, bit i set if sensor i+1 active
    volatile int16_t position;        // 0-4000, weighted center (2000 = center)
    volatile int16_t error;           // deviation from center = position - 2000
} LineReg;

extern LineReg g_line_reg;

#endif /* LINE_REG_H */
