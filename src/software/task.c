/*
 *  ======== task.c ========
 *  Application task implementations.
 *
 *  vBlueTask     — blink blue LED (PB3) @ 750ms
 *  vGreenTask    — blink green LED (PB2) @ 1000ms
 *  vI2CScanTask  — I2C bus scan (prio 2), runs once, signals logger
 *  vImuPollTask  — IMU poll (prio 3), reads LSM6DSV16X → shadow register
 *  vLoggerTask   — print LED stats + IMU data via DMA-UART @ 1 Hz
 */

#include "include/app_tasks.h"
#include "include/build_in_led.h"
#include "include/led_reg.h"
#include "include/XDS110_cdc.h"
#include "include/imu_shadow.h"           /* shadow register accessors */
#include "include/motor_driver_uart.h"    /* motor driver proxy */
#include "include/car_speed_ctrl.h"    /* shadow register accessors */

/* I2C functions (in src/driver/chip/I2C_test.c) */
extern void i2c_test_init(void);
extern void i2c_scan_bus(void);
extern void i2c_scan_print_results(void);

/* Motor Driver Proxy — see motor_driver_uart.h */
extern bool lsm6dsv16x_is_present(void);
extern bool lsm6dsv16x_init(void);
extern void lsm6dsv16x_sync_from_device(void);

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <stdio.h>

/* ── Encoder → travel distance conversion ──
 *  2340 counts per wheel-rev (13 lines × 4 edges × 45 reduction ratio)
 *  Wheel diameter = 68.5mm → circumference = PI × 68.5 ≈ 215.20mm
 *  mm_per_count = (PI × 68.5) / 2340 ≈ 0.09198 */
#define ENCODER_COUNTS_PER_REV  2340U
#define WHEEL_DIAMETER_MM       68.5f
#define MM_PER_COUNT            (3.1415926f * WHEEL_DIAMETER_MM / (float)ENCODER_COUNTS_PER_REV)

void motor_update_distance(void)
{
    float dist_left  = (float)g_motor_driver_reg.encoder_total_left  * MM_PER_COUNT;
    float dist_right = (float)g_motor_driver_reg.encoder_total_right * MM_PER_COUNT;
    motor_set_distance_left_mm(dist_left);
    motor_set_distance_right_mm(dist_right);
}

/* ---- Semaphore (created in main.c before scheduler starts) ---- */
extern SemaphoreHandle_t g_scanDoneSem;
extern SemaphoreHandle_t g_motorDoneSem;
extern SemaphoreHandle_t g_motorSyncSem;
extern SemaphoreHandle_t g_ctrlSyncSem;

/* ── Blue LED blink ── */
void vBlueTask(void *pvParameters)
{
    (void) pvParameters;
    for (;;) {
        blue_led_toggle();
        led_inc_blue();
        vTaskDelay(pdMS_TO_TICKS(750));
    }
}

/* ── Green LED blink ── */
void vGreenTask(void *pvParameters)
{
    (void) pvParameters;
    for (;;) {
        green_led_toggle();
        led_inc_green();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* ── I2C scan task (prio 2): runs first, signals logger when done ── */
void vI2CScanTask(void *pvParameters)
{
    (void) pvParameters;
    vTaskDelay(pdMS_TO_TICKS(300));// 等那些芯片先启动
    i2c_scan_bus();

    /* Counting semaphore — give twice: one for ImuPoll, one for Logger */
    xSemaphoreGive(g_scanDoneSem);
    xSemaphoreGive(g_scanDoneSem);

    vTaskDelete(NULL);
}

/* ── IMU poll task (prio 3): runs FOREVER, syncs sensor → shadow ── */
void vImuPollTask(void *pvParameters)
{
    (void) pvParameters;

    /* Wait for I2C scan to finish.  vI2CScanTask (prio 2) gives
     * the semaphore after i2c_scan_bus(), so this is safe. */
    xSemaphoreTake(g_scanDoneSem, portMAX_DELAY);

    /* Check if the IMU was found on the bus */
    if (!lsm6dsv16x_is_present()) {
        /* IMU not found — silently exit */
        vTaskDelete(NULL);
    }

    /* Initialize IMU (I2C is up, bus scan found it) */
    (void) lsm6dsv16x_init();

    TickType_t xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        lsm6dsv16x_sync_from_device();
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(5));  /* 200 Hz */
    }
}

/* ── Logger task (prio 1): prints IMU + motor encoder stats via DMA-UART @ 10 Hz ──
 *
 *  Waits for:
 *   1. g_scanDoneSem  from vI2CScanTask  (I2C bus scan done)
 *   2. g_motorDoneSem from vMotorInitTask (motor config done, DMA-UART safe) */
void vLoggerTask(void *pvParameters)
{
    (void) pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();

    uart_send_async((const uint8_t *)
        "MSPM0G3507 FreeRTOS — I2C Scan\r\n", 35, 0);

    /* Wait for I2C scan */
    xSemaphoreTake(g_scanDoneSem, portMAX_DELAY);
    i2c_scan_print_results();

    /* Wait for motor init to finish before printing encoder data */
    xSemaphoreTake(g_motorDoneSem, portMAX_DELAY);

    /* Read battery voltage (UART health check) and print */
    {
        uint16_t raw = motor_read_battery_voltage();
        char buf[64];
        int n = snprintf(buf, sizeof(buf),
                         "Motor battery: %u.%uV\r\n",
                         (unsigned int)(raw / 10U),
                         (unsigned int)(raw % 10U));
        if (n > 0 && (size_t) n < sizeof(buf)) {
            uart_send_async((const uint8_t *) buf, (size_t) n, 0);
        }
    }

    vTaskDelay(pdMS_TO_TICKS(100));

    for (;;) {
        char buf[UART_TX_BUF_SIZE];
        unsigned long ticks = xTaskGetTickCount();
        unsigned long secs  = ticks / 1000;
        unsigned long ms    = ticks % 1000;

        uint16_t qps   = imu_get_qps();
        int16_t  yaw   = imu_get_yaw_deg100();
        (void) imu_get_pitch_deg100();
        (void) imu_get_roll_deg100();

        /* Motor encoder data (from motor sync task @ 10ms) */
        int16_t enc_left  = motor_get_encoder_10ms_left();
        int16_t enc_right = motor_get_encoder_10ms_right();
        int16_t spd_left  = motor_get_speed_left();
        int16_t spd_right = motor_get_speed_right();
        int32_t tot_left  = motor_get_encoder_left();
        int32_t tot_right = motor_get_encoder_right();
        uint16_t msync    = motor_get_sync_rate();
        float dist_left   = motor_get_distance_left_mm();
        float dist_right  = motor_get_distance_right_mm();

        int n = snprintf(buf, sizeof(buf),
                         "[%lu.%03lus] B:%lu G:%lu | qps:%-3u msync:%-3u yaw:%7.2f° | "
                         "enc L:%d R:%d | spd L:%d R:%d | total L:%ld R:%ld | dist L:%.1f R:%.1f mm\r\n",
                         secs, ms,
                         (unsigned long) led_get_blue(),
                         (unsigned long) led_get_green(),
                         (unsigned int) qps,
                         (unsigned int) msync,
                         (double) yaw   / 100.0,
                         (int) enc_left, (int) enc_right,
                         (int) spd_left, (int) spd_right,
                         (long) tot_left, (long) tot_right,
                         (double) dist_left, (double) dist_right);

        if (n > 0 && (size_t) n < sizeof(buf)) {
            uart_send_async((const uint8_t *) buf, (size_t) n, 0);
        }
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(100));
    }
}

/* ── Motor Init Task (prio 2): one-shot config, signals Logger, then delete ── */
void vMotorInitTask(void *pvParameters)
{
    (void) pvParameters;
    
    // car_ctrl_set_target_speed(500.0f, 500.0f);  /* stop car before motor init */
    /* Init UART1 + send stop commands (motor_driver_init calls motor_uart_init internally) */
    motor_driver_init();

    /* Run the TT encoder config sequence */
    bool ok = cmd_config_tt_encoder(&g_motor_driver_reg);

    motor_print_config(ok);

    /* Release Logger — motor init is done, encoder data is safe to read */
    xSemaphoreGive(g_motorDoneSem);

    /* Release MotorSync — motor init is done, sync can start */
    xSemaphoreGive(g_motorSyncSem);

    vTaskDelete(NULL);
}

/* ── Motor Sync Task (prio 3): periodic encoder read @ 10ms ── */
void vMotorSyncTask(void *pvParameters)
{
    (void) pvParameters;

    /* Wait for motor init to complete (signaled by vMotorInitTask) */
    xSemaphoreTake(g_motorSyncSem, portMAX_DELAY);

    if (!motor_is_initialized()) {
        vTaskDelete(NULL);
    }

    TickType_t xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        /* sync: read UART encoders → write shadow register */
        sync_encoder_from_device(&g_motor_driver_reg);

        /* convert encoder total → travel distance (mm) */
        motor_update_distance();

        /* release CarCtrl task (same priority → runs next) */
        xSemaphoreGive(g_ctrlSyncSem);

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(10));
    }
}

/* ── Car Speed Control Task (prio 3): PID @ 100Hz, triggered by MotorSync ──
 *
 *  Runs immediately after vMotorSyncTask each tick — same priority,
 *  semaphore-gated so encoder_10ms is always fresh.
 *  encoder_10ms → PID → PWM → flush_pwm_to_device */
void vCarCtrlTask(void *pvParameters)
{
    (void) pvParameters;

    /* Wait for motor init */
    xSemaphoreTake(g_motorSyncSem, portMAX_DELAY);

    if (!motor_is_initialized()) {
        vTaskDelete(NULL);
    }

    for (;;) {
        /* Block until vMotorSyncTask finishes sync + distance update */
        xSemaphoreTake(g_ctrlSyncSem, portMAX_DELAY);

        /* PID speed control → PWM → flush to device */
        car_ctrl_pid_tick();
    }
}
