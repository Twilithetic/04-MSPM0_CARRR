/*
 *  ======== line8_reg.h ========
 *  8-Channel IR Line Sensor — shadow register (opaque type).
 *
 *  Full struct definition is in src/proxy/line8_reg.c.
 *  Client code uses only the accessor functions declared below.
 *
 *  Hardware: 8 digital IR channels wired directly to GPIO inputs.
 *    X1 = PA14, X2 = PA15, X3 = PA16, X4 = PA17   (left half)
 *    X5 = PB6,  X6 = PB7,  X7 = PB8,  X8 = PB9    (right half)
 *    X1 is the leftmost sensor, X8 the rightmost.
 *
 *  Data flow:
 *    sync_from_device → DL_GPIO_readPins (src/driver/board/line8_gpio.c)
 *                     → write shadow register
 *
 *  One global instance: g_line8_reg (defined in src/proxy/line8_reg.c)
 *
 *  Channel values: 0 = black (on line), 1 = white (off line)
 *
 *  Position calculation (8 sensors, evenly spaced):
 *    weights: 0, 1000, 2000, 3000, 4000, 5000, 6000, 7000
 *    weighted-avg of active sensors → position (0-7000, center=3500)
 *    error = position - 3500
 */

#ifndef LINE8_REG_H
#define LINE8_REG_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LINE8_CHANNELS  8U

/* ---- Opaque type (full struct in line8_reg.c) ---- */
typedef struct Line8Reg Line8Reg;

extern Line8Reg g_line8_reg;

/* ================================================================
 *  Read access (Client / Controller / Logger)
 * ================================================================ */

/** Raw digital values (0=black/on-line, 1=white/off-line) — index 0..7 */
uint8_t  line8_get_raw(uint8_t idx);
void     line8_get_raw_all(uint8_t *dst);    /* copies 8 bytes into dst */

/** Line mask after inversion (1=line present, 0=no-line) — index 0..7 */
uint8_t  line8_get_line(uint8_t idx);

/** Bitmask: bit i set if sensor i sees line */
uint8_t  line8_get_mask(void);

/** How many sensors currently see the line (0..8) */
uint8_t  line8_get_active_count(void);

/** Weighted position (0-7000 millipoints, center=3500) */
int16_t  line8_get_position(void);

/** Deviation from center = position - 3500  (signed) */
int16_t  line8_get_error(void);

/** Encoded 4-bit white values: left=ch0..3, right=ch4..7 */
uint8_t  line8_get_left_white_val(void);
uint8_t  line8_get_right_white_val(void);

/* ================================================================
 *  Write access (Proxy only — declared for line8_reg.c linkage)
 * ================================================================ */

void line8_set_raw(uint8_t idx, uint8_t val);
void line8_compute_position(void);               /* recalc position/error/mask */
void line8_compute_white_vals(void);             /* recalc left/right white val */

#ifdef __cplusplus
}
#endif

#endif /* LINE8_REG_H */
