/*
 *  ======== line8_reg.c ========
 *  8-Channel IR Line Sensor shadow register — struct definition,
 *  global instance, and all accessors in one file.
 *
 *  Channel values: 0 = black (on line), 1 = white (off line)
 *
 *  Sensor position weights (8 sensors evenly spaced):
 *    Sensor 0 / X1 (leftmost,  PA14) → weight  0
 *    Sensor 1 / X2            (PA15) → weight 1000
 *    Sensor 2 / X3            (PA16) → weight 2000
 *    Sensor 3 / X4            (PA17) → weight 3000
 *    Sensor 4 / X5            (PB6)  → weight 4000
 *    Sensor 5 / X6            (PB7)  → weight 5000
 *    Sensor 6 / X7            (PB8)  → weight 6000
 *    Sensor 7 / X8 (rightmost, PB9)  → weight 7000
 *
 *  Weighted average gives 0-7000; center is 3500.
 */

#include "include/line8_reg.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* 8 evenly-spaced positions, millipoints */
static const int16_t POS_WEIGHTS[LINE8_CHANNELS] = {
    0, 1000, 2000, 3000, 4000, 5000, 6000, 7000
};

/* ---- Full struct (private — opaque to client) ---- */
struct Line8Reg {
    /* ---- sync_from_device: updated by GPIO read ---- */
    volatile uint8_t  raw[LINE8_CHANNELS];       /* 0=black/line, 1=white/bg */
    volatile uint8_t  line[LINE8_CHANNELS];      /* inverted: 1=line */
    volatile uint8_t  mask;                      /* bit i = 1 if line[i]==1 */
    volatile uint8_t  active_count;              /* 0..8 */
    volatile int16_t  position;                  /* 0-7000, center=3500 */
    volatile int16_t  error;                     /* position - 3500 */

    /* ---- derived: white-level encoded values ---- */
    volatile uint8_t  left_white_val;            /* ch0..3 encoded */
    volatile uint8_t  right_white_val;           /* ch4..7 encoded */
};

Line8Reg g_line8_reg = {0};

/* ── Read access ── */

uint8_t line8_get_raw(uint8_t idx)
{
    if (idx >= LINE8_CHANNELS) return 0;
    return g_line8_reg.raw[idx];
}

void line8_get_raw_all(uint8_t *dst)
{
    if (dst) {
        memcpy(dst, (const void *)g_line8_reg.raw, LINE8_CHANNELS);
    }
}

uint8_t line8_get_line(uint8_t idx)
{
    if (idx >= LINE8_CHANNELS) return 0;
    return g_line8_reg.line[idx];
}

uint8_t line8_get_mask(void)
{
    return g_line8_reg.mask;
}

uint8_t line8_get_active_count(void)
{
    return g_line8_reg.active_count;
}

int16_t line8_get_position(void)
{
    return g_line8_reg.position;
}

int16_t line8_get_error(void)
{
    return g_line8_reg.error;
}

/* ── Write access (Proxy only) ── */

void line8_set_raw(uint8_t idx, uint8_t val)
{
    if (idx >= LINE8_CHANNELS) return;
    g_line8_reg.raw[idx] = val;
    /* invert: raw -> line (1=white/bg, 0=black/line → line=1 means on-line) */
    g_line8_reg.line[idx] = (val == 0) ? 1 : 0;
}

void line8_compute_position(void)
{
    uint8_t  i;
    uint8_t  mask = 0;
    uint8_t  count = 0;
    int32_t  weighted_sum = 0;

    for (i = 0; i < LINE8_CHANNELS; i++) {
        if (g_line8_reg.line[i]) {
            mask |= (1U << i);
            count++;
            weighted_sum += (int32_t)POS_WEIGHTS[i];
        }
    }

    g_line8_reg.mask = mask;
    g_line8_reg.active_count = count;

    if (count > 0) {
        g_line8_reg.position = (int16_t)(weighted_sum / (int32_t)count);
    } else {
        g_line8_reg.position = 3500;  /* default to center when lost */
    }

    g_line8_reg.error = g_line8_reg.position - 3500;
}

void line8_compute_white_vals(void)
{
    g_line8_reg.left_white_val = (uint8_t)(
          (g_line8_reg.raw[0] ? 2 : 0)
        + (g_line8_reg.raw[1] ? 4 : 0)
        + (g_line8_reg.raw[2] ? 8 : 0)
        + (g_line8_reg.raw[3] ? 16 : 0));

    g_line8_reg.right_white_val = (uint8_t)(
          (g_line8_reg.raw[4] ? 16 : 0)
        + (g_line8_reg.raw[5] ? 8 : 0)
        + (g_line8_reg.raw[6] ? 4 : 0)
        + (g_line8_reg.raw[7] ? 2 : 0));
}

uint8_t line8_get_left_white_val(void)
{
    return g_line8_reg.left_white_val;
}

uint8_t line8_get_right_white_val(void)
{
    return g_line8_reg.right_white_val;
}
