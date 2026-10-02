#include "encoder.h"
Encoder::Encoder(int pin_a, int pin_b) {
    pinA = pin_a;
    pinB = pin_b;
}
void Encoder::begin() {
    ESP32Encoder::useInternalWeakPullResistors = puType::up;
    encoder.attachHalfQuad(pinA, pinB);
    encoder.clearCount();
}
int64_t Encoder::getCount() {
    return encoder.getCount();
}
void Encoder::clearCount() {
    encoder.clearCount();
}
