#include "ti_msp_dl_config.h"

#include <stdbool.h>
#include <stdint.h>

#define MOTOR_I2C_ADDR              (0x26U)

#define REG_MOTOR_TYPE              (0x01U)
#define REG_MOTOR_DEADZONE          (0x02U)
#define REG_MOTOR_PULSE_LINE        (0x03U)
#define REG_MOTOR_REDUCTION_RATIO   (0x04U)
#define REG_WHEEL_DIAMETER          (0x05U)
#define REG_SPEED_CONTROL           (0x06U)
#define REG_PWM_CONTROL             (0x07U)
#define REG_ENCODER_10MS_M1         (0x10U)
#define REG_ENCODER_ALL_HIGH_M1     (0x20U)

#define TT_ENCODER_MOTOR_TYPE       (3U)
#define TT_ENCODER_LINES            (13U)
#define TT_REDUCTION_RATIO          (45U)
#define TT_WHEEL_DIAMETER_MM        (68.0f)
#define TT_DEADZONE                 (1250U)

#define LINE_BLACK_LEVEL            (0U)
#define LINE_LEFT_BASE_SPEED        (110)
#define LINE_RIGHT_BASE_SPEED       (110)
#define LINE_MAX_SPEED              (180)
#define LINE_CORRECTION_MAX         (50)
#define LINE_KP_NUM                 (1)
#define LINE_KP_DEN                 (20)
#define LINE_KD_NUM                 (1)
#define LINE_KD_DEN                 (45)
#define LINE_TURN_SIGN              (-1)
#define LINE_STOP_ON_ALL_BLACK      (1U)
#define LINE_SEARCH_WHEN_LOST       (0U)
#define LINE_SEARCH_SPEED           (100)
#define LINE_LOOP_DELAY_CYCLES      (960000U)
#define LINE_PRINT_INTERVAL_LOOPS   (5U)
#define LINE_ENCODER_INTERVAL_LOOPS (20U)

#define M2_FORWARD_SIGN             (-1)
#define M4_FORWARD_SIGN             (-1)

#define I2C_DELAY_CYCLES            (160U)
#define DELAY_1MS_CYCLES            (32000U)
#define DELAY_100MS_CYCLES          (3200000U)
#define DELAY_300MS_CYCLES          (9600000U)
#define DELAY_1S_CYCLES             (32000000U)

#define GRAY_ALL_PINS               (GRAY_S1_PIN | GRAY_S2_PIN | GRAY_S3_PIN | GRAY_S4_PIN | GRAY_S5_PIN)

typedef struct {
    uint8_t raw[5];
    uint8_t line[5];
    uint8_t active_count;
    uint8_t mask;
    int16_t position;
    int16_t error;
} line_sample_t;

static const uint32_t g_gray_pins[5] = {
    GRAY_S1_PIN,
    GRAY_S2_PIN,
    GRAY_S3_PIN,
    GRAY_S4_PIN,
    GRAY_S5_PIN,
};

static const int16_t g_gray_positions[5] = {
    0,
    1000,
    2000,
    3000,
    4000,
};

static void uart_write_char(char ch)
{
    DL_UART_Main_transmitDataBlocking(UART_0_INST, (uint8_t) ch);
}

static void uart_write_str(const char *str)
{
    while (*str != '\0') {
        uart_write_char(*str++);
    }
}

static void uart_write_u32(uint32_t value)
{
    char buf[10];
    uint32_t index = 0;

    if (value == 0U) {
        uart_write_char('0');
        return;
    }

    while ((value > 0U) && (index < sizeof(buf))) {
        buf[index++] = (char) ('0' + (value % 10U));
        value /= 10U;
    }

    while (index > 0U) {
        uart_write_char(buf[--index]);
    }
}

static void uart_write_i32(int32_t value)
{
    uint32_t magnitude;

    if (value < 0) {
        uart_write_char('-');
        magnitude = (uint32_t) (-(value + 1)) + 1U;
    } else {
        magnitude = (uint32_t) value;
    }

    uart_write_u32(magnitude);
}

static void print_status(const char *name, uint8_t status)
{
    uart_write_str(name);
    uart_write_str(status == 0U ? ": OK\r\n" : ": IIC error ");
    if (status != 0U) {
        uart_write_u32(status);
        uart_write_str("\r\n");
    }
}

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
    return (DL_GPIO_readPins(MOTOR_I2C_PORT, MOTOR_I2C_SDA_PIN) & MOTOR_I2C_SDA_PIN) != 0U;
}

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

static uint8_t motor_write(uint8_t reg, const uint8_t *data, uint8_t len)
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

static uint8_t motor_read(uint8_t reg, uint8_t *buf, uint8_t len)
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

static uint8_t motor_set_u8(uint8_t reg, uint8_t value)
{
    return motor_write(reg, &value, 1U);
}

static uint8_t motor_set_u16(uint8_t reg, uint16_t value)
{
    uint8_t bytes[2];

    bytes[0] = (uint8_t) (value >> 8);
    bytes[1] = (uint8_t) value;
    return motor_write(reg, bytes, 2U);
}

static uint8_t motor_set_float(uint8_t reg, float value)
{
    union {
        float f;
        uint8_t bytes[4];
    } data;

    data.f = value;
    return motor_write(reg, data.bytes, 4U);
}

static uint8_t motor_control(uint8_t reg, int16_t m1, int16_t m2, int16_t m3, int16_t m4)
{
    int16_t values[4] = {m1, m2, m3, m4};
    uint8_t bytes[8];

    for (uint8_t i = 0U; i < 4U; i++) {
        uint16_t raw = (uint16_t) values[i];
        bytes[i * 2U] = (uint8_t) (raw >> 8);
        bytes[(i * 2U) + 1U] = (uint8_t) raw;
    }

    return motor_write(reg, bytes, sizeof(bytes));
}

static uint8_t motor_pwm(int16_t m1, int16_t m2, int16_t m3, int16_t m4)
{
    return motor_control(REG_PWM_CONTROL, m1, m2, m3, m4);
}

static uint8_t motor_speed(int16_t m1, int16_t m2, int16_t m3, int16_t m4)
{
    return motor_control(REG_SPEED_CONTROL, m1, m2, m3, m4);
}

static uint8_t motor_read_i16(uint8_t reg, int16_t *value)
{
    uint8_t bytes[2];
    uint8_t status = motor_read(reg, bytes, sizeof(bytes));

    if (status == 0U) {
        *value = (int16_t) (((uint16_t) bytes[0] << 8) | bytes[1]);
    }

    return status;
}

static uint8_t motor_read_i32_pair(uint8_t high_reg, int32_t *value)
{
    uint8_t high[2];
    uint8_t low[2];
    uint8_t status = motor_read(high_reg, high, sizeof(high));

    if (status != 0U) {
        return status;
    }

    status = motor_read((uint8_t) (high_reg + 1U), low, sizeof(low));
    if (status == 0U) {
        uint32_t raw = ((uint32_t) high[0] << 24) |
                       ((uint32_t) high[1] << 16) |
                       ((uint32_t) low[0] << 8) |
                       (uint32_t) low[1];
        *value = (int32_t) raw;
    }

    return status;
}

static uint8_t motor_read_all_encoders(int32_t encoders[4])
{
    for (uint8_t i = 0U; i < 4U; i++) {
        uint8_t status = motor_read_i32_pair((uint8_t) (REG_ENCODER_ALL_HIGH_M1 + (i * 2U)), &encoders[i]);
        if (status != 0U) {
            return status;
        }
    }

    return 0U;
}

static uint8_t motor_read_10ms_encoders(int16_t encoders[4])
{
    for (uint8_t i = 0U; i < 4U; i++) {
        uint8_t status = motor_read_i16((uint8_t) (REG_ENCODER_10MS_M1 + i), &encoders[i]);
        if (status != 0U) {
            return status;
        }
    }

    return 0U;
}

static uint8_t motor_config_tt_encoder(void)
{
    uint8_t status;

    status = motor_set_u8(REG_MOTOR_TYPE, TT_ENCODER_MOTOR_TYPE);
    print_status("set motor type TT encoder", status);
    if (status != 0U) {
        return status;
    }
    delay_cycles(DELAY_100MS_CYCLES);

    status = motor_set_u16(REG_MOTOR_REDUCTION_RATIO, TT_REDUCTION_RATIO);
    print_status("set reduction ratio 45", status);
    if (status != 0U) {
        return status;
    }
    delay_cycles(DELAY_100MS_CYCLES);

    status = motor_set_u16(REG_MOTOR_PULSE_LINE, TT_ENCODER_LINES);
    print_status("set encoder lines 13", status);
    if (status != 0U) {
        return status;
    }
    delay_cycles(DELAY_100MS_CYCLES);

    status = motor_set_float(REG_WHEEL_DIAMETER, TT_WHEEL_DIAMETER_MM);
    print_status("set wheel diameter 68mm", status);
    if (status != 0U) {
        return status;
    }
    delay_cycles(DELAY_100MS_CYCLES);

    status = motor_set_u16(REG_MOTOR_DEADZONE, TT_DEADZONE);
    print_status("set deadzone 1250", status);
    delay_cycles(DELAY_100MS_CYCLES);

    return status;
}

static void print_encoder_line(void)
{
    int32_t total[4] = {0};
    int16_t delta[4] = {0};
    uint8_t total_status = motor_read_all_encoders(total);
    uint8_t delta_status = motor_read_10ms_encoders(delta);

    if ((total_status != 0U) || (delta_status != 0U)) {
        uart_write_str("encoder read error total=");
        uart_write_u32(total_status);
        uart_write_str(" delta=");
        uart_write_u32(delta_status);
        uart_write_str("\r\n");
        return;
    }

    uart_write_str("all M1=");
    uart_write_i32(total[0]);
    uart_write_str(" M2=");
    uart_write_i32(total[1]);
    uart_write_str(" M3=");
    uart_write_i32(total[2]);
    uart_write_str(" M4=");
    uart_write_i32(total[3]);

    uart_write_str(" | 10ms M1=");
    uart_write_i32(delta[0]);
    uart_write_str(" M2=");
    uart_write_i32(delta[1]);
    uart_write_str(" M3=");
    uart_write_i32(delta[2]);
    uart_write_str(" M4=");
    uart_write_i32(delta[3]);
    uart_write_str("\r\n");
}

static int16_t clamp_i16(int32_t value, int16_t min_value, int16_t max_value)
{
    if (value > max_value) {
        return max_value;
    }

    if (value < min_value) {
        return min_value;
    }

    return (int16_t) value;
}

static int16_t apply_motor_sign(int16_t pwm, int8_t sign)
{
    return (sign < 0) ? (int16_t) -pwm : pwm;
}

static uint8_t motor_stop(void)
{
    uint8_t speed_status = motor_speed(0, 0, 0, 0);
    uint8_t pwm_status = motor_pwm(0, 0, 0, 0);

    return (speed_status != 0U) ? speed_status : pwm_status;
}

static uint8_t drive_m2_m4(int16_t left_speed, int16_t right_speed)
{
    return motor_speed(0,
        apply_motor_sign(left_speed, M2_FORWARD_SIGN),
        0,
        apply_motor_sign(right_speed, M4_FORWARD_SIGN));
}

static bool line_should_stop(const line_sample_t *sample)
{
    if (sample->active_count == 0U) {
        return true;
    }

#if LINE_STOP_ON_ALL_BLACK
    if (sample->active_count >= 4U) {
        return true;
    }
#endif

    return false;
}

static line_sample_t read_line_sample(void)
{
    line_sample_t sample = {0};
    int32_t weighted_sum = 0;
    uint32_t pins = DL_GPIO_readPins(GRAY_PORT, GRAY_ALL_PINS);

    sample.position = 2000;

    for (uint8_t i = 0U; i < 5U; i++) {
        sample.raw[i] = ((pins & g_gray_pins[i]) != 0U) ? 1U : 0U;
        sample.line[i] = (sample.raw[i] == LINE_BLACK_LEVEL) ? 1U : 0U;

        if (sample.line[i] != 0U) {
            sample.active_count++;
            sample.mask |= (uint8_t) (1U << i);
            weighted_sum += g_gray_positions[i];
        }
    }

    if (sample.active_count != 0U) {
        sample.position = (int16_t) (weighted_sum / sample.active_count);
        sample.error = sample.position - 2000;
    }

    return sample;
}

static void calculate_line_speed(const line_sample_t *sample, int16_t *left_speed, int16_t *right_speed)
{
    static int16_t last_error = 0;
    int16_t error;
    int16_t correction;
    int32_t proportional;
    int32_t derivative;

    if (line_should_stop(sample)) {
#if LINE_SEARCH_WHEN_LOST
        if (sample->active_count == 0U) {
            if (last_error >= 0) {
                *left_speed = LINE_SEARCH_SPEED;
                *right_speed = -LINE_SEARCH_SPEED;
            } else {
                *left_speed = -LINE_SEARCH_SPEED;
                *right_speed = LINE_SEARCH_SPEED;
            }
        } else {
            *left_speed = 0;
            *right_speed = 0;
        }
#else
        last_error = 0;
        *left_speed = 0;
        *right_speed = 0;
#endif
        return;
    }

    if (sample->active_count >= 4U) {
        error = 0;
    } else {
        error = sample->error;
    }

    proportional = ((int32_t) error * LINE_KP_NUM) / LINE_KP_DEN;
    derivative = ((int32_t) (error - last_error) * LINE_KD_NUM) / LINE_KD_DEN;
    correction = clamp_i16(
        ((int32_t) LINE_TURN_SIGN * (proportional + derivative)),
        -LINE_CORRECTION_MAX,
        LINE_CORRECTION_MAX);
    last_error = error;

    *left_speed = clamp_i16(
        (int32_t) LINE_LEFT_BASE_SPEED + correction,
        0,
        LINE_MAX_SPEED);
    *right_speed = clamp_i16(
        (int32_t) LINE_RIGHT_BASE_SPEED - correction,
        0,
        LINE_MAX_SPEED);
}

static void print_line_sample(const line_sample_t *sample, int16_t left_speed, int16_t right_speed, uint8_t status)
{
    uart_write_str("gray raw=");
    for (uint8_t i = 0U; i < 5U; i++) {
        uart_write_char((char) ('0' + sample->raw[i]));
    }

    uart_write_str(" line=");
    for (uint8_t i = 0U; i < 5U; i++) {
        uart_write_char((char) ('0' + sample->line[i]));
    }

    uart_write_str(" cnt=");
    uart_write_u32(sample->active_count);
    if (sample->active_count == 0U) {
        uart_write_str(" mode=lost");
    } else if (sample->active_count >= 4U) {
        uart_write_str(" mode=all_black");
    } else {
        uart_write_str(" mode=follow");
    }
    uart_write_str(" pos=");
    uart_write_i32(sample->position);
    uart_write_str(" err=");
    uart_write_i32(sample->error);
    uart_write_str(" L=");
    uart_write_i32(left_speed);
    uart_write_str(" R=");
    uart_write_i32(right_speed);

    if (status != 0U) {
        uart_write_str(" motor_error=");
        uart_write_u32(status);
    }

    uart_write_str("\r\n");
}

int main(void)
{
    uint8_t status;
    uint16_t loop_count = 0U;

    SYSCFG_DL_init();
    delay_cycles(DELAY_100MS_CYCLES);
    i2c_idle();

    uart_write_str("\r\nMSPM0 5-channel gray line follower\r\n");
    uart_write_str("Motor IIC: SDA=PA13 SCL=PA12 addr=0x26\r\n");
    uart_write_str("Debug UART: PA10 TX / PA11 RX, 115200 8N1\r\n");
    uart_write_str("Gray GPIO: S1=PA14 S2=PA15 S3=PA16 S4=PA17 S5=PA21, black=0\r\n");
    uart_write_str("Drive mapping: left=M2 right=M4, M2/M4 forward signs are inverted in code\r\n");
    uart_write_str("Motor config: encoder TT, line=13, ratio=45, wheel=68mm, deadzone=1250\r\n");
    uart_write_str("Action: slow PD line follow with encoder speed control. Lift wheels before first test.\r\n");

    status = motor_stop();
    print_status("initial motor stop", status);
    delay_cycles(DELAY_100MS_CYCLES);

    status = motor_config_tt_encoder();
    if (status == 0U) {
        uart_write_str("line follower starts after 3 seconds\r\n");
        delay_cycles(DELAY_1S_CYCLES);
        delay_cycles(DELAY_1S_CYCLES);
        delay_cycles(DELAY_1S_CYCLES);
    } else {
        uart_write_str("motor config failed; check 5V/GND/SDA/SCL wiring before moving wheels\r\n");
        while (1) {
            (void) motor_stop();
            delay_cycles(DELAY_1S_CYCLES);
        }
    }

    while (1) {
        line_sample_t sample = read_line_sample();
        int16_t left_speed = 0;
        int16_t right_speed = 0;

        calculate_line_speed(&sample, &left_speed, &right_speed);
        if (line_should_stop(&sample)) {
            status = motor_stop();
        } else {
            status = drive_m2_m4(left_speed, right_speed);
        }

        if ((loop_count % LINE_PRINT_INTERVAL_LOOPS) == 0U) {
            print_line_sample(&sample, left_speed, right_speed, status);
        }

        if ((loop_count % LINE_ENCODER_INTERVAL_LOOPS) == 0U) {
            print_encoder_line();
        }

        if (status != 0U) {
            (void) motor_stop();
            delay_cycles(DELAY_300MS_CYCLES);
        } else {
            delay_cycles(LINE_LOOP_DELAY_CYCLES);
        }

        loop_count++;
    }
}
