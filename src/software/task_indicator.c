/*
 *  ======== task_indicator.c ========
 *  LED heartbeat + logger telemetry output.
 *
 *  vBlueTask   — blink blue LED (PB3) @ 750ms
 *  vGreenTask  — blink green LED (PB2) @ 1000ms
 *  vLoggerTask — prints IMU + motor encoder stats via DMA-UART @ 10Hz
 */

#include "include/app_tasks.h"
#include "include/build_in_led.h"
#include "include/led_reg.h"
#include "include/XDS110_cdc.h"
#include "include/imu_shadow.h"
#include "include/motor_driver_uart.h"

extern void i2c_scan_print_results(void);

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <stdio.h>

/* ---- Semaphores (created in main.c) ---- */
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

/* ── Logger task (prio 1): prints IMU + motor encoder stats via DMA-UART @ 10Hz ──
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
        unsigned long secs  = ticks / configTICK_RATE_HZ;
        unsigned long ms    = (ticks % configTICK_RATE_HZ) * 1000UL / configTICK_RATE_HZ;

        uint16_t qps   = imu_get_smooth_qps();
        int16_t  yaw   = imu_get_yaw_deg100();
        (void) imu_get_pitch_deg100();
        (void) imu_get_roll_deg100();

        /* Motor speed + encoder data — read from shadow register */
        float   spdL_mm_s  = motor_get_speed_left_mm_s();
        float   spdR_mm_s  = motor_get_speed_right_mm_s();

        float dist_left   = motor_get_distance_left_mm();
        float dist_right  = motor_get_distance_right_mm();
        uint16_t msync    = motor_get_smooth_sync_rate();
        int32_t enc_total_left  = motor_get_encoder_left();
        int32_t enc_total_right = motor_get_encoder_right();
        int16_t enc10ms_left  = motor_get_encoder_10ms_left();
        int16_t enc10ms_right = motor_get_encoder_10ms_right();

        int n = snprintf(buf, sizeof(buf),
                         "[%lu.%03lus] B:%lu G:%lu | qps:%-3u msync:%-3u | yaw:%7.2f° | "
                         "spd L:%5.0f R:%5.0f mm/s | "
                         "10ms L:%+5d R:%+5d | dist L:%.1f R:%.1f mm | enc L:%ld R:%ld\r\n",
                         secs, ms,
                         (unsigned long) led_get_blue(),
                         (unsigned long) led_get_green(),
                         (unsigned int) qps,
                         (unsigned int) msync,
                         (double) yaw   / 100.0,
                         (double) spdL_mm_s, (double) spdR_mm_s,
                         (int) enc10ms_left, (int) enc10ms_right,
                         (double) dist_left, (double) dist_right,
                         (long) enc_total_left, (long) enc_total_right);

        if (n > 0 && (size_t) n < sizeof(buf)) {
            uart_send_async((const uint8_t *) buf, (size_t) n, 0);
        }
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(100));
    }
}
