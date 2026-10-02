#include <Arduino.h>
#include <Bluepad32.h>
#include "motor.h"
#include "encoder.h"
#define PIN_ENC_A_LEFT 10
#define PIN_ENC_B_LEFT 11
#define PIN_ENC_A_RIGHT 6
#define PIN_ENC_B_RIGHT 7
Motor motorLeft(8, 9, 0, 1);
Motor motorRight(4, 5, 2, 3);
Encoder encoderLeft(PIN_ENC_A_LEFT, PIN_ENC_B_LEFT);
Encoder encoderRight(PIN_ENC_A_RIGHT, PIN_ENC_B_RIGHT);
ControllerPtr myController = nullptr;
void onConnectedController(ControllerPtr ctl) {
    if (myController == nullptr) {
        Serial.println("KONTROLER PRIPOJEN!");
        myController = ctl;
        ControllerProperties properties = ctl->getProperties();
        Serial.printf("MAC adresa: %s\n", properties.btaddr);
    } else {
        Serial.println("Jiny kontroler pripojen, ale ignoruji ho (mame uz jeden).");
    }
}
void onDisconnectedController(ControllerPtr ctl) {
    if (myController == ctl) {
        Serial.println("KONTROLER ODPOJEN!");
        myController = nullptr;
        motorLeft.stop();
        motorRight.stop();
    }
}
const int BUZZER_PIN = 12;
int targetSpeedLeft = 0;
int targetSpeedRight = 0;
int currentSpeedLeft = 0;
int currentSpeedRight = 0;
const unsigned long ACCEL_DELAY_MS = 20;
void setup() {
    Serial.begin(115200);
    delay(3000);
    Serial.println("Startuji WALL-E Firmware...");
    motorLeft.begin();
    motorRight.begin();
    encoderLeft.begin();
    encoderRight.begin();
    BP32.setup(&onConnectedController, &onDisconnectedController);
    BP32.forgetBluetoothKeys();
    Serial.println("Cekam na pripojeni DualShocku (zmackni PS button)...");
}
void processKeyboard() {
    if (Serial.available() > 0) {
        char cmd = Serial.read();
        if (cmd == 'q' || cmd == 'Q') {
            Serial.println("LEVY PAS -> CIL DOPREDU!");
            targetSpeedLeft = 255;
        }
        else if (cmd == 'a' || cmd == 'A') {
            Serial.println("LEVY PAS -> CIL DOZADU!");
            targetSpeedLeft = -255;
        }
        else if (cmd == 'e' || cmd == 'E') {
            Serial.println("PRAVY PAS -> CIL DOPREDU!");
            targetSpeedRight = 255;
        }
        else if (cmd == 'd' || cmd == 'D') {
            Serial.println("PRAVY PAS -> CIL DOZADU!");
            targetSpeedRight = -255;
        }
        else if (cmd == ' ' || cmd == 's' || cmd == 'S') {
            Serial.println("NOUZOVY STOP OBA PASY!");
            targetSpeedLeft = 0;
            targetSpeedRight = 0;
            currentSpeedLeft = 0;
            currentSpeedRight = 0;
            motorLeft.stop();
            motorRight.stop();
        }
    }
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint > 500) {
        Serial.printf("Rychlost L:%4d R:%4d | Enkoder L: %lld | R: %lld\n",
            currentSpeedLeft, currentSpeedRight, encoderLeft.getCount(), encoderRight.getCount());
        lastPrint = millis();
    }
}
void updateMotors() {
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate >= ACCEL_DELAY_MS) {
        if (currentSpeedLeft < targetSpeedLeft) {
            currentSpeedLeft++;
        } else if (currentSpeedLeft > targetSpeedLeft) {
            currentSpeedLeft--;
        }
        if (currentSpeedRight < targetSpeedRight) {
            currentSpeedRight++;
        } else if (currentSpeedRight > targetSpeedRight) {
            currentSpeedRight--;
        }
        motorLeft.setSpeed(currentSpeedLeft);
        motorRight.setSpeed(currentSpeedRight);
        lastUpdate = millis();
    }
}
void loop() {
    processKeyboard();
    updateMotors();
    delay(5);
}
