/*
 *  ======== line8_reg.h ========
 *  8-Channel IR Line Sensor — shadow register (opaque type).
 *
 *  Full struct definition is in src/proxy/line8_reg.c.
 *  Client code uses only the accessor functions declared below.
 *
 *  Data flow:
 *    sync_from_device → UART2 read → parse $D/$A frame → write shadow register
 *
 *  One global instance: g_line8_reg (defined in src/proxy/line8_reg.c)
 *
 *  Sensor protocol:
 *    Host sends:    $0,0,1#      — digital mode (0/1 per channel)
 *                   $0,1,0#      — analog mode  (0-4095 per channel)
 *    Sensor sends:  $D,x1:N,x2:N,x3:N,x4:N,x5:N,x6:N,x7:N,x8:N#
 *                   $A,x1:NNNN,x2:NNNN,...,x8:NNNN#
 *
 *  Digital: 0 = black (on line), 1 = white (off line)
 *  Analog:  0 ~ 4095, lower = darker
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

/** Analog values (0-4095) — index 0..7, valid only if mode==1 */
uint16_t line8_get_analog(uint8_t idx);

/** Bitmask: bit i set if sensor i sees line */
uint8_t  line8_get_mask(void);

/** How many sensors currently see the line (0..8) */
uint8_t  line8_get_active_count(void);

/** Weighted position (0-7000 millipoints, center=3500) */
int16_t  line8_get_position(void);

/** Deviation from center = position - 3500  (signed) */
int16_t  line8_get_error(void);

/** Communication status: 0 = OK, non-zero = error */
uint8_t  line8_get_comm_status(void);

/** Data mode: 0 = digital, 1 = analog */
uint8_t  line8_get_mode(void);

/** True if initialized OK */
bool     line8_is_initialized(void);

/** Frame count since init */
uint16_t line8_get_frame_count(void);

/* ================================================================
 *  Write access (Proxy only — declared for line8_reg.c linkage)
 * ================================================================ */

void line8_set_raw(uint8_t idx, uint8_t val);
void line8_set_line(uint8_t idx, uint8_t val);
void line8_set_analog(uint8_t idx, uint16_t val);
void line8_compute_position(void);               /* recalc position/error/mask */
void line8_set_comm_status(uint8_t status);
void line8_set_mode(uint8_t mode);
void line8_set_initialized(bool val);
void line8_inc_frame_count(void);

#ifdef __cplusplus
}
#endif

#endif /* LINE8_REG_H */
