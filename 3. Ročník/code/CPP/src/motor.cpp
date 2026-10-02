#include "motor.h"
Motor::Motor(int pin_l_pwm, int pin_r_pwm, int channel_l, int channel_r) {
    pinLPwm = pin_l_pwm;
    pinRPwm = pin_r_pwm;
    pwmChannelL = channel_l;
    pwmChannelR = channel_r;
}
void Motor::begin() {
    ledcSetup(pwmChannelL, pwmFreq, pwmResolution);
    ledcSetup(pwmChannelR, pwmFreq, pwmResolution);
    ledcAttachPin(pinLPwm, pwmChannelL);
    ledcAttachPin(pinRPwm, pwmChannelR);
    stop();
}
void Motor::setSpeed(int speed) {
    if (speed > 255) speed = 255;
    if (speed < -255) speed = -255;
    if (abs(speed) < 15) {
        stop();
        return;
    }
    if (speed > 0) {
        ledcWrite(pwmChannelR, 0);
        ledcWrite(pwmChannelL, speed);
    } else {
        ledcWrite(pwmChannelL, 0);
        ledcWrite(pwmChannelR, abs(speed));
    }
}
void Motor::stop() {
    ledcWrite(pwmChannelL, 0);
    ledcWrite(pwmChannelR, 0);
}
