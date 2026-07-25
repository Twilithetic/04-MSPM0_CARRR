/*
 *  ======== motor.c ========
 *  Motor driver I2C bit-bang proxy.
 *  Extracted from empty.c: all I2C primitives, raw_* functions,
 *  sync_* / flush_* / cmd_* public API.
 *
 *  Architecture:
 *     Level 0 - I2C bit-bang GPIO primitives (static)
 *     Level 1 - I2C protocol functions (static)
 *     Level 2 - raw_* read/write wrappers (static)
 *     Level 3 - Public sync_* / flush_* / cmd_* API
 */

#include "include/motor.h"
#include "include/uart_debug.h"
#include "ti_msp_dl_config.h"

#include <stdbool.h>
#include <stdint.h>

/* ====================================================================
 *  Constants — I2C timing, registers, motor parameters
 * ==================================================================== */

#define MOTOR_I2C_ADDR            (0x26U)

#define REG_MOTOR_TYPE            (0x01U)
#define REG_MOTOR_DEADZONE        (0x02U)
#define REG_MOTOR_PULSE_LINE      (0x03U)
#define REG_MOTOR_REDUCTION_RATIO (0x04U)
#define REG_WHEEL_DIAMETER        (0x05U)
#define REG_SPEED_CONTROL         (0x06U)
#define REG_PWM_CONTROL           (0x07U)
#define REG_ENCODER_10MS_M1       (0x10U)
#define REG_ENCODER_ALL_HIGH_M1   (0x20U)

#define TT_ENCODER_MOTOR_TYPE     (3U)
#define TT_ENCODER_LINES          (13U)
#define TT_REDUCTION_RATIO        (45U)
#define TT_WHEEL_DIAMETER_MM      (68.0f)
#define TT_DEADZONE               (1250U)

#define M2_FORWARD_SIGN           (-1)
#define M4_FORWARD_SIGN           (-1)

#define I2C_DELAY_CYCLES          (160U)
#define DELAY_1MS_CYCLES          (32000U)
#define DELAY_100MS_CYCLES        (3200000U)

/* ====================================================================
 *  Level 0 — I2C GPIO bit-bang primitives
 * ==================================================================== */

static void i2c_delay(void)
{
    delay_cycles(I2C_DELAY_CYCLES);
}

static void sda_release(void)
{
    DL_GPIO_setPins(MOTOR_I2C_PORT, MOTOR_I2C_SDA_PIN);
    DL_GPIO_disableOutput(MOTOR_I2C_PORT, MOTOR_I2C_SDA_PIN);
    DL_GPIO_initDigitalInputFeatures(MOTOR_I2C_SDA_IOMUX,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
        DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
}

static void sda_drive_low(void)
{
    DL_GPIO_initDigitalOutputFeatures(MOTOR_I2C_SDA_IOMUX,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
        DL_GPIO_DRIVE_STRENGTH_LOW, DL_GPIO_HIZ_DISABLE);
    DL_GPIO_clearPins(MOTOR_I2C_PORT, MOTOR_I2C_SDA_PIN);
    DL_GPIO_enableOutput(MOTOR_I2C_PORT, MOTOR_I2C_SDA_PIN);
}

static void sda_output(void)
{
    sda_release();
}

static void sda_write(bool high)
{
    if (high) {
        sda_release();
    } else {
        sda_drive_low();
    }
}

static void scl_write(bool high)
{
    if (high) {
        DL_GPIO_setPins(MOTOR_I2C_PORT, MOTOR_I2C_SCL_PIN);
    } else {
        DL_GPIO_clearPins(MOTOR_I2C_PORT, MOTOR_I2C_SCL_PIN);
    }
}

static bool sda_read(void)
{
    return (DL_GPIO_readPins(MOTOR_I2C_PORT, MOTOR_I2C_SDA_PIN)
            & MOTOR_I2C_SDA_PIN) != 0U;
}

/* ====================================================================
 *  Level 1 — I2C protocol functions
 * ==================================================================== */

static void i2c_idle(void)
{
    sda_output();
    sda_write(true);
    scl_write(true);
    i2c_delay();
}

static void i2c_start(void)
{
    sda_output();
    sda_write(true);
    scl_write(true);
    i2c_delay();
    sda_write(false);
    i2c_delay();
    scl_write(false);
}

static void i2c_stop(void)
{
    sda_output();
    scl_write(false);
    sda_write(false);
    i2c_delay();
    scl_write(true);
    i2c_delay();
    sda_write(true);
    i2c_delay();
}

static void i2c_send_byte(uint8_t data)
{
    scl_write(false);

    for (uint8_t i = 0; i < 8U; i++) {
        sda_write((data & 0x80U) != 0U);
        i2c_delay();
        scl_write(true);
        i2c_delay();
        scl_write(false);
        data <<= 1;
    }
}

static bool i2c_wait_ack(void)
{
    bool nack;
    uint16_t timeout = 100U;

    sda_release();
    i2c_delay();
    scl_write(true);
    i2c_delay();

    while (sda_read() && (timeout > 0U)) {
        timeout--;
        i2c_delay();
    }

    nack = sda_read();
    scl_write(false);
    sda_release();
    return !nack;
}

static uint8_t i2c_read_byte(void)
{
    uint8_t data = 0U;

    sda_release();
    for (uint8_t i = 0; i < 8U; i++) {
        scl_write(false);
        i2c_delay();
        scl_write(true);
        i2c_delay();
        data <<= 1;
        if (sda_read()) {
            data |= 1U;
        }
    }
    scl_write(false);
    sda_release();

    return data;
}

static void i2c_send_ack(bool ack)
{
    scl_write(false);
    sda_write(!ack);
    i2c_delay();
    scl_write(true);
    i2c_delay();
    scl_write(false);
    sda_write(true);
}

static uint8_t i2c_write(uint8_t reg, const uint8_t *data, uint8_t len)
{
    i2c_start();
    i2c_send_byte((uint8_t) (MOTOR_I2C_ADDR << 1));
    if (!i2c_wait_ack()) {
        i2c_stop();
        return 1U;
    }

    i2c_send_byte(reg);
    if (!i2c_wait_ack()) {
        i2c_stop();
        return 2U;
    }

    for (uint8_t i = 0U; i < len; i++) {
        i2c_send_byte(data[i]);
        if (!i2c_wait_ack()) {
            i2c_stop();
            return (uint8_t) (3U + i);
        }
    }

    i2c_stop();
    return 0U;
}

static uint8_t i2c_read(uint8_t reg, uint8_t *buf, uint8_t len)
{
    i2c_start();
    i2c_send_byte((uint8_t) (MOTOR_I2C_ADDR << 1));
    if (!i2c_wait_ack()) {
        i2c_stop();
        return 1U;
    }

    i2c_send_byte(reg);
    if (!i2c_wait_ack()) {
        i2c_stop();
        return 2U;
    }

    i2c_start();
    i2c_send_byte((uint8_t) ((MOTOR_I2C_ADDR << 1) | 1U));
    if (!i2c_wait_ack()) {
        i2c_stop();
        delay_cycles(DELAY_1MS_CYCLES);

        i2c_start();
        i2c_send_byte((uint8_t) ((MOTOR_I2C_ADDR << 1) | 1U));
        if (!i2c_wait_ack()) {
            i2c_stop();
            return 3U;
        }
    }

    for (uint8_t i = 0U; i < len; i++) {
        buf[i] = i2c_read_byte();
        i2c_send_ack(i < (uint8_t) (len - 1U));
    }

    i2c_stop();
    return 0U;
}

/* ====================================================================
 *  Level 2 — raw_* protocol wrappers (static, pure I/O, no business logic)
 * ==================================================================== */

static uint8_t raw_write_u8(uint8_t reg, uint8_t value)
{
    return i2c_write(reg, &value, 1U);
}

static uint8_t raw_write_u16(uint8_t reg, uint16_t value)
{
    uint8_t bytes[2];
    bytes[0] = (uint8_t) (value >> 8);
    bytes[1] = (uint8_t) value;
    return i2c_write(reg, bytes, 2U);
}

static uint8_t raw_write_float(uint8_t reg, float value)
{
    union {
        float   f;
        uint8_t bytes[4];
    } data;
    data.f = value;
    return i2c_write(reg, data.bytes, 4U);
}

static uint8_t raw_write_i16_array(uint8_t reg, const int16_t *vals, uint8_t count)
{
    uint8_t bytes[8];
    for (uint8_t i = 0U; i < count; i++) {
        uint16_t raw = (uint16_t) vals[i];
        bytes[i * 2U]     = (uint8_t) (raw >> 8);
        bytes[(i * 2U) + 1U] = (uint8_t) raw;
    }
    return i2c_write(reg, bytes, count * 2U);
}

static uint8_t raw_read_i16(uint8_t reg, int16_t *value)
{
    uint8_t bytes[2];
    uint8_t status = i2c_read(reg, bytes, sizeof(bytes));
    if (status == 0U) {
        *value = (int16_t) (((uint16_t) bytes[0] << 8) | bytes[1]);
    }
    return status;
}

static uint8_t raw_read_i32_pair(uint8_t high_reg, int32_t *value)
{
    uint8_t high[2];
    uint8_t low[2];
    uint8_t status = i2c_read(high_reg, high, sizeof(high));
    if (status != 0U) {
        return status;
    }
    status = i2c_read((uint8_t) (high_reg + 1U), low, sizeof(low));
    if (status == 0U) {
        uint32_t raw = ((uint32_t) high[0] << 24)
                     | ((uint32_t) high[1] << 16)
                     | ((uint32_t) low[0]  << 8)
                     |  (uint32_t) low[1];
        *value = (int32_t) raw;
    }
    return status;
}

static uint8_t raw_read_all_encoders(int32_t encoders[4])
{
    for (uint8_t i = 0U; i < 4U; i++) {
        uint8_t status = raw_read_i32_pair(
            (uint8_t) (REG_ENCODER_ALL_HIGH_M1 + (i * 2U)), &encoders[i]);
        if (status != 0U) {
            return status;
        }
    }
    return 0U;
}

static uint8_t raw_read_10ms_encoders(int16_t encoders[4])
{
    for (uint8_t i = 0U; i < 4U; i++) {
        uint8_t status = raw_read_i16(
            (uint8_t) (REG_ENCODER_10MS_M1 + i), &encoders[i]);
        if (status != 0U) {
            return status;
        }
    }
    return 0U;
}

/* ---- Sign helper (wiring-dependent direction invert) ---- */

static int16_t apply_motor_sign(int16_t pwm, int8_t sign)
{
    return (sign < 0) ? (int16_t) -pwm : pwm;
}

/* ====================================================================
 *  Level 3 — Public sync_* / flush_* / cmd_* API
 * ==================================================================== */

/* ---- SYNC: read I2C → write shadow (no return) ---- */

void sync_encoder_from_device(const MotorProxy *p, MotorReg *r)
{
    int32_t total[4] = {0};
    int16_t delta[4] = {0};
    uint8_t s1 = raw_read_all_encoders(total);
    uint8_t s2 = raw_read_10ms_encoders(delta);

    r->comm_status = (s1 != 0U) ? s1 : s2;

    /* On failure: do NOT modify shadow (preserves old valid values) */
    if (r->comm_status == 0U) {
        for (uint8_t i = 0U; i < 4U; i++) {
            r->encoder_total[i] = total[i];
            r->encoder_10ms[i]  = delta[i];
        }
    }
}

void sync_config_from_device(const MotorProxy *p, MotorReg *r)
{
    /* Read-back config registers for shadow consistency */
    int16_t val16;
    uint8_t s;

    s = raw_read_i16(REG_MOTOR_TYPE, &val16);
    if (s == 0U) { r->motor_type = (uint16_t) val16; }

    s = raw_read_i16(REG_MOTOR_PULSE_LINE, &val16);
    if (s == 0U) { r->pulse_line = (uint16_t) val16; }

    s = raw_read_i16(REG_MOTOR_REDUCTION_RATIO, &val16);
    if (s == 0U) { r->reduction_ratio = (uint16_t) val16; }

    s = raw_read_i16(REG_MOTOR_DEADZONE, &val16);
    if (s == 0U) { r->deadzone = (uint16_t) val16; }
}

/* ---- FLUSH: read shadow target_* → write I2C (no value params) ---- */

void flush_speed_to_device(const MotorProxy *p, MotorReg *r)
{
    int16_t speeds[4] = {
        0,  /* M1 unused */
        apply_motor_sign(r->target_speed_m2, M2_FORWARD_SIGN),
        0,  /* M3 unused */
        apply_motor_sign(r->target_speed_m4, M4_FORWARD_SIGN),
    };
    r->comm_status = raw_write_i16_array(REG_SPEED_CONTROL, speeds, 4);
}

void flush_pwm_to_device(const MotorProxy *p, MotorReg *r)
{
    int16_t pwms[4] = {
        r->target_pwm_m1,
        apply_motor_sign(r->target_pwm_m2, M2_FORWARD_SIGN),
        r->target_pwm_m3,
        apply_motor_sign(r->target_pwm_m4, M4_FORWARD_SIGN),
    };
    r->comm_status = raw_write_i16_array(REG_PWM_CONTROL, pwms, 4);
}

void flush_stop_to_device(const MotorProxy *p, MotorReg *r)
{
    /* Zero all target_* fields in shadow */
    r->target_speed_m1 = 0;
    r->target_speed_m2 = 0;
    r->target_speed_m3 = 0;
    r->target_speed_m4 = 0;
    r->target_pwm_m1   = 0;
    r->target_pwm_m2   = 0;
    r->target_pwm_m3   = 0;
    r->target_pwm_m4   = 0;

    /* Flush both registers */
    uint8_t s1 = raw_write_i16_array(REG_SPEED_CONTROL,
        (const int16_t[]){0, 0, 0, 0}, 4);
    uint8_t s2 = raw_write_i16_array(REG_PWM_CONTROL,
        (const int16_t[]){0, 0, 0, 0}, 4);
    r->comm_status = (s1 != 0U) ? s1 : s2;
}

/* ---- CMD: device commands (fixed values) ---- */

void cmd_config_tt_encoder(const MotorProxy *p)
{
    uint8_t status;

    status = raw_write_u8(REG_MOTOR_TYPE, TT_ENCODER_MOTOR_TYPE);
    uart_write_str("set motor type TT encoder");
    uart_write_str(status == 0U ? ": OK\r\n" : ": IIC error ");
    if (status != 0U) { uart_write_u32(status); uart_write_str("\r\n"); return; }
    delay_cycles(DELAY_100MS_CYCLES);

    status = raw_write_u16(REG_MOTOR_REDUCTION_RATIO, TT_REDUCTION_RATIO);
    uart_write_str("set reduction ratio 45");
    uart_write_str(status == 0U ? ": OK\r\n" : ": IIC error ");
    if (status != 0U) { uart_write_u32(status); uart_write_str("\r\n"); return; }
    delay_cycles(DELAY_100MS_CYCLES);

    status = raw_write_u16(REG_MOTOR_PULSE_LINE, TT_ENCODER_LINES);
    uart_write_str("set encoder lines 13");
    uart_write_str(status == 0U ? ": OK\r\n" : ": IIC error ");
    if (status != 0U) { uart_write_u32(status); uart_write_str("\r\n"); return; }
    delay_cycles(DELAY_100MS_CYCLES);

    status = raw_write_float(REG_WHEEL_DIAMETER, TT_WHEEL_DIAMETER_MM);
    uart_write_str("set wheel diameter 68mm");
    uart_write_str(status == 0U ? ": OK\r\n" : ": IIC error ");
    if (status != 0U) { uart_write_u32(status); uart_write_str("\r\n"); return; }
    delay_cycles(DELAY_100MS_CYCLES);

    status = raw_write_u16(REG_MOTOR_DEADZONE, TT_DEADZONE);
    uart_write_str("set deadzone 1250");
    uart_write_str(status == 0U ? ": OK\r\n" : ": IIC error ");
    if (status != 0U) { uart_write_u32(status); uart_write_str("\r\n"); }
}

/* ---- Init ---- */

void motor_proxy_init(const MotorProxy *p)
{
    (void) p;
    i2c_idle();

    /* Raw stop using local zeroed array — not through shadow */
    raw_write_i16_array(REG_SPEED_CONTROL, (const int16_t[]){0, 0, 0, 0}, 4);
    raw_write_i16_array(REG_PWM_CONTROL,   (const int16_t[]){0, 0, 0, 0}, 4);
}
