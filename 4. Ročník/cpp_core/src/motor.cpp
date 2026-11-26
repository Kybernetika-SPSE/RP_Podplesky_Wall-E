#include "motor.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>

Motor::Motor(std::string motor_label, std::string lpwm_line, std::string rpwm_line)
    : label(std::move(motor_label)),
      lineNameL(std::move(lpwm_line)),
      lineNameR(std::move(rpwm_line)) {}

Motor::~Motor() {
    shutdown();
}

bool Motor::begin(bool dry_run) {
    shutdown();
    dryRun = dry_run;
    currentSpeed.store(0);

    if (dryRun) {
        std::cout << "[MOTOR " << label << "] DRY-RUN režim (LPWM=" << lineNameL
                  << ", RPWM=" << lineNameR << ", SW PWM " << SW_PWM_FREQ_HZ << " Hz).\n";
        return true;
    }

    const bool okL = gpioL.open(lineNameL, "walle_" + label + "_lpwm", 0);
    const bool okR = gpioR.open(lineNameR, "walle_" + label + "_rpwm", 0);
    if (!okL || !okR) {
        std::cerr << "[MOTOR " << label << "] CHYBA: Nepodařilo se otevřít GPIO linky "
                  << lineNameL << " / " << lineNameR
                  << ". Motor zůstane vypnutý.\n";
        gpioL.closeLine();
        gpioR.closeLine();
        return false;
    }

    running.store(true);
    pwmThread = std::thread(&Motor::pwmLoop, this);
    std::cout << "[MOTOR " << label << "] SW PWM aktivní (" << SW_PWM_FREQ_HZ
              << " Hz, LPWM=" << lineNameL << ", RPWM=" << lineNameR << ").\n";
    return true;
}

void Motor::setSpeed(int speed) {
    const int clamped = std::clamp(speed, -PWM_MAX, PWM_MAX);
    if (std::abs(clamped) < DEADBAND) {
        currentSpeed.store(0);
    } else {
        currentSpeed.store(clamped);
    }
}

void Motor::stop() {
    currentSpeed.store(0);
}

void Motor::shutdown() {
    currentSpeed.store(0);
    running.store(false);
    if (pwmThread.joinable()) {
        pwmThread.join();
    }
    gpioL.closeLine();
    gpioR.closeLine();
}

void Motor::pwmLoop() {
    using clock = std::chrono::steady_clock;
    constexpr auto period = std::chrono::microseconds(SW_PWM_PERIOD_US);

    gpioL.setValue(0);
    gpioR.setValue(0);

    while (running.load()) {
        const int speed = currentSpeed.load();
        const auto cycleStart = clock::now();

        if (speed == 0) {
            gpioL.setValue(0);
            gpioR.setValue(0);
            std::this_thread::sleep_until(cycleStart + period);
            continue;
        }

        const bool forward = speed > 0;
        const int mag = std::abs(speed);
        GpioOutputLine& active = forward ? gpioL : gpioR;
        GpioOutputLine& inactive = forward ? gpioR : gpioL;

        // Bezpečnost BTS7960: neaktivní kanál vždy nejprve stáhnout na 0
        inactive.setValue(0);

        if (mag >= PWM_MAX) {
            active.setValue(1);
            std::this_thread::sleep_until(cycleStart + period);
            continue;
        }

        const int onUs = (mag * SW_PWM_PERIOD_US) / PWM_MAX;
        if (onUs > 0) {
            active.setValue(1);
            std::this_thread::sleep_until(cycleStart + std::chrono::microseconds(onUs));
        }

        active.setValue(0);
        std::this_thread::sleep_until(cycleStart + period);
    }

    gpioL.setValue(0);
    gpioR.setValue(0);
}
