/*
 *  ======== motor_driver_uart.h ========
 *  4-Way Motor Driver Board — UART1 proxy + shadow register API.
 *
 *  Hardware:
 *    UART1: PA8 TX, PA9 RX @ 115200 8N1
 *    TI Drivers CALLBACK mode + DMA (channel 1)
 *
 *  Data flow:
 *    sync_encoder_from_device → UART1 read → shadow register (g_motor_driver_reg)
 *    flush_*_to_device        → shadow target_* → UART1 write
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

/* ---- SYNC: read UART1 → write shadow ---- */
void sync_encoder_from_device(MotorDriverReg *r);
void sync_config_from_device(MotorDriverReg *r);

/* ---- FLUSH: read shadow target_* → write UART1 ---- */
void flush_speed_to_device(MotorDriverReg *r);
void flush_pwm_to_device(MotorDriverReg *r);
void flush_stop_to_device(MotorDriverReg *r);

/* ---- CMD: TT encoder config sequence ---- */
bool cmd_config_tt_encoder(MotorDriverReg *r);

/* ---- Battery health check (UART1 read) ---- */
uint16_t motor_read_battery_voltage(void);

/* ---- printf redirect (fputc override) ---- */
void motor_uart_putchar(char c);

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_DRIVER_UART_H */
