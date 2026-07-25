/*
 *  ======== line.c ========
 *  Line sensor proxy: reads 5 GPIO pins, computes weighted centroid,
 *  writes to line shadow register.
 *  Extracted from empty.c read_line_sample().
 */

#include "include/line.h"
#include "ti_msp_dl_config.h"

/* ---- GRAY_ALL_PINS mask (S1-S5, original empty.c define) ---- */
#define GRAY_ALL_PINS \
    (GRAY_S1_PIN | GRAY_S2_PIN | GRAY_S3_PIN | GRAY_S4_PIN | GRAY_S5_PIN)

/* ---- Pin masks and position tables (from original empty.c) ---- */

static const uint32_t g_gray_pins[5] = {
    GRAY_S1_PIN,
    GRAY_S2_PIN,
    GRAY_S3_PIN,
    GRAY_S4_PIN,
    GRAY_S5_PIN,
};

static const int16_t g_gray_positions[5] = {
    0,
    1000,
    2000,
    3000,
    4000,
};

void sync_from_device(const LineProxy *p, LineReg *r)
{
    uint32_t pins        = DL_GPIO_readPins(GRAY_PORT, GRAY_ALL_PINS);
    int32_t  weighted_sum = 0;
    uint8_t  count        = 0;
    uint8_t  mask         = 0;

    for (uint8_t i = 0U; i < 5U; i++) {
        r->raw[i]  = ((pins & p->pins[i]) != 0U) ? 1U : 0U;
        r->line[i] = (r->raw[i] == p->black_level) ? 1U : 0U;

        if (r->line[i] != 0U) {
            count++;
            mask |= (uint8_t) (1U << i);
            weighted_sum += p->positions[i];
        }
    }

    r->active_count = count;
    r->mask         = mask;

    if (count != 0U) {
        r->position = (int16_t) (weighted_sum / count);
        r->error    = r->position - 2000;
    } else {
        r->position = 2000;
        r->error    = 0;
    }
}
