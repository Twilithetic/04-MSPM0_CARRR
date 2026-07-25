/*
 *  ======== controller.h ========
 *  PD line-following controller.
 *  Reads g_line_reg shadow → computes speeds → writes g_motor_reg.target_*.
 *  Also updates g_status_reg status flags.
 */

#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "include/line_reg.h"
#include "include/motor_reg.h"
#include "include/status_reg.h"

/// PD controller: reads LineReg shadow, writes MotorReg target_speed_* and
/// StatusReg flags.  Returns true if motors should stop (lost / all-black).
void controller_calculate(const LineReg *line, MotorReg *motor,
                          StatusReg *status);

/// Reset PD internal state (last_error)
void controller_reset(void);

#endif /* CONTROLLER_H */
