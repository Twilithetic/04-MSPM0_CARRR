/*
 *  ======== motor_driver_uart.h ========
 *  4-Way Motor Driver Board — UART1 proxy + board PID API.
 *
 *  The driver board has its own built-in closed-loop PID speed controller.
 *  We set PID params and target speed on the board; it runs PID internally.
 *  Direct PWM bypass is also available.
 *
 *  Hardware:
 *    UART1: PA8 TX, PA9 RX @ 115200 8N1
 *
 *  Protocol:
 *    Commands:  $cmd:args#          (no CR/LF — '#' is the terminator)
 *    Responses: $KEY:data#          (terminated by '#')
 *
 *    Board PID speed control:
 *      $mpid:P,I,D#                 — set board internal PID parameters
 *      $spd:M1,M2,M3,M4#            — target speed (encoder counts/10ms)
 *
 *    Direct PWM:
 *      $pwm:M1,M2,M3,M4#            — raw PWM (-7200 ~ +7200)
 *
 *    Config / Upload:
 *      $mtype:N#  $deadzone:N#  $mline:N#  $mphase:N#  $wdiameter:F#
 *      $upload:ALL,10ms,SPD#       — enable periodic encoder/speed upload
 *
 *    Query:
 *      $read_vol#                    →  $Battery:X.XXV#
 *
 *  Motor mapping (our 2-wheel car uses M2 and M4):
 *    M1=LF  M2=LEFT  M3=RF  M4=RIGHT
 */

#ifndef MOTOR_DRIVER_UART_H
#define MOTOR_DRIVER_UART_H

#include "motor_driver_reg.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Init ---- */
bool motor_driver_init(void);

/* ---- TT Encoder config sequence ---- */
bool cmd_config_tt_encoder(MotorDriverReg *r);

/* ---- Board PID parameters (sets $mpid:P,I,D#) ---- */
void motor_send_pid(float kp, float ki, float kd);

/* ---- Speed control (board PID, encoder counts/10ms per motor) ----
 *
 *  Sends $spd:M1,M2,M3,M4#.  M1/M3 should be 0 for our 2-wheel car.
 *  Mapping: M2=LEFT, M4=RIGHT.
 *
 *  Units: encoder counts per 10ms.
 *  Conversion: 1 motor rev = 13 lines × 4 edges × 45 ratio = 2340 counts
 *              mm/s → counts/10ms:  counts_10ms = mm_s × 2340 / 215.2 / 100
 *  For convenience, use motor_send_speed_mm_s() which does this internally.
 */
void motor_send_speed(int16_t m1, int16_t m2, int16_t m3, int16_t m4);

/* Speed in mm/s — converts to encoder counts and sends $spd: */
void motor_send_speed_mm_s(float left_mm_s, float right_mm_s);

/* ---- Direct PWM (raw, bypasses board PID) ----
 *
 *  Sends $pwm:M1,M2,M3,M4#.  M1/M3 should be 0 for our 2-wheel car.
 *  Range: -7200 ~ +7200.  Deadzone on board is ~1250.
 */
void motor_send_pwm(int16_t m1, int16_t m2, int16_t m3, int16_t m4);

/* ---- Stop ---- */
void motor_send_stop(void);

/* ---- SYNC: read UART1 → write shadow register ---- */
void sync_encoder_from_device(MotorDriverReg *r);
void sync_config_from_device(MotorDriverReg *r);

/* ---- Battery health check ---- */
uint16_t motor_read_battery_voltage(void);

/* ---- printf redirect (fputc override) ---- */
void motor_uart_putchar(char c);

/* ---- Print motor config to debug-UART (UART0, non-blocking) ---- */
void motor_print_config(bool ok);

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_DRIVER_UART_H */
