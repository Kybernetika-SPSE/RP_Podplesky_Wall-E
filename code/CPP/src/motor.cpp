#include "motor.h"

Motor::Motor(int pin_l_pwm, int pin_r_pwm, int channel_l, int channel_r) {
    pinLPwm = pin_l_pwm;
    pinRPwm = pin_r_pwm;
    pwmChannelL = channel_l;
    pwmChannelR = channel_r;
}

void Motor::begin() {
    // Setup PWM channels
    // Note: Arduino ESP32 core 3.x changes PWM API, but ledcSetup is standard for 2.x
    // We will use standard ledcSetup which is compatible with most PlatformIO installs
    ledcSetup(pwmChannelL, pwmFreq, pwmResolution);
    ledcSetup(pwmChannelR, pwmFreq, pwmResolution);
    
    // Attach pins to channels
    ledcAttachPin(pinLPwm, pwmChannelL);
    ledcAttachPin(pinRPwm, pwmChannelR);
    
    // Make sure we are stopped initially
    stop();
}

void Motor::setSpeed(int speed) {
    // Constrain speed to -255 ... 255
    if (speed > 255) speed = 255;
    if (speed < -255) speed = -255;
    
    // Deadzone to prevent whining at very low speeds
    if (abs(speed) < 15) {
        stop();
        return;
    }
    
    if (speed > 0) {
        // Drive Forward: L_PWM gets the duty cycle, R_PWM is 0
        ledcWrite(pwmChannelR, 0);
        ledcWrite(pwmChannelL, speed);
    } else {
        // Drive Backward: R_PWM gets the duty cycle, L_PWM is 0
        ledcWrite(pwmChannelL, 0);
        ledcWrite(pwmChannelR, abs(speed));
    }
}

void Motor::stop() {
    ledcWrite(pwmChannelL, 0);
    ledcWrite(pwmChannelR, 0);
}
