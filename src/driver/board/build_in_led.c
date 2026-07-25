/*
 *  ======== build_in_led.c ========
 *  Board built-in LED driver.
 *  PB2 = blue LED  (LED1/PIN_2), high-active
 *  PB3 = green LED (LED1/PIN_3), high-active
 */

#include "include/build_in_led.h"
#include "ti_msp_dl_config.h"

/* ---- PB2 blue (PIN_2) ---- */
void blue_led_on(void)    { DL_GPIO_setPins(LED1_PORT, LED1_PIN_2_PIN); }
void blue_led_off(void)   { DL_GPIO_clearPins(LED1_PORT, LED1_PIN_2_PIN); }
void blue_led_toggle(void) { DL_GPIO_togglePins(LED1_PORT, LED1_PIN_2_PIN); }

/* ---- PB3 green (PIN_3) ---- */
void green_led_on(void)    { DL_GPIO_setPins(LED1_PORT, LED1_PIN_3_PIN); }
void green_led_off(void)   { DL_GPIO_clearPins(LED1_PORT, LED1_PIN_3_PIN); }
void green_led_toggle(void) { DL_GPIO_togglePins(LED1_PORT, LED1_PIN_3_PIN); }
