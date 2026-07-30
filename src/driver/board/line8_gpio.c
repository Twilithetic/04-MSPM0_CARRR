/*
 *  ======== line8_gpio.c ========
 *  8-Channel IR Line Sensor Proxy — direct GPIO read.
 *
 *  Hardware (configured in empty.syscfg, pinmux in ti_msp_dl_config):
 *    X1 = PA14, X2 = PA15, X3 = PA16, X4 = PA17   (left half)
 *    X5 = PB6,  X6 = PB7,  X7 = PB8,  X8 = PB9    (right half)
 *    X1 is the leftmost sensor, X8 the rightmost.
 *    All inputs, no internal resistor (module drives push-pull outputs).
 *
 *  Channel values: 0 = black (on line), 1 = white (off line)
 *
 *  Implementation: pure DriverLib — no IRQ, no bus, no state.
 *  Pins are initialized once by SYSCFG_DL_init() at startup; the sync
 *  below only reads them, so there is nothing extra to set up.
 */

#include "include/line8_reg.h"

#include "ti_msp_dl_config.h"

#include <stdbool.h>
#include <stdint.h>

/* ====================================================================
 *  SYNC: read the 8 GPIO inputs → write shadow register
 *
 *  Called periodically (e.g. @ 100 Hz) from the line sync task.
 *  readPins returns non-zero when the pin is high (1 = white/off-line).
 * ==================================================================== */

void sync_line8_from_device(Line8Reg *r)
{
    (void) r;   /* single global instance: g_line8_reg */

    /* Left half — GPIOA, X1..X4 (leftmost → center-left) */
    line8_set_raw(0, (DL_GPIO_readPins(LINE8_LEFT_PORT, LINE8_LEFT_X1_PIN) != 0U) ? 1U : 0U);
    line8_set_raw(1, (DL_GPIO_readPins(LINE8_LEFT_PORT, LINE8_LEFT_X2_PIN) != 0U) ? 1U : 0U);
    line8_set_raw(2, (DL_GPIO_readPins(LINE8_LEFT_PORT, LINE8_LEFT_X3_PIN) != 0U) ? 1U : 0U);
    line8_set_raw(3, (DL_GPIO_readPins(LINE8_LEFT_PORT, LINE8_LEFT_X4_PIN) != 0U) ? 1U : 0U);

    /* Right half — GPIOB, X5..X8 (center-right → rightmost) */
    line8_set_raw(4, (DL_GPIO_readPins(LINE8_RIGHT_PORT, LINE8_RIGHT_X5_PIN) != 0U) ? 1U : 0U);
    line8_set_raw(5, (DL_GPIO_readPins(LINE8_RIGHT_PORT, LINE8_RIGHT_X6_PIN) != 0U) ? 1U : 0U);
    line8_set_raw(6, (DL_GPIO_readPins(LINE8_RIGHT_PORT, LINE8_RIGHT_X7_PIN) != 0U) ? 1U : 0U);
    line8_set_raw(7, (DL_GPIO_readPins(LINE8_RIGHT_PORT, LINE8_RIGHT_X8_PIN) != 0U) ? 1U : 0U);

    /* recompute position, error, mask */
    line8_compute_position();
}
