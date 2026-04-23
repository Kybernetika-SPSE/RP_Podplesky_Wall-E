#ifndef MOTOR_H
#define MOTOR_H

#include <Arduino.h>

class Motor {
private:
    int pinLPwm;
    int pinRPwm;
    int pwmChannelL;
    int pwmChannelR;
    
    // PWM properties for ESP32
    const int pwmFreq = 5000;
    const int pwmResolution = 8; // 8-bit resolution (0-255)

public:
    Motor(int pin_l_pwm, int pin_r_pwm, int channel_l, int channel_r);
    void begin();
    
    // Set speed from -255 to 255
    // Positive values drive forward, negative drive backward
    void setSpeed(int speed);
    
    // Stop the motor immediately
    void stop();
};

#endif // MOTOR_H
