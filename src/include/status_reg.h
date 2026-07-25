/*
 *  ======== status_reg.h ========
 *  System status shadow registers.
 *  Maintained by main loop and controller.
 *
 *  Data flow:
 *    Updated by controller_calculate() and main loop
 *    Read by any task for system state
 */

#ifndef STATUS_REG_H
#define STATUS_REG_H

#include "ti_msp_dl_config.h"
#include <stdbool.h>
#include <stdint.h>

/// System status shadow register (one global instance: g_status_reg)
typedef struct {
    /* ---- Status flags set by controller / main loop ---- */
    volatile bool    line_lost;           // all sensors see white (lost)
    volatile bool    line_all_black;      // >=4 sensors see black (cross/stop)
    volatile bool    motor_error;         // true if last motor I2C failed
    volatile uint8_t motor_error_code;    // detail: I2C error step (>0 = fail)
    volatile uint16_t loop_count;         // main loop iteration counter
    volatile bool    initialized;         // motor config completed successfully
    volatile bool    button_pressed;      // KEY pin (PA18) state

    /* ---- Target / command flags ---- */
    volatile bool    target_motor_stop;   // emergency motor stop request
} StatusReg;

extern StatusReg g_status_reg;

#endif /* STATUS_REG_H */
