#ifndef ENCODER_H
#define ENCODER_H

#include <Arduino.h>
#include <ESP32Encoder.h>

class Encoder {
private:
    ESP32Encoder encoder;
    int pinA;
    int pinB;

public:
    Encoder(int pin_a, int pin_b);
    void begin();
    
    // Get current tick count
    int64_t getCount();
    
    // Reset tick count to 0
    void clearCount();
};

#endif // ENCODER_H
