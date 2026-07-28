/*
 *  ======== uart1_init.c ========
 *  UART1 printf override — maps fputc to motor driver UART1 (PA8/PA9).
 *
 *  Overriding fputc here means ALL printf calls go through UART1
 *  (the motor board).  This replaces the default newlib _write → UART0
 *  path when this file is linked in.
 *
 *  If you want printf → XDS110 (UART0) back, remove this file from the build.
 */

#include "include/motor_driver_reg.h"  /* motor_uart_putchar() */
#include <stdio.h>

/* fputc override: printf → motor UART1 */
int fputc(int c, FILE *f)
{
    (void)f;
    motor_uart_putchar((char)c);
    return c;
}
