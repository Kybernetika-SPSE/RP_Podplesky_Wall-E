#include <Arduino.h>
#include <Bluepad32.h>
#include "motor.h"
#include "encoder.h"

// ==========================================
// PIN DEFINITIONS (Based on KiCad Schematic)
// ==========================================

// LEFT DRIVER
#define PIN_L_PWM_LEFT 4
#define PIN_R_PWM_LEFT 5

// RIGHT DRIVER
#define PIN_L_PWM_RIGHT 8
#define PIN_R_PWM_RIGHT 9

// LEFT ENCODER
#define PIN_ENC_A_LEFT 16
#define PIN_ENC_B_LEFT 17

// RIGHT ENCODER
#define PIN_ENC_A_RIGHT 10
#define PIN_ENC_B_RIGHT 11

// ==========================================
// OBJECT INITIALIZATION
// ==========================================

// Motors: pins, pwm channel L, pwm channel R
Motor motorLeft(PIN_L_PWM_LEFT, PIN_R_PWM_LEFT, 0, 1);
Motor motorRight(PIN_L_PWM_RIGHT, PIN_R_PWM_RIGHT, 2, 3);

// Encoders
Encoder encoderLeft(PIN_ENC_A_LEFT, PIN_ENC_B_LEFT);
Encoder encoderRight(PIN_ENC_A_RIGHT, PIN_ENC_B_RIGHT);

// Gamepad pointer
ControllerPtr myController = nullptr;

// ==========================================
// BLUEPAD32 CALLBACKS
// ==========================================

void onConnectedController(ControllerPtr ctl) {
    if (myController == nullptr) {
        Serial.println("KONTROLER PRIPOJEN!");
        myController = ctl;
        
        // You can query the controller properties here
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
        
        // Zastavit motory z bezpečnostních důvodů!
        motorLeft.stop();
        motorRight.stop();
    }
}

// ==========================================
// SETUP & LOOP
// ==========================================

void setup() {
    Serial.begin(115200);
    Serial.println("Startuji WALL-E Firmware...");

    // Inicializace motorů a enkodérů
    motorLeft.begin();
    motorRight.begin();
    encoderLeft.begin();
    encoderRight.begin();

    // Inicializace Bluepad32
    BP32.setup(&onConnectedController, &onDisconnectedController);
    
    // Forget previously paired controllers (optional)
    // BP32.forgetBluetoothKeys();

    Serial.println("Cekam na pripojeni DualShocku (zmackni PS button)...");
}

void processGamepad() {
    if (myController && myController->isConnected()) {
        
        // DualShock joystick hodnoty jsou od -511 do 512
        // Chceme je namapovat na rychlost -255 az 255 pro nase motory
        
        // Tank Drive ovladani: 
        // Leva packa = Levy pas, Prava packa = Pravy pas
        int leftJoyY = myController->axisY(); 
        int rightJoyY = myController->axisRY();
        
        // Osa Y na ovladaci: nahoru je zaporne cislo (-511), dolu je kladne (512).
        // My chceme dopredu = kladne, dozadu = zaporne.
        int speedLeft = map(leftJoyY, -511, 512, 255, -255);
        int speedRight = map(rightJoyY, -511, 512, 255, -255);
        
        motorLeft.setSpeed(speedLeft);
        motorRight.setSpeed(speedRight);

        // Vypis odometrie a rychlosti kazdych 500ms
        static unsigned long lastPrint = 0;
        if (millis() - lastPrint > 500) {
            Serial.printf("Rychlost L: %d | Rychlost R: %d\n", speedLeft, speedRight);
            Serial.printf("Enkoder L: %lld | Enkoder R: %lld\n", encoderLeft.getCount(), encoderRight.getCount());
            lastPrint = millis();
        }
    }
}

void loop() {
    // Toto se musi volat neustale
    BP32.update();
    
    // Zpracovani vstupu a rizeni motoru
    processGamepad();
    
    delay(10); // Mala pauza pro uvolneni CPU
}
