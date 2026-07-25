/*
 *  ======== main.c ========
 *  Minimal FreeRTOS demo — two LEDs blinking asynchronously.
 *
 *  Blue LED (PB2): 500ms period (task priority 1)
 *  Green LED (PB3): 500ms period, 250ms phase offset (task priority 1)
 *
 *  All previous line-following code has been commented out below.
 *  Tasks are implemented in src/software/task.c.
 */

#include "ti_msp_dl_config.h"
#include "include/task.h"
#include "include/app_hooks.h"

#include <FreeRTOS.h>
#include <task.h>

#define DELAY_100MS_CYCLES  (3200000U)
#define DELAY_1S_CYCLES     (32000000U)

int main(void)
{
    /* ---- Hardware init ---- */
    SYSCFG_DL_init();
    delay_cycles(DELAY_100MS_CYCLES);

    /* ---- Create application tasks ---- */
    xTaskCreate(vBlueTask,  "BlueLED",  configMINIMAL_STACK_SIZE,
                NULL,       1,          NULL);
    xTaskCreate(vGreenTask, "GreenLED", configMINIMAL_STACK_SIZE,
                NULL,       1,          NULL);

    /* ---- Start FreeRTOS scheduler (never returns) ---- */
    vTaskStartScheduler();

    /* Should never reach here */
    for (;;) {}
}

/* ====================================================================
 *  Original line‑following main loop (commented out for now).
 *
 *  Architecture: Hardware Proxy + Shadow Register
 *    [SYNC]  line.sync_from_device()    → writes g_line_reg
 *            motor.sync_encoder()       → writes g_motor_reg
 *    [THINK] controller_calculate()     → reads g_line_reg, writes target_*
 *    [FLUSH] motor.flush_speed()        → reads target_*, writes I2C
 *    [PRINT] print_*()                  → reads all shadow registers
 * ====================================================================
 *
 * #include "ti_msp_dl_config.h"
 * #include "include/motor_reg.h"
 * #include "include/line_reg.h"
 * #include "include/status_reg.h"
 * #include "include/motor.h"
 * #include "include/line.h"
 * #include "include/uart_debug.h"
 * #include "include/controller.h"
 * #include <stdbool.h>
 * #include <stdint.h>
 *
 * #define LINE_LOOP_DELAY_CYCLES      (960000U)
 * #define LINE_PRINT_INTERVAL_LOOPS   (5U)
 * #define LINE_ENCODER_INTERVAL_LOOPS (20U)
 * #define DELAY_100MS_CYCLES          (3200000U)
 * #define DELAY_300MS_CYCLES          (9600000U)
 * #define DELAY_1S_CYCLES             (32000000U)
 *
 * static void print_line_header(void) { ... }
 * static void print_line_sample(...) { ... }
 * static void print_encoder_line(...) { ... }
 *
 * int main(void)
 * {
 *     SYSCFG_DL_init();
 *     delay_cycles(DELAY_100MS_CYCLES);
 *
 *     MotorProxy motor = { .i2c_addr = 0x26 };
 *     LineProxy  line  = { ... };
 *     motor_proxy_init(&motor);
 *     print_line_header();
 *     cmd_config_tt_encoder(&motor);
 *     g_status_reg.initialized = true;
 *     delay_cycles(DELAY_1S_CYCLES * 3);
 *
 *     while (1) {
 *         sync_from_device(&line, &g_line_reg);
 *         controller_calculate(&g_line_reg, &g_motor_reg, &g_status_reg);
 *         if (g_status_reg.line_lost || g_status_reg.line_all_black) {
 *             flush_stop_to_device(&motor, &g_motor_reg);
 *         } else {
 *             flush_speed_to_device(&motor, &g_motor_reg);
 *         }
 *         if ((g_status_reg.loop_count % LINE_ENCODER_INTERVAL_LOOPS) == 0U) {
 *             sync_encoder_from_device(&motor, &g_motor_reg);
 *         }
 *         if ((g_status_reg.loop_count % LINE_PRINT_INTERVAL_LOOPS) == 0U) {
 *             print_line_sample(&g_line_reg, &g_motor_reg, &g_status_reg);
 *         }
 *         if ((g_status_reg.loop_count % LINE_ENCODER_INTERVAL_LOOPS) == 0U) {
 *             print_encoder_line(&g_motor_reg);
 *         }
 *         uint8_t comm_err = g_motor_reg.comm_status;
 *         if (comm_err != 0U) {
 *             g_status_reg.motor_error = true;
 *             g_status_reg.motor_error_code = comm_err;
 *             flush_stop_to_device(&motor, &g_motor_reg);
 *             delay_cycles(DELAY_300MS_CYCLES);
 *         } else {
 *             g_status_reg.motor_error = false;
 *             delay_cycles(LINE_LOOP_DELAY_CYCLES);
 *         }
 *         g_status_reg.loop_count++;
 *     }
 * }
 */
