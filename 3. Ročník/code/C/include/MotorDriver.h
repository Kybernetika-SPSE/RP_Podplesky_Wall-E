#pragma once

#include <cstdint>
#include <vector>
#include <mutex>

// PCA9685 Registers
#define PCA9685_MODE1 0x00
#define PCA9685_PRESCALE 0xFE
#define LED0_ON_L 0x06

class MotorDriver {
public:
    MotorDriver(int i2c_bus = 1, int i2c_addr = 0x40);
    ~MotorDriver();

    bool init();
    
    // Set target velocity: -1.0 (Full Reverse) to 1.0 (Full Forward)
    void setTargetVelocity(double linear, double angular);

    // Update loop to ramp values towards targets (call periodically, e.g., 50Hz)
    void update(double dt);

private:
    int i2c_fd;
    int i2c_address;

    // Smoothed/Current Values
    double current_linear = 0.0;
    double current_angular = 0.0;

    // Target Values
    double target_linear = 0.0;
    double target_angular = 0.0;

    // Parameters
    double max_accel_linear = 1.0; // Units/sec^2 (takes 1 sec to reach full speed)
    double max_accel_angular = 2.0; // Faster turning response

    // I2C Helpers
    void writeByte(uint8_t reg, uint8_t data);
    uint8_t readByte(uint8_t reg);
    void setPWM(int channel, uint16_t on, uint16_t off);
    
    // Motor Output Helpers
    void setMotorL(double speed);
    void setMotorR(double speed);
};
