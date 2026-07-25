/*
 *  ======== main.c ========
 *  Application entry point — 5-channel gray line follower.
 *
 *  Architecture (Hardware Proxy + Shadow Register):
 *    [SYNC]  line.sync_from_device()    → writes g_line_reg
 *            motor.sync_encoder()       → writes g_motor_reg  (every N loops)
 *    [THINK] controller_calculate()     → reads g_line_reg, writes target_*
 *    [FLUSH] motor.flush_speed()        → reads target_*, writes I2C
 *    [PRINT] print_*()                  → reads all shadow registers
 *
 *  This main loop NEVER calls raw_* I2C/GPIO functions directly.
 *  All I/O goes through the proxy (sync/flush) layer.
 */

#include "ti_msp_dl_config.h"

#include "include/motor_reg.h"
#include "include/line_reg.h"
#include "include/status_reg.h"

#include "include/motor.h"
#include "include/line.h"
#include "include/uart_debug.h"

#include "include/controller.h"

#include <stdbool.h>
#include <stdint.h>

/* ---- Timing constants ---- */

#define LINE_LOOP_DELAY_CYCLES      (960000U)
#define LINE_PRINT_INTERVAL_LOOPS   (5U)
#define LINE_ENCODER_INTERVAL_LOOPS (20U)

#define DELAY_100MS_CYCLES          (3200000U)
#define DELAY_300MS_CYCLES          (9600000U)
#define DELAY_1S_CYCLES             (32000000U)

/* ---- Local debug helpers (read-only from shadows) ---- */

static void print_line_header(void)
{
    uart_write_str("\r\nMSPM0 5-channel gray line follower\r\n");
    uart_write_str("Motor IIC: SDA=PA13 SCL=PA12 addr=0x26\r\n");
    uart_write_str("Debug UART: PA10 TX / PA11 RX, 115200 8N1\r\n");
    uart_write_str("Gray GPIO: S1=PA14 S2=PA15 S3=PA16 S4=PA17 S5=PA21, black=0\r\n");
    uart_write_str("Drive mapping: left=M2 right=M4\r\n");
    uart_write_str("Motor config: encoder TT, line=13, ratio=45, wheel=68mm, deadzone=1250\r\n");
    uart_write_str("Action: slow PD line follow with encoder speed control.\r\n");
}

static void print_line_sample(const LineReg *line, const MotorReg *motor,
                               const StatusReg *status)
{
    (void) motor;
    (void) status;

    uart_write_str("gray raw=");
    for (uint8_t i = 0U; i < 5U; i++) {
        uart_write_char((char) ('0' + line->raw[i]));
    }

    uart_write_str(" line=");
    for (uint8_t i = 0U; i < 5U; i++) {
        uart_write_char((char) ('0' + line->line[i]));
    }

    uart_write_str(" cnt=");
    uart_write_u32(line->active_count);

    if (line->active_count == 0U) {
        uart_write_str(" mode=lost");
    } else if (line->active_count >= 4U) {
        uart_write_str(" mode=all_black");
    } else {
        uart_write_str(" mode=follow");
    }

    uart_write_str(" pos=");
    uart_write_i32(line->position);
    uart_write_str(" err=");
    uart_write_i32(line->error);
    uart_write_str(" L=");
    uart_write_i32(motor->target_speed_m2);
    uart_write_str(" R=");
    uart_write_i32(motor->target_speed_m4);

    if (motor->comm_status != 0U) {
        uart_write_str(" motor_error=");
        uart_write_u32(motor->comm_status);
    }

    uart_write_str("\r\n");
}

static void print_encoder_line(const MotorReg *motor)
{
    uart_write_str("all M1=");
    uart_write_i32(motor->encoder_total[0]);
    uart_write_str(" M2=");
    uart_write_i32(motor->encoder_total[1]);
    uart_write_str(" M3=");
    uart_write_i32(motor->encoder_total[2]);
    uart_write_str(" M4=");
    uart_write_i32(motor->encoder_total[3]);

    uart_write_str(" | 10ms M1=");
    uart_write_i32(motor->encoder_10ms[0]);
    uart_write_str(" M2=");
    uart_write_i32(motor->encoder_10ms[1]);
    uart_write_str(" M3=");
    uart_write_i32(motor->encoder_10ms[2]);
    uart_write_str(" M4=");
    uart_write_i32(motor->encoder_10ms[3]);
    uart_write_str("\r\n");
}

/* ====================================================================
 *  main — SYNC → THINK → FLUSH loop
 * ==================================================================== */

int main(void)
{
    /* ---- Stage 1: Hardware init ---- */
    SYSCFG_DL_init();

    delay_cycles(DELAY_100MS_CYCLES);

    /* ---- Stage 2: Proxy init ---- */
    MotorProxy motor = { .i2c_addr = 0x26 };
    LineProxy  line  = {
        .black_level = 0,
        .pins        = (const uint32_t[]){
            GRAY_S1_PIN, GRAY_S2_PIN, GRAY_S3_PIN, GRAY_S4_PIN, GRAY_S5_PIN,
        },
        .positions   = (const int16_t[]){0, 1000, 2000, 3000, 4000},
    };

    motor_proxy_init(&motor);

    /* ---- Stage 3: Boot message ---- */
    print_line_header();

    /* ---- Stage 4: Motor configuration (one-time init) ---- */
    cmd_config_tt_encoder(&motor);
    g_status_reg.initialized = true;

    uart_write_str("\r\nline follower starts after 3 seconds\r\n");
    delay_cycles(DELAY_1S_CYCLES);
    delay_cycles(DELAY_1S_CYCLES);
    delay_cycles(DELAY_1S_CYCLES);

    /* ---- Stage 5: Main loop ---- */
    while (1) {
        /* 5a: SYNC — read hardware into shadows */
        sync_from_device(&line, &g_line_reg);

        /* 5b: THINK — read shadows, compute, write target_* shadows */
        controller_calculate(&g_line_reg, &g_motor_reg, &g_status_reg);

        /* 5c: FLUSH — read target_* shadows, write I2C */
        if (g_status_reg.line_lost || g_status_reg.line_all_black) {
            flush_stop_to_device(&motor, &g_motor_reg);
        } else {
            flush_speed_to_device(&motor, &g_motor_reg);
        }

        /* 5d: SYNC encoders (lower frequency) */
        if ((g_status_reg.loop_count % LINE_ENCODER_INTERVAL_LOOPS) == 0U) {
            sync_encoder_from_device(&motor, &g_motor_reg);
        }

        /* 5e: Debug print (reads only from shadow registers) */
        if ((g_status_reg.loop_count % LINE_PRINT_INTERVAL_LOOPS) == 0U) {
            print_line_sample(&g_line_reg, &g_motor_reg, &g_status_reg);
        }

        if ((g_status_reg.loop_count % LINE_ENCODER_INTERVAL_LOOPS) == 0U) {
            print_encoder_line(&g_motor_reg);
        }

        /* 5f: Error handling */
        uint8_t comm_err = g_motor_reg.comm_status;
        if (comm_err != 0U) {
            g_status_reg.motor_error      = true;
            g_status_reg.motor_error_code = comm_err;
            flush_stop_to_device(&motor, &g_motor_reg);
            delay_cycles(DELAY_300MS_CYCLES);
        } else {
            g_status_reg.motor_error = false;
            delay_cycles(LINE_LOOP_DELAY_CYCLES);
        }

        g_status_reg.loop_count++;
    }
}
