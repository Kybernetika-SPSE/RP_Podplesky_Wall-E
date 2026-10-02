#include "motor.h"
#include <iostream>
#include <fstream>
#include <cmath>

Motor::Motor(int channel_l, int channel_r) {
    pwmChannelL = channel_l;
    pwmChannelR = channel_r;
    
    // Na Jetsonu se k hardwarovému PWM běžně přistupuje přes sysfs: /sys/class/pwm/pwmchipX
    // Cesty si budete muset případně upravit podle toho, jaké piny na Jetsonu fyzicky použijete
    pwmBasePathL = "/sys/class/pwm/pwmchip0/pwm" + std::to_string(pwmChannelL);
    pwmBasePathR = "/sys/class/pwm/pwmchip0/pwm" + std::to_string(pwmChannelR);
}

bool Motor::writeSysfs(const std::string& path, const std::string& filename, const std::string& value) {
    std::ofstream file(path + "/" + filename);
    if (!file.is_open()) return false;
    file << value;
    return true;
}

void Motor::begin() {
    // Export PWM (pokud už není)
    std::ofstream exportFileL("/sys/class/pwm/pwmchip0/export");
    if(exportFileL.is_open()) exportFileL << pwmChannelL;
    
    std::ofstream exportFileR("/sys/class/pwm/pwmchip0/export");
    if(exportFileR.is_open()) exportFileR << pwmChannelR;

    // Nastavení periody (v nanosekundách) - 20000000 ns = 50 Hz, 1000000 ns = 1kHz
    writeSysfs(pwmBasePathL, "period", "1000000"); 
    writeSysfs(pwmBasePathR, "period", "1000000");

    writeSysfs(pwmBasePathL, "enable", "1");
    writeSysfs(pwmBasePathR, "enable", "1");
    
    stop();
}

void Motor::setSpeed(int speed) {
    if (speed > 255) speed = 255;
    if (speed < -255) speed = -255;
    
    if (std::abs(speed) < 15) {
        stop();
        return;
    }
    
    // Přepočet rychlosti 0-255 na duty_cycle v nanosekundách (0 - 1000000)
    int duty_ns = (std::abs(speed) * 1000000) / 255;
    
    if (speed > 0) {
        writeSysfs(pwmBasePathR, "duty_cycle", "0");
        writeSysfs(pwmBasePathL, "duty_cycle", std::to_string(duty_ns));
    } else {
        writeSysfs(pwmBasePathL, "duty_cycle", "0");
        writeSysfs(pwmBasePathR, "duty_cycle", std::to_string(duty_ns));
    }
}

void Motor::stop() {
    writeSysfs(pwmBasePathL, "duty_cycle", "0");
    writeSysfs(pwmBasePathR, "duty_cycle", "0");
}
