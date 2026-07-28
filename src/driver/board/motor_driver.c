/*
 *  ======== motor_driver.c ========
 *  4-Way Motor Driver Board Proxy — GPIO bit-bang I2C → shadow register.
 *
 *  Architecture: Hardware Proxy + Shadow Register pattern.
 *    - Client tasks read g_motor_driver_reg (via accessors in motor_driver_reg.h).
 *    - This Proxy handles bit-bang I2C communication and writes the shadow register.
 *
 *  Hardware:
 *    GPIO bit-bang I2C  PA15/SCL  PA16/SDA @ ~100 kHz
 *    Motor driver board 7-bit address: 0x26
 *
 *  48-pin RHB package has no hardware I2C1 on PA15/PA16 (I2C1 is on PA14/PA16).
 *  We use GPIO bit-bang (software I2C) instead, referencing the MSPM0 CCS
 *  reference implementation (IOI2C.c / IOI2C.h).
 *
 *  Reference: docs/4路电机驱动板/  (Arduino/ESP32/MSPM0 reference implementations)
 */

#include "include/motor_driver_reg.h"

#include <FreeRTOS.h>
#include <task.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* DriverLib GPIO for bit-bang I2C */
#include <ti/devices/msp/msp.h>
#include <ti/driverlib/driverlib.h>

/* ====================================================================
 *  GPIO Bit-Bang I2C Pin Definitions
 *  PA15 = SCL  (output), PA16 = SDA  (open-drain emulation via input/output switch)
 * ==================================================================== */

#define I2C_PORT              GPIOA
#define I2C_SCL_PIN           DL_GPIO_PIN_15    /* PA15 */
#define I2C_SDA_PIN           DL_GPIO_PIN_16    /* PA16 */
#define I2C_SCL_IOMUX         IOMUX_PINCM16      /* PA15 */
#define I2C_SDA_IOMUX         IOMUX_PINCM17      /* PA16 */

/* ---- GPIO helpers (emulate open-drain SDA via direction switching) ---- */

#define SDA_IN()   DL_GPIO_initDigitalInput(I2C_SDA_IOMUX)
#define SDA_OUT()  do { \
    DL_GPIO_initDigitalOutput(I2C_SDA_IOMUX); \
    DL_GPIO_setPins(I2C_PORT, I2C_SDA_PIN);  \
    DL_GPIO_enableOutput(I2C_PORT, I2C_SDA_PIN); \
} while (0)

#define SCL_HIGH()  DL_GPIO_setPins(I2C_PORT, I2C_SCL_PIN)
#define SCL_LOW()   DL_GPIO_clearPins(I2C_PORT, I2C_SCL_PIN)
#define SDA_HIGH()  DL_GPIO_setPins(I2C_PORT, I2C_SDA_PIN)
#define SDA_LOW()   DL_GPIO_clearPins(I2C_PORT, I2C_SDA_PIN)
#define SDA_READ()  ((DL_GPIO_readPins(I2C_PORT, I2C_SDA_PIN) & I2C_SDA_PIN) ? 1 : 0)

/* ---- Simple microsecond delay (spin-loop, 32 MHz SYSOSC) ---- */
#define DELAY_US(us)  delay_cycles((uint32_t)(us) * 32U)

/* ====================================================================
 *  Constants
 * ==================================================================== */

#define MOTOR_I2C_ADDR         0x26U
#define MOTOR_I2C_TIMEOUT_MS   10U

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

/* Default config (user spec: encoder 13 lines, ratio 45:1, wheel 67mm) */
#define DEFAULT_PULSE_LINE      13U
#define DEFAULT_REDUCTION_RATIO 45U
#define DEFAULT_DEADZONE        1250U

/* ====================================================================
 *  Local state
 * ==================================================================== */

/* (no state needed — pure GPIO bit-bang) */

/* ====================================================================
 *  Bit-Bang I2C Primitives
 *
 *  Standard I2C protocol implemented via GPIO bit-bang (software I2C).
 *  Based on MSPM0 CCS reference implementation (IOI2C.c).
 * ==================================================================== */

static void i2c_start(void)
{
    SDA_OUT();
    SDA_HIGH();
    SCL_HIGH();
    DELAY_US(1);
    SDA_LOW();
    DELAY_US(1);
    SCL_LOW();
}

static void i2c_stop(void)
{
    SDA_OUT();
    SCL_LOW();
    SDA_LOW();
    SCL_HIGH();
    DELAY_US(1);
    SDA_HIGH();
    DELAY_US(1);
}

static uint8_t i2c_wait_ack(void)
{
    uint8_t ack_flag = 10;
    SCL_LOW();
    SDA_HIGH();
    SDA_IN();
    SCL_HIGH();
    while (SDA_READ() && ack_flag) {
        ack_flag--;
        DELAY_US(1);
    }
    if (ack_flag == 0) {
        i2c_stop();
        return 1;  /* NACK / timeout */
    }
    SCL_LOW();
    SDA_OUT();
    return 0;  /* ACK */
}

static void i2c_send_ack(uint8_t ack)
{
    SDA_OUT();
    SCL_LOW();
    SDA_LOW();
    DELAY_US(5);
    if (ack) { SDA_HIGH(); } else { SDA_LOW(); }
    SCL_HIGH();
    DELAY_US(5);
    SCL_LOW();
    SDA_HIGH();
}

static void i2c_send_byte(uint8_t txd)
{
    SDA_OUT();
    SCL_LOW();
    for (int i = 0; i < 8; i++) {
        if (txd & 0x80) { SDA_HIGH(); } else { SDA_LOW(); }
        DELAY_US(1);
        SCL_HIGH();
        DELAY_US(5);
        SCL_LOW();
        DELAY_US(5);
        txd <<= 1;
    }
}

static uint8_t i2c_read_byte(void)
{
    uint8_t receive = 0;
    SDA_IN();
    for (int i = 0; i < 8; i++) {
        SCL_LOW();
        DELAY_US(5);
        SCL_HIGH();
        DELAY_US(5);
        receive <<= 1;
        if (SDA_READ()) { receive |= 1; }
        DELAY_US(5);
    }
    SCL_LOW();
    return receive;
}

/* ====================================================================
 *  Low-level raw I2C helpers (static — NOT exposed to client tasks)
 * ==================================================================== */

/*
 *  Write N bytes to a register on the motor driver board.
 *  Format: [reg_addr] [data0] [data1] ... [dataN-1]
 *  Returns true on ACK.
 */
static bool raw_write_reg(uint8_t reg, const uint8_t *data, uint8_t len)
{
    uint8_t i = 0;
    i2c_start();
    i2c_send_byte((uint8_t)((MOTOR_I2C_ADDR << 1) | 0));
    if (i2c_wait_ack()) { i2c_stop(); return false; }
    i2c_send_byte(reg);
    if (i2c_wait_ack()) { i2c_stop(); return false; }
    for (i = 0; i < len; i++) {
        i2c_send_byte(data[i]);
        if (i2c_wait_ack()) { i2c_stop(); return false; }
    }
    i2c_stop();
    return true;
}

/* Write a single byte to a register */
static bool raw_write_u8_reg(uint8_t reg, uint8_t val)
{
    return raw_write_reg(reg, &val, 1);
}

/* Write a uint16 (big-endian) to a register */
static bool raw_write_u16be_reg(uint8_t reg, uint16_t val)
{
    uint8_t buf[2];
    buf[0] = (uint8_t)((val >> 8) & 0xFF);
    buf[1] = (uint8_t)(val & 0xFF);
    return raw_write_reg(reg, buf, 2);
}

/* Write four int16 values (big-endian) to the speed/PWM register */
static bool raw_write_4xi16be_reg(uint8_t reg, int16_t m1, int16_t m2,
                                   int16_t m3, int16_t m4)
{
    uint8_t buf[8];
    buf[0] = (uint8_t)((m1 >> 8) & 0xFF);
    buf[1] = (uint8_t)(m1 & 0xFF);
    buf[2] = (uint8_t)((m2 >> 8) & 0xFF);
    buf[3] = (uint8_t)(m2 & 0xFF);
    buf[4] = (uint8_t)((m3 >> 8) & 0xFF);
    buf[5] = (uint8_t)(m3 & 0xFF);
    buf[6] = (uint8_t)((m4 >> 8) & 0xFF);
    buf[7] = (uint8_t)(m4 & 0xFF);
    return raw_write_reg(reg, buf, 8);
}

/* Write float32 (little-endian) to a register */
static bool raw_write_float_le_reg(uint8_t reg, float val)
{
    uint8_t buf[4];
    (void) memcpy(buf, &val, sizeof(float));
    return raw_write_reg(reg, buf, 4);
}

/*
 *  Read N bytes from a register on the motor driver board.
 *  Standard I2C write-then-read: write reg addr → repeated START → read data.
 *  Returns true on success.
 */
static bool raw_read_reg(uint8_t reg, uint8_t *data, uint8_t len)
{
    uint8_t i;
    i2c_start();
    i2c_send_byte((uint8_t)((MOTOR_I2C_ADDR << 1) | 0));
    if (i2c_wait_ack()) { i2c_stop(); return false; }
    i2c_send_byte(reg);
    if (i2c_wait_ack()) { i2c_stop(); return false; }

    i2c_start();
    i2c_send_byte((uint8_t)((MOTOR_I2C_ADDR << 1) | 1));
    if (i2c_wait_ack()) { i2c_stop(); return false; }

    for (i = 0; i < (uint8_t)(len - 1U); i++) {
        data[i] = i2c_read_byte();
        i2c_send_ack(0);
    }
    data[i] = i2c_read_byte();
    i2c_send_ack(1);
    i2c_stop();
    return true;
}

/* Read int16 (big-endian) from a register */
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

/*
 *  sync_encoder_from_device: Read all encoder values from motor board
 *  and write them into the shadow register.
 *
 *  Only M2 (left) and M4 (right) are physically present.
 *  The board always returns 4 motor slots — M1/M3 reads are made
 *  to keep the I2C bus happy but their values are discarded.
 *
 *  Reads:
 *    M2: 0x11 (10ms delta), 0x22/0x23 (total count)
 *    M4: 0x13 (10ms delta), 0x26/0x27 (total count)
 */
void sync_encoder_from_device(MotorDriverReg *r)
{
    int16_t delta;
    uint8_t hi_buf[2], lo_buf[2];

    /* ── 10ms encoder deltas (M2=right, M4=left) ── */

    /* M4 10ms delta (reg 0x13) — LEFT wheel */
    if (raw_read_i16be_reg(REG_ENC_10MS_M4, &delta)) {
        r->encoder_10ms_left = delta;
    } else {
        r->comm_status = REG_ENC_10MS_M4;
        return;
    }

    /* M2 10ms delta (reg 0x11) — RIGHT wheel */
    if (raw_read_i16be_reg(REG_ENC_10MS_M2, &delta)) {
        r->encoder_10ms_right = delta;
    } else {
        r->comm_status = REG_ENC_10MS_M2;
        return;
    }

    /* ── Total encoder counts ── */

    /* M4 total (reg 0x26 high + 0x27 low) — LEFT wheel */
    if (!raw_read_reg(REG_ENC_TOTAL_HIGH_M4, hi_buf, 2)) {
        r->comm_status = REG_ENC_TOTAL_HIGH_M4;
        return;
    }
    if (!raw_read_reg(REG_ENC_TOTAL_LOW_M4, lo_buf, 2)) {
        r->comm_status = REG_ENC_TOTAL_LOW_M4;
        return;
    }
    r->encoder_total_left = (int32_t)(
        ((uint32_t)hi_buf[0] << 24) | ((uint32_t)hi_buf[1] << 16) |
        ((uint32_t)lo_buf[0] << 8)  | ((uint32_t)lo_buf[1])
    );

    /* M2 total (reg 0x22 high + 0x23 low) — RIGHT wheel */
    if (!raw_read_reg(REG_ENC_TOTAL_HIGH_M2, hi_buf, 2)) {
        r->comm_status = REG_ENC_TOTAL_HIGH_M2;
        return;
    }
    if (!raw_read_reg(REG_ENC_TOTAL_LOW_M2, lo_buf, 2)) {
        r->comm_status = REG_ENC_TOTAL_LOW_M2;
        return;
    }
    r->encoder_total_right = (int32_t)(
        ((uint32_t)hi_buf[0] << 24) | ((uint32_t)hi_buf[1] << 16) |
        ((uint32_t)lo_buf[0] << 8)  | ((uint32_t)lo_buf[1])
    );

    r->comm_status = 0;  /* all OK */
}

/*
 *  sync_config_from_device: Read motor config back from the device
 *  into the shadow register.
 */
void sync_config_from_device(MotorDriverReg *r)
{
    /* Config registers are write-only on the motor board,
     * so this reads back from the shadow (values set during init).
     * If the motor board supported config readback, it would go here. */
    (void) r;
}

/* ====================================================================
 *  Public API — FLUSH (read shadow target_* → write I2C, no value params)
 * ==================================================================== */

/*
 *  flush_speed_to_device: Read target_speed_left/right from shadow → I2C write.
 *  Caller MUST set r->target_speed_left/right BEFORE calling this.
 *  M1/M3 are always 0 (no motor connected).
 */
void flush_speed_to_device(MotorDriverReg *r)
{
    int16_t left  = r->target_speed_left;
    int16_t right = r->target_speed_right;

    /* M1=0, M2=right, M3=0, M4=left */
    if (!raw_write_4xi16be_reg(REG_SPEED_CONTROL, 0, right, 0, left)) {
        r->comm_status = REG_SPEED_CONTROL;
    }
}

/*
 *  flush_pwm_to_device: Read target_pwm_left/right from shadow → I2C write.
 *  Caller MUST set r->target_pwm_left/right BEFORE calling this.
 *  M1/M3 are always 0 (no motor connected).
 */
void flush_pwm_to_device(MotorDriverReg *r)
{
    int16_t left  = r->target_pwm_left;
    int16_t right = r->target_pwm_right;

    /* M1=0, M2=right, M3=0, M4=left */
    if (!raw_write_4xi16be_reg(REG_PWM_CONTROL, 0, right, 0, left)) {
        r->comm_status = REG_PWM_CONTROL;
    }
}

/*
 *  flush_stop_to_device: Zero target_left/right in shadow, then flush both
 *  speed and PWM to stop both motors.
 */
void flush_stop_to_device(MotorDriverReg *r)
{
    r->target_speed_left  = 0;
    r->target_speed_right = 0;
    r->target_pwm_left    = 0;
    r->target_pwm_right   = 0;

    flush_speed_to_device(r);
    flush_pwm_to_device(r);
}

/* ====================================================================
 *  Public API — CMD (device commands, fixed values)
 * ==================================================================== */

/*
 *  cmd_config_tt_encoder: Full TT encoder init sequence.
 *
 *  Writes 5 config registers with 100ms delays between each
 *  (matching the Arduino/ESP32/MSPM0 reference implementations).
 *
 *  Returns true on success, false on failure (comm_status set to failed reg).
 */
bool cmd_config_tt_encoder(MotorDriverReg *r)
{
    /* 1. Motor type = TT encoder */
    if (!raw_write_u8_reg(REG_MOTOR_TYPE, MOTOR_TYPE_TT_ENCODER)) {
        r->comm_status = REG_MOTOR_TYPE;
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(100));

    /* 2. Reduction ratio = 45 */
    if (!raw_write_u16be_reg(REG_REDUCTION_RATIO, DEFAULT_REDUCTION_RATIO)) {
        r->comm_status = REG_REDUCTION_RATIO;
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(100));

    /* 3. Encoder lines = 13 */
    if (!raw_write_u16be_reg(REG_PULSE_LINE, DEFAULT_PULSE_LINE)) {
        r->comm_status = REG_PULSE_LINE;
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(100));

    /* 4. Wheel diameter = 67.0 mm (float32 LE) */
    if (!raw_write_float_le_reg(REG_WHEEL_DIAMETER, 67.0f)) {
        r->comm_status = REG_WHEEL_DIAMETER;
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(100));

    /* 5. Dead zone = 1250 */
    if (!raw_write_u16be_reg(REG_DEADZONE, DEFAULT_DEADZONE)) {
        r->comm_status = REG_DEADZONE;
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(100));

    /* Update shadow with config values */
    r->motor_type      = MOTOR_TYPE_TT_ENCODER;
    r->pulse_line      = DEFAULT_PULSE_LINE;
    r->reduction_ratio = DEFAULT_REDUCTION_RATIO;
    r->wheel_diameter  = 67.0f;
    r->deadzone        = DEFAULT_DEADZONE;
    r->comm_status     = 0;
    r->initialized     = true;

    return true;
}

/* ====================================================================
 *  Public API — Init
 * ==================================================================== */

/*
 *  motor_driver_init: Initialise GPIO pins for bit-bang I2C, send stop command
 *  to all motors.
 *
 *  We use GPIO bit-bang (not hardware I2C1), so there is no I2C_Handle.
 *  Returns true on success (always succeeds — GPIO init cannot fail).
 */
bool motor_driver_init(void)
{
    /* ── 1. Configure PA15 (SCL) and PA16 (SDA) as digital outputs ── */
    DL_GPIO_initDigitalOutput(I2C_SCL_IOMUX);
    DL_GPIO_initDigitalOutput(I2C_SDA_IOMUX);
    DL_GPIO_setPins(I2C_PORT, I2C_SCL_PIN | I2C_SDA_PIN);
    DL_GPIO_enableOutput(I2C_PORT, I2C_SCL_PIN | I2C_SDA_PIN);

    /* ── 2. Stop all motors (write zero speed + PWM) ── */
    {
        uint8_t stop_buf[8] = {0};
        (void) raw_write_reg(REG_SPEED_CONTROL, stop_buf, 8);
        (void) raw_write_reg(REG_PWM_CONTROL, stop_buf, 8);
    }

    return true;
}
