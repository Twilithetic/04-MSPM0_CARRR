/*
 *  ======== build_in_led.h ========
 *  Board built-in LED driver (PB2 blue, PB3 green, high-active).
 */

#ifndef BUILD_IN_LED_H
#define BUILD_IN_LED_H

#include <stdbool.h>

void blue_led_on(void);
void blue_led_off(void);
void blue_led_toggle(void);

void green_led_on(void);
void green_led_off(void);
void green_led_toggle(void);

#endif /* BUILD_IN_LED_H */
