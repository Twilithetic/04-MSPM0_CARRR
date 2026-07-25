/*
 *  ======== led_reg.h ========
 *  Opaque type for LED debug counters.
 *  Full struct definition is private to registers.c.
 */

#ifndef LED_REG_H
#define LED_REG_H

#include <stdint.h>

/// Opaque handle.
typedef struct LedReg LedReg;

/// Global LED debug instance (defined in registers.c)
extern LedReg g_led_reg;

// ---- blue ----
void     led_inc_blue(void);
uint32_t led_get_blue(void);

// ---- green ----
void     led_inc_green(void);
uint32_t led_get_green(void);

#endif /* LED_REG_H */
