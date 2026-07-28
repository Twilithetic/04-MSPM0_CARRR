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
#include "include/motor_driver_reg.h"     /* motor driver shadow */

/* I2C functions (in src/driver/chip/I2C_test.c) */
extern void i2c_test_init(void);
extern void i2c_scan_bus(void);
extern void i2c_scan_print_results(void);

/* Motor Driver Proxy (in src/driver/board/motor_driver.c) */
extern bool motor_driver_init(void);
extern void sync_encoder_from_device(MotorDriverReg *r);
extern bool cmd_config_tt_encoder(MotorDriverReg *r);
extern void flush_speed_to_device(MotorDriverReg *r);
extern void flush_pwm_to_device(MotorDriverReg *r);
extern void flush_stop_to_device(MotorDriverReg *r);

/* IMU Proxy (in src/driver/board/LSM6DSV16X.c) */
extern bool lsm6dsv16x_is_present(void);
extern bool lsm6dsv16x_init(void);
extern void lsm6dsv16x_sync_from_device(void);

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <stdio.h>

/* ---- Semaphore (created in main.c before scheduler starts) ---- */
extern SemaphoreHandle_t g_scanDoneSem;
extern SemaphoreHandle_t g_motorDoneSem;

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

    for (;;) {
        char buf[UART_TX_BUF_SIZE];
        unsigned long ticks = xTaskGetTickCount();
        unsigned long secs  = ticks / 1000;
        unsigned long ms    = ticks % 1000;

        uint16_t qps   = imu_get_qps();
        int16_t  yaw   = imu_get_yaw_deg100();
        int16_t  pitch = imu_get_pitch_deg100();
        int16_t  roll  = imu_get_roll_deg100();

        /* Motor encoder data (from motor sync task @ 10ms) */
        int16_t enc_left  = motor_get_encoder_10ms_left();
        int16_t enc_right = motor_get_encoder_10ms_right();

        int n = snprintf(buf, sizeof(buf),
                         "[%lu.%03lus] B:%lu G:%lu | qps:%-3u yaw:%7.2f° | enc L:%d R:%d\r\n",
                         secs, ms,
                         (unsigned long) led_get_blue(),
                         (unsigned long) led_get_green(),
                         (unsigned int) qps,
                         (double) yaw   / 100.0,
                         (int) enc_left, (int) enc_right);

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

    /* Init GPIO bit-bang I2C pins */
    motor_driver_init();

    /* Run the TT encoder config sequence */
    bool ok = cmd_config_tt_encoder(&g_motor_driver_reg);

    if (ok) {
        char buf[64];
        int n = snprintf(buf, sizeof(buf),
                         "Motor init OK: type=%u enc=%u ratio=%u dia=%.1fmm dz=%u\r\n",
                         (unsigned int) motor_get_motor_type(),
                         (unsigned int) motor_get_pulse_line(),
                         (unsigned int) motor_get_reduction_ratio(),
                         (double) motor_get_wheel_diameter(),
                         (unsigned int) motor_get_deadzone());
        if (n > 0 && (size_t) n < sizeof(buf)) {
            uart_send_async((const uint8_t *) buf, (size_t) n, 0);
        }
    } else {
        char buf[48];
        int n = snprintf(buf, sizeof(buf),
                         "Motor init FAIL: err_step=0x%02X\r\n",
                         (unsigned int) motor_get_comm_status());
        if (n > 0 && (size_t) n < sizeof(buf)) {
            uart_send_async((const uint8_t *) buf, (size_t) n, 0);
        }
    }

    /* Release Logger — motor init is done, encoder data is safe to read */
    xSemaphoreGive(g_motorDoneSem);

    vTaskDelete(NULL);
}

/* ── Motor Sync Task (prio 3): periodic encoder read @ 10ms ── */
void vMotorSyncTask(void *pvParameters)
{
    (void) pvParameters;

    /* Wait ~500ms for motor init to complete */
    vTaskDelay(pdMS_TO_TICKS(500));

    if (!motor_is_initialized()) {
        vTaskDelete(NULL);
    }

    TickType_t xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        /* sync: read I2C encoders → write shadow register */
        sync_encoder_from_device(&g_motor_driver_reg);

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(10));
    }
}
