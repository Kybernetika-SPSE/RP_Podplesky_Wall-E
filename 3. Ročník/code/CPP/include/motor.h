#ifndef MOTOR_H
#define MOTOR_H
#include <Arduino.h>
class Motor {
private:
    int pinLPwm;
    int pinRPwm;
    int pwmChannelL;
    int pwmChannelR;
    const int pwmFreq = 5000;
    const int pwmResolution = 8;
public:
    Motor(int pin_l_pwm, int pin_r_pwm, int channel_l, int channel_r);
    void begin();
    void setSpeed(int speed);
    void stop();
};
#endif
