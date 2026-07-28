/*
 *  ======== motor_driver.c ========
 *  4-Way Motor Driver Board Proxy — GPIO bit-bang I2C → shadow register.
 *
 *  Architecture: Hardware Proxy + Shadow Register pattern.
 *    - Client tasks read g_motor_driver_reg (via accessors in motor_driver_reg.h).
 *    - This Proxy handles I2C communication and writes the shadow register.
 *
 *  Hardware:
 *    GPIO bit-bang I2C  PA15/SCL  PA16/SDA
 *    I2C primitives: src/driver/chip/I2C_soft.c (weak functions)
 *    Platform overrides: src/driver/board/i2c_soft_platform.c
 *    Motor driver board 7-bit address: 0x26
 *
 *  Reference: docs/4路电机驱动板/ (Arduino/ESP32/MSPM0 reference)
 */

#include "include/motor_driver_reg.h"
#include "ti_msp_dl_config.h"

#include <FreeRTOS.h>
#include <task.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* I2C_soft.c primitives (platform overrides in i2c_soft_platform.c) */
extern void I2C_Start(void);
extern void I2C_Stop(void);
extern void I2C_SendByte(uint8_t byte);
extern uint8_t I2C_ReceiveByte(void);
extern bool I2C_ReceiveAck(void);
extern void I2C_SendAck_Continue(void);
extern void I2C_SendAck_Done(void);

/* ====================================================================
 *  Constants
 * ==================================================================== */

#define MOTOR_I2C_ADDR         0x26U

/* Write registers */
#define REG_MOTOR_TYPE          0x01U
#define REG_DEADZONE            0x02U
#define REG_PULSE_LINE          0x03U
#define REG_REDUCTION_RATIO     0x04U
#define REG_WHEEL_DIAMETER      0x05U
#define REG_SPEED_CONTROL       0x06U
#define REG_PWM_CONTROL         0x07U

/* Read registers — 10ms encoder deltas */
#define REG_ENC_10MS_M1         0x10U
#define REG_ENC_10MS_M2         0x11U
#define REG_ENC_10MS_M3         0x12U
#define REG_ENC_10MS_M4         0x13U

/* Read registers — total encoder counts (high + low) */
#define REG_ENC_TOTAL_HIGH_M1   0x20U
#define REG_ENC_TOTAL_LOW_M1    0x21U
#define REG_ENC_TOTAL_HIGH_M2   0x22U
#define REG_ENC_TOTAL_LOW_M2    0x23U
#define REG_ENC_TOTAL_HIGH_M3   0x24U
#define REG_ENC_TOTAL_LOW_M3    0x25U
#define REG_ENC_TOTAL_HIGH_M4   0x26U
#define REG_ENC_TOTAL_LOW_M4    0x27U

/* Motor type: TT encoder */
#define MOTOR_TYPE_TT_ENCODER   3U

/* Default config (encoder 13 lines, ratio 45:1, wheel 67mm) */
#define DEFAULT_PULSE_LINE      13U
#define DEFAULT_REDUCTION_RATIO 45U
#define DEFAULT_DEADZONE        1250U

/* ====================================================================
 *  Low-level raw I2C helpers (static — NOT exposed to client tasks)
 * ==================================================================== */

static bool raw_write_reg(uint8_t reg, const uint8_t *data, uint8_t len)
{
    I2C_Start();
    I2C_SendByte((uint8_t)((MOTOR_I2C_ADDR << 1) | 0));
    if (!I2C_ReceiveAck()) { I2C_Stop(); return false; }
    I2C_SendByte(reg);
    if (!I2C_ReceiveAck()) { I2C_Stop(); return false; }
    for (uint8_t i = 0; i < len; i++) {
        I2C_SendByte(data[i]);
        if (!I2C_ReceiveAck()) { I2C_Stop(); return false; }
    }
    I2C_Stop();
    return true;
}

static bool raw_write_u8_reg(uint8_t reg, uint8_t val)          { return raw_write_reg(reg, &val, 1); }

static bool raw_write_u16be_reg(uint8_t reg, uint16_t val)
{
    uint8_t buf[2];
    buf[0] = (uint8_t)((val >> 8) & 0xFF);
    buf[1] = (uint8_t)(val & 0xFF);
    return raw_write_reg(reg, buf, 2);
}

static bool raw_write_4xi16be_reg(uint8_t reg, int16_t m1, int16_t m2, int16_t m3, int16_t m4)
{
    uint8_t buf[8];
    buf[0] = (uint8_t)((m1 >> 8) & 0xFF);  buf[1] = (uint8_t)(m1 & 0xFF);
    buf[2] = (uint8_t)((m2 >> 8) & 0xFF);  buf[3] = (uint8_t)(m2 & 0xFF);
    buf[4] = (uint8_t)((m3 >> 8) & 0xFF);  buf[5] = (uint8_t)(m3 & 0xFF);
    buf[6] = (uint8_t)((m4 >> 8) & 0xFF);  buf[7] = (uint8_t)(m4 & 0xFF);
    return raw_write_reg(reg, buf, 8);
}

static bool raw_write_float_le_reg(uint8_t reg, float val)
{
    uint8_t buf[4];
    (void) memcpy(buf, &val, sizeof(float));
    return raw_write_reg(reg, buf, 4);
}

static bool raw_read_reg(uint8_t reg, uint8_t *data, uint8_t len)
{
    uint8_t i;
    I2C_Start();
    I2C_SendByte((uint8_t)((MOTOR_I2C_ADDR << 1) | 0));
    if (!I2C_ReceiveAck()) { I2C_Stop(); return false; }
    I2C_SendByte(reg);
    if (!I2C_ReceiveAck()) { I2C_Stop(); return false; }

    I2C_Start();
    I2C_SendByte((uint8_t)((MOTOR_I2C_ADDR << 1) | 1));
    if (!I2C_ReceiveAck()) { I2C_Stop(); return false; }

    for (i = 0; i < (uint8_t)(len - 1U); i++) {
        data[i] = I2C_ReceiveByte();
        I2C_SendAck_Continue();
    }
    data[i] = I2C_ReceiveByte();
    I2C_SendAck_Done();
    I2C_Stop();
    return true;
}

static bool raw_read_i16be_reg(uint8_t reg, int16_t *out)
{
    uint8_t buf[2] = {0};
    if (!raw_read_reg(reg, buf, 2)) { return false; }
    *out = (int16_t)(((uint16_t)buf[0] << 8) | (uint16_t)buf[1]);
    return true;
}

/* ====================================================================
 *  Public API — SYNC (read I2C → write shadow, no return value)
 * ==================================================================== */

void sync_encoder_from_device(MotorDriverReg *r)
{
    int16_t delta;
    uint8_t hi_buf[2], lo_buf[2];

    /* M4 10ms delta (reg 0x13) — LEFT */
    if (raw_read_i16be_reg(REG_ENC_10MS_M4, &delta)) r->encoder_10ms_left  = delta;
    else { r->comm_status = REG_ENC_10MS_M4; return; }

    /* M2 10ms delta (reg 0x11) — RIGHT */
    if (raw_read_i16be_reg(REG_ENC_10MS_M2, &delta)) r->encoder_10ms_right = delta;
    else { r->comm_status = REG_ENC_10MS_M2; return; }

    /* M4 total (0x26+0x27) — LEFT */
    if (!raw_read_reg(REG_ENC_TOTAL_HIGH_M4, hi_buf, 2)) { r->comm_status = REG_ENC_TOTAL_HIGH_M4; return; }
    if (!raw_read_reg(REG_ENC_TOTAL_LOW_M4, lo_buf, 2))  { r->comm_status = REG_ENC_TOTAL_LOW_M4;  return; }
    r->encoder_total_left = (int32_t)(((uint32_t)hi_buf[0] << 24) | ((uint32_t)hi_buf[1] << 16) |
                                       ((uint32_t)lo_buf[0] << 8)  |  (uint32_t)lo_buf[1]);

    /* M2 total (0x22+0x23) — RIGHT */
    if (!raw_read_reg(REG_ENC_TOTAL_HIGH_M2, hi_buf, 2)) { r->comm_status = REG_ENC_TOTAL_HIGH_M2; return; }
    if (!raw_read_reg(REG_ENC_TOTAL_LOW_M2, lo_buf, 2))  { r->comm_status = REG_ENC_TOTAL_LOW_M2;  return; }
    r->encoder_total_right = (int32_t)(((uint32_t)hi_buf[0] << 24) | ((uint32_t)hi_buf[1] << 16) |
                                        ((uint32_t)lo_buf[0] << 8)  |  (uint32_t)lo_buf[1]);

    r->comm_status = 0;
}

void sync_config_from_device(MotorDriverReg *r) { (void) r; /* write-only */ }

/* ====================================================================
 *  Public API — FLUSH (read shadow → write I2C)
 * ==================================================================== */

void flush_speed_to_device(MotorDriverReg *r)
{
    /* M1=0, M2=right, M3=0, M4=left */
    if (!raw_write_4xi16be_reg(REG_SPEED_CONTROL, 0, r->target_speed_right, 0, r->target_speed_left))
        r->comm_status = REG_SPEED_CONTROL;
}

void flush_pwm_to_device(MotorDriverReg *r)
{
    if (!raw_write_4xi16be_reg(REG_PWM_CONTROL, 0, r->target_pwm_right, 0, r->target_pwm_left))
        r->comm_status = REG_PWM_CONTROL;
}

void flush_stop_to_device(MotorDriverReg *r)
{
    r->target_speed_left = r->target_speed_right = 0;
    r->target_pwm_left   = r->target_pwm_right   = 0;
    flush_speed_to_device(r);
    flush_pwm_to_device(r);
}

/* ====================================================================
 *  Public API — CMD
 * ==================================================================== */

bool cmd_config_tt_encoder(MotorDriverReg *r)
{
    if (!raw_write_u8_reg(REG_MOTOR_TYPE, MOTOR_TYPE_TT_ENCODER))
        { r->comm_status = REG_MOTOR_TYPE; return false; }
    vTaskDelay(pdMS_TO_TICKS(100));

    if (!raw_write_u16be_reg(REG_REDUCTION_RATIO, DEFAULT_REDUCTION_RATIO))
        { r->comm_status = REG_REDUCTION_RATIO; return false; }
    vTaskDelay(pdMS_TO_TICKS(100));

    if (!raw_write_u16be_reg(REG_PULSE_LINE, DEFAULT_PULSE_LINE))
        { r->comm_status = REG_PULSE_LINE; return false; }
    vTaskDelay(pdMS_TO_TICKS(100));

    if (!raw_write_float_le_reg(REG_WHEEL_DIAMETER, 67.0f))
        { r->comm_status = REG_WHEEL_DIAMETER; return false; }
    vTaskDelay(pdMS_TO_TICKS(100));

    if (!raw_write_u16be_reg(REG_DEADZONE, DEFAULT_DEADZONE))
        { r->comm_status = REG_DEADZONE; return false; }
    vTaskDelay(pdMS_TO_TICKS(100));

    r->motor_type = MOTOR_TYPE_TT_ENCODER;
    r->pulse_line = DEFAULT_PULSE_LINE;
    r->reduction_ratio = DEFAULT_REDUCTION_RATIO;
    r->wheel_diameter = 67.0f;
    r->deadzone = DEFAULT_DEADZONE;
    r->comm_status = 0;
    r->initialized = true;
    return true;
}

/* ====================================================================
 *  Public API — Init
 * ==================================================================== */

bool motor_driver_init(void)
{
    uint8_t stop_buf[8] = {0};
    (void) raw_write_reg(REG_SPEED_CONTROL, stop_buf, 8);
    (void) raw_write_reg(REG_PWM_CONTROL, stop_buf, 8);
    return true;
}
