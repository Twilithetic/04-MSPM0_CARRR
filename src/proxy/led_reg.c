/*
 *  ======== led_reg.c ========
 *  LED debug counter shadow register — global instance + accessors.
 */

#include "include/led_reg.h"
#include <stdint.h>

/* ---- Full struct definition (opaque to client) ---- */
struct LedReg {
    uint32_t blue_toggle_count;
    uint32_t green_toggle_count;
};

LedReg g_led_reg = {0};

/* ---- blue ---- */
void     led_inc_blue(void)             { g_led_reg.blue_toggle_count++; }
uint32_t led_get_blue(void)             { return g_led_reg.blue_toggle_count; }

/* ---- green ---- */
void     led_inc_green(void)            { g_led_reg.green_toggle_count++; }
uint32_t led_get_green(void)            { return g_led_reg.green_toggle_count; }
