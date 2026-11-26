#pragma once

#include <atomic>
#include <string>
#include <thread>

#include "gpio_line.h"

class Motor {
public:
    static constexpr int PWM_MAX = 255;
    static constexpr int DEADBAND = 15;
    static constexpr int SW_PWM_FREQ_HZ = 250;                  // 250 Hz (4000 us period)
    static constexpr int SW_PWM_PERIOD_US = 1000000 / SW_PWM_FREQ_HZ;

private:
    std::string label;
    std::string lineNameL;
    std::string lineNameR;
    bool dryRun = false;

    GpioOutputLine gpioL;
    GpioOutputLine gpioR;

    std::atomic<int> currentSpeed{0};  // -255..+255
    std::atomic<bool> running{false};
    std::thread pwmThread;

    void pwmLoop();

public:
    Motor(std::string motor_label, std::string lpwm_line, std::string rpwm_line);
    ~Motor();

    Motor(const Motor&) = delete;
    Motor& operator=(const Motor&) = delete;

    bool begin(bool dry_run = false);
    void setSpeed(int speed);  // -255 to 255
    int getSpeed() const { return currentSpeed.load(); }
    void stop();
    void shutdown();
};
