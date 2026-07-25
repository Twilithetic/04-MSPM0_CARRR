/*
 *  ======== uart_debug.h ========
 *  Blocking UART debug output driver.
 *  No shadow registers needed — fire-and-forget debug output.
 */

#ifndef UART_DEBUG_H
#define UART_DEBUG_H

#include <stdint.h>

void uart_write_char(char ch);
void uart_write_str(const char *str);
void uart_write_u32(uint32_t value);
void uart_write_i32(int32_t value);

#endif /* UART_DEBUG_H */
