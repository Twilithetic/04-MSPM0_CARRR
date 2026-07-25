/*
 *  ======== uart_debug.c ========
 *  Blocking UART debug output using DL_UART_Main_transmitDataBlocking.
 *  Extracted from empty.c uart_write_* static functions.
 */

#include "include/uart_debug.h"
#include "ti_msp_dl_config.h"

void uart_write_char(char ch)
{
    DL_UART_Main_transmitDataBlocking(UART_0_INST, (uint8_t) ch);
}

void uart_write_str(const char *str)
{
    while (*str != '\0') {
        uart_write_char(*str++);
    }
}

void uart_write_u32(uint32_t value)
{
    char buf[10];
    uint32_t index = 0;

    if (value == 0U) {
        uart_write_char('0');
        return;
    }

    while ((value > 0U) && (index < sizeof(buf))) {
        buf[index++] = (char) ('0' + (value % 10U));
        value /= 10U;
    }

    while (index > 0U) {
        uart_write_char(buf[--index]);
    }
}

void uart_write_i32(int32_t value)
{
    uint32_t magnitude;

    if (value < 0) {
        uart_write_char('-');
        magnitude = (uint32_t) (-(value + 1)) + 1U;
    } else {
        magnitude = (uint32_t) value;
    }

    uart_write_u32(magnitude);
}
