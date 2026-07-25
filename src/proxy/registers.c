/*
 *  ======== registers.c ========
 *  Single compilation unit that defines all shadow register global instances.
 *  Initialized to zero (BSS).
 */

#include "include/motor_reg.h"
#include "include/line_reg.h"
#include "include/status_reg.h"

MotorReg  g_motor_reg  = {0};
LineReg   g_line_reg   = {0};
StatusReg g_status_reg = {0};
