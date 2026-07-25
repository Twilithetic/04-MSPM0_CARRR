/*
 *  ======== build_in_led.c ========
 *  Board built-in LED driver.
 *  GPIO init is handled by SysConfig via empty.syscfg.
 *  Pin macros are defined here, not in the auto-generated ti_msp_dl_config.h.
 *
 *  PB3 = blue  LED, PB2 = green LED (both PORTB, high-active).
 */

#include "include/build_in_led.h"
#include "ti_msp_dl_config.h"

/* ---- Pin macros (PB3=blue, PB2=green) ---- */
#define LED_GPIO_PORT         (GPIOB)
#define LED_GPIO_PIN_BLUE     (DL_GPIO_PIN_3)
#define LED_GPIO_PIN_GREEN    (DL_GPIO_PIN_2)

/* ---- PB3 blue ---- */
void blue_led_on(void)     { DL_GPIO_setPins(LED_GPIO_PORT, LED_GPIO_PIN_BLUE); }
void blue_led_off(void)    { DL_GPIO_clearPins(LED_GPIO_PORT, LED_GPIO_PIN_BLUE); }
void blue_led_toggle(void) { DL_GPIO_togglePins(LED_GPIO_PORT, LED_GPIO_PIN_BLUE); }

/* ---- PB2 green ---- */
void green_led_on(void)     { DL_GPIO_setPins(LED_GPIO_PORT, LED_GPIO_PIN_GREEN); }
void green_led_off(void)    { DL_GPIO_clearPins(LED_GPIO_PORT, LED_GPIO_PIN_GREEN); }
void green_led_toggle(void) { DL_GPIO_togglePins(LED_GPIO_PORT, LED_GPIO_PIN_GREEN); }
