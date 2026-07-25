/*
 *  ======== status_reg.h ========
 *  Opaque type — full definition is private to registers.c.
 *  All access goes through the functions declared below.
 */

#ifndef STATUS_REG_H
#define STATUS_REG_H

#include "ti_msp_dl_config.h"
#include <stdbool.h>
#include <stdint.h>

/// Opaque handle.  Callers can hold and pass pointers but must use
/// the access functions to read/write fields.
typedef struct StatusReg StatusReg;

/// Global system-status instance (defined in registers.c)
extern StatusReg g_status_reg;

// ---- line_lost ----
void status_set_line_lost(StatusReg *reg, bool val);
bool status_get_line_lost(const StatusReg *reg);

// ---- line_all_black ----
void status_set_line_all_black(StatusReg *reg, bool val);
bool status_get_line_all_black(const StatusReg *reg);

// ---- motor_error ----
void status_set_motor_error(StatusReg *reg, bool val);
bool status_get_motor_error(const StatusReg *reg);

// ---- motor_error_code ----
void status_set_motor_error_code(StatusReg *reg, uint8_t val);
uint8_t status_get_motor_error_code(const StatusReg *reg);

// ---- loop_count ----
void     status_set_loop_count(StatusReg *reg, uint16_t val);
uint16_t status_get_loop_count(const StatusReg *reg);

// ---- initialized ----
void status_set_initialized(StatusReg *reg, bool val);
bool status_get_initialized(const StatusReg *reg);

// ---- button_pressed ----
void status_set_button_pressed(StatusReg *reg, bool val);
bool status_get_button_pressed(const StatusReg *reg);

// ---- target_motor_stop ----
void status_set_target_motor_stop(StatusReg *reg, bool val);
bool status_get_target_motor_stop(const StatusReg *reg);

#endif /* STATUS_REG_H */
