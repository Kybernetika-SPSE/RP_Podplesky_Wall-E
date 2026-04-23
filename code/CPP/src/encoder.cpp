#include "encoder.h"

Encoder::Encoder(int pin_a, int pin_b) {
    pinA = pin_a;
    pinB = pin_b;
}

void Encoder::begin() {
    // ESP32Encoder requires interrupt enabling 
    // This is usually done globally once, but library handles it safely
    ESP32Encoder::useInternalWeakPullResistors = DOWN;
    
    // Attach pins for quadrature reading
    encoder.attachHalfQuad(pinA, pinB);
    encoder.clearCount();
}

int64_t Encoder::getCount() {
    return encoder.getCount();
}

void Encoder::clearCount() {
    encoder.clearCount();
}
