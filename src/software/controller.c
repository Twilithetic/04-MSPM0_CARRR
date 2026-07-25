/*
 *  ======== controller.c ========
 *  PD line-following controller.
 *  Extracted from empty.c calculate_line_speed() + line_should_stop().
 *
 *  Convention (per spec):
 *    - Controller READS from shadow registers (zero I/O latency)
 *    - Controller WRITES target_* to motor shadow for later flush
 *    - Controller WRITES status flags to status shadow
 *    - Controller NEVER calls raw_* I/O functions
 */

#include "include/controller.h"
#include <stdbool.h>
#include <stdint.h>

/* ---- PD tuning constants (from empty.c) ---- */

#define LINE_BLACK_LEVEL        (0U)
#define LINE_LEFT_BASE_SPEED    (110)
#define LINE_RIGHT_BASE_SPEED   (110)
#define LINE_MAX_SPEED          (180)
#define LINE_CORRECTION_MAX     (50)
#define LINE_KP_NUM             (1)
#define LINE_KP_DEN             (20)
#define LINE_KD_NUM             (1)
#define LINE_KD_DEN             (45)
#define LINE_TURN_SIGN          (-1)
#define LINE_STOP_ON_ALL_BLACK  (1U)
#define LINE_SEARCH_WHEN_LOST   (0U)
#define LINE_SEARCH_SPEED       (100)

/* ---- File-scope PD state ---- */

static int16_t g_last_error = 0;

/* ---- Helpers ---- */

static int16_t clamp_i16(int32_t value, int16_t min_value, int16_t max_value)
{
    if (value > max_value) { return max_value; }
    if (value < min_value) { return min_value; }
    return (int16_t) value;
}

static bool line_should_stop(const LineReg *line)
{
    if (line->active_count == 0U) {
        return true;   /* lost */
    }
#if LINE_STOP_ON_ALL_BLACK
    if (line->active_count >= 4U) {
        return true;   /* cross / end line */
    }
#endif
    return false;
}

/* ---- Public API ---- */

void controller_calculate(const LineReg *line, MotorReg *motor,
                          StatusReg *status)
{
    int16_t error;
    int16_t correction;
    int32_t proportional;
    int32_t derivative;

    /* Write status flags from line sensor readings */
    status_set_line_lost(status, line->active_count == 0U);
    status_set_line_all_black(status, line->active_count >= 4U);

    /* Stop condition */
    if (line_should_stop(line)) {
#if LINE_SEARCH_WHEN_LOST
        if (line->active_count == 0U) {
            if (g_last_error >= 0) {
                motor->target_speed_m2 =  LINE_SEARCH_SPEED;
                motor->target_speed_m4 = -LINE_SEARCH_SPEED;
            } else {
                motor->target_speed_m2 = -LINE_SEARCH_SPEED;
                motor->target_speed_m4 =  LINE_SEARCH_SPEED;
            }
            /* Keep M1/M3 zero, they are unused */
            motor->target_speed_m1 = 0;
            motor->target_speed_m3 = 0;
            return;
        }
#endif
        /* Full stop */
        motor->target_speed_m1 = 0;
        motor->target_speed_m2 = 0;
        motor->target_speed_m3 = 0;
        motor->target_speed_m4 = 0;
        g_last_error = 0;
        return;
    }

    /* ---- PD computation ---- */
    if (line->active_count >= 4U) {
        error = 0;
    } else {
        error = line->error;
    }

    proportional = ((int32_t) error * LINE_KP_NUM) / LINE_KP_DEN;
    derivative   = ((int32_t) (error - g_last_error) * LINE_KD_NUM) / LINE_KD_DEN;
    correction   = clamp_i16(
        ((int32_t) LINE_TURN_SIGN * (proportional + derivative)),
        -LINE_CORRECTION_MAX,
        LINE_CORRECTION_MAX);
    g_last_error = error;

    /* Write target speeds to motor shadow (for later flush) */
    motor->target_speed_m1 = 0;
    motor->target_speed_m3 = 0;
    motor->target_speed_m2 = clamp_i16(
        (int32_t) LINE_LEFT_BASE_SPEED + correction,
        0, LINE_MAX_SPEED);
    motor->target_speed_m4 = clamp_i16(
        (int32_t) LINE_RIGHT_BASE_SPEED - correction,
        0, LINE_MAX_SPEED);
}

void controller_reset(void)
{
    g_last_error = 0;
}
