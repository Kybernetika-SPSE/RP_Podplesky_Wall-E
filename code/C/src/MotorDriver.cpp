#include "MotorDriver.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/i2c-dev.h>

// Clamp helper
template <typename T>
T clamp(T val, T min, T max) {
    if (val < min) return min;
    if (val > max) return max;
    return val;
}

MotorDriver::MotorDriver(int i2c_bus, int i2c_addr) : i2c_address(i2c_addr) {
    char filename[20];
    snprintf(filename, 19, "/dev/i2c-%d", i2c_bus);
    i2c_fd = open(filename, O_RDWR);
    if (i2c_fd < 0) {
        std::cerr << "Error opening I2C bus: " << filename << std::endl;
    }
}

MotorDriver::~MotorDriver() {
    if (i2c_fd >= 0) {
        setTargetVelocity(0, 0);
        update(0.1); // Force stop
        close(i2c_fd);
    }
}

bool MotorDriver::init() {
    if (i2c_fd < 0) return false;

    if (ioctl(i2c_fd, I2C_SLAVE, i2c_address) < 0) {
        std::cerr << "Error selecting I2C device" << std::endl;
        return false;
    }

    // Reset PCA9685
    writeByte(PCA9685_MODE1, 0x00);
    
    // Set PWM frequency to ~50Hz (Standard analog servo / motor driver freq)
    // Formula: prescale = round(osc_clock / (4096 * update_rate)) - 1
    // osc_clock = 25MHz
    // 25000000 / (4096 * 50) - 1 = ~121
    uint8_t prescale = 121;
    
    uint8_t oldmode = readByte(PCA9685_MODE1);
    uint8_t newmode = (oldmode & 0x7F) | 0x10; // Sleep
    writeByte(PCA9685_MODE1, newmode);
    writeByte(PCA9685_PRESCALE, prescale);
    writeByte(PCA9685_MODE1, oldmode);
    usleep(5000);
    writeByte(PCA9685_MODE1, oldmode | 0xA1); // Auto-increment on

    return true;
}

void MotorDriver::setTargetVelocity(double linear, double angular) {
    target_linear = clamp(linear, -1.0, 1.0);
    target_angular = clamp(angular, -1.0, 1.0);
}

void MotorDriver::update(double dt) {
    // Ramping Linear
    double diff_lin = target_linear - current_linear;
    double max_delta_lin = max_accel_linear * dt;
    current_linear += clamp(diff_lin, -max_delta_lin, max_delta_lin);

    // Ramping Angular
    double diff_ang = target_angular - current_angular;
    double max_delta_ang = max_accel_angular * dt;
    current_angular += clamp(diff_ang, -max_delta_ang, max_delta_ang);

    // Differential Drive Mixing
    // Left = Linear - Angular
    // Right = Linear + Angular
    // (Signs depend on motor orientation, adjust here if needed)
    double left_speed = current_linear - current_angular;
    double right_speed = current_linear + current_angular;

    // Normalize if exceeding 1.0 to preserve ratio (optional, but good for turning at high speed)
    double max_mag = std::max(std::abs(left_speed), std::abs(right_speed));
    if (max_mag > 1.0) {
        left_speed /= max_mag;
        right_speed /= max_mag;
    }

    setMotorL(left_speed);
    setMotorR(right_speed);
}

// Low-level PCA9685 PWM sets
// Channels assumed from schematic:
// CH0: L_FWD, CH1: L_BWD
// CH2: R_FWD, CH3: R_BWD
void MotorDriver::setMotorL(double speed) {
    uint16_t pwm_val = (uint16_t)(std::abs(speed) * 4095.0);
    if (speed > 0) {
        setPWM(0, 0, pwm_val); // L_FWD
        setPWM(1, 0, 0);       // L_BWD OFF
    } else {
        setPWM(0, 0, 0);       // L_FWD OFF
        setPWM(1, 0, pwm_val); // L_BWD
    }
}

void MotorDriver::setMotorR(double speed) {
    uint16_t pwm_val = (uint16_t)(std::abs(speed) * 4095.0);
    if (speed > 0) {
        setPWM(2, 0, pwm_val); // R_FWD
        setPWM(3, 0, 0);       // R_BWD OFF
    } else {
        setPWM(2, 0, 0);       // R_FWD OFF
        setPWM(3, 0, pwm_val); // R_BWD
    }
}

void MotorDriver::writeByte(uint8_t reg, uint8_t data) {
    uint8_t buf[2] = {reg, data};
    write(i2c_fd, buf, 2);
}

uint8_t MotorDriver::readByte(uint8_t reg) {
    if (write(i2c_fd, &reg, 1) != 1) return 0;
    uint8_t data;
    if (read(i2c_fd, &data, 1) != 1) return 0;
    return data;
}

void MotorDriver::setPWM(int channel, uint16_t on, uint16_t off) {
    uint8_t reg = LED0_ON_L + 4 * channel;
    uint8_t data[5];
    data[0] = reg;
    data[1] = on & 0xFF;
    data[2] = on >> 8;
    data[3] = off & 0xFF;
    data[4] = off >> 8;
    write(i2c_fd, data, 5);
}
