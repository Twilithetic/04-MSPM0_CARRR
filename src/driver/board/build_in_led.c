/*
 *  ======== build_in_led.c ========
 *  Board built-in LED driver — self-contained, no SysConfig dependency.
 *  PB3 = blue  LED (PORTB pin 3), high-active
 *  PB2 = green LED (PORTB pin 2), high-active
 */

#include "include/build_in_led.h"
#include "ti_msp_dl_config.h"

#define GPIO_LEDS_PORT      (GPIOB)
#define GPIO_LEDS_PIN_BLUE  (DL_GPIO_PIN_3)   /* PB3 */
#define GPIO_LEDS_PIN_GREEN (DL_GPIO_PIN_2)   /* PB2 */

/* ---- PB3 blue ---- */
void blue_led_on(void)     { DL_GPIO_setPins(GPIO_LEDS_PORT, GPIO_LEDS_PIN_BLUE); }
void blue_led_off(void)    { DL_GPIO_clearPins(GPIO_LEDS_PORT, GPIO_LEDS_PIN_BLUE); }
void blue_led_toggle(void) { DL_GPIO_togglePins(GPIO_LEDS_PORT, GPIO_LEDS_PIN_BLUE); }

/* ---- PB2 green ---- */
void green_led_on(void)     { DL_GPIO_setPins(GPIO_LEDS_PORT, GPIO_LEDS_PIN_GREEN); }
void green_led_off(void)    { DL_GPIO_clearPins(GPIO_LEDS_PORT, GPIO_LEDS_PIN_GREEN); }
void green_led_toggle(void) { DL_GPIO_togglePins(GPIO_LEDS_PORT, GPIO_LEDS_PIN_GREEN); }
