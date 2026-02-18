#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include "MotorDriver.h"
#include "InputHandler.h"

// Global flag for clean exit
volatile sig_atomic_t stop_signal = 0;

void signal_handler(int signum) {
    stop_signal = 1;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " /dev/input/eventX" << std::endl;
        return 1;
    }

    std::string input_device_path = argv[1];
    
    // Initialize components
    MotorDriver motors(1, 0x40); // I2C bus 1, Address 0x40
    InputHandler input(input_device_path);

    if (!motors.init()) {
        std::cerr << "Failed to init motors!" << std::endl;
        return 1;
    }

    if (!input.init()) {
        std::cerr << "Failed to init input device (check permissions/path)!" << std::endl;
        return 1;
    }

    // Capture Ctrl+C
    std::signal(SIGINT, signal_handler);

    std::cout << "Starting WALL-E Control..." << std::endl;
    input.start();

    // Control Loop parameters
    double target_turn = 0.0;
    const double TURN_DECAY = 0.1; // How fast turning stops when scrolling stops
    const double SCROLL_SENSITIVITY = 0.2; // How much one scroll notch adds to turn speed

    auto last_time = std::chrono::steady_clock::now();

    while (!stop_signal) {
        auto now = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsed = now - last_time;
        double dt = elapsed.count();
        last_time = now;

        // 1. Read Input
        double fwd_cmd = 0.0;
        if (input.isLeftPressed()) fwd_cmd += 1.0;
        if (input.isRightPressed()) fwd_cmd -= 1.0;

        // Scroll logic for turning
        // Strategy: Scroll events add to target_turn, but it decays to 0 over time
        int scroll_delta = input.getScrollDelta();
        
        // "kolečkem nahoru (positive?) otáčení doleva (positive angular?)"
        // Usually Left Turn = Positive Angular Velocity (CCW)
        // Check mouse polarity: Pulling wheel towards user (Down) is usually -1. Pushing away (Up) is +1.
        // User wants: Up -> Left Turn (+), Down -> Right Turn (-)
        if (scroll_delta != 0) {
            target_turn += scroll_delta * SCROLL_SENSITIVITY;
            // Cap turn speed
            if (target_turn > 1.0) target_turn = 1.0;
            if (target_turn < -1.0) target_turn = -1.0;
        } else {
            // Decay back to 0 if not scrolling
            if (target_turn > 0) {
                target_turn -= TURN_DECAY;
                if (target_turn < 0) target_turn = 0;
            } else if (target_turn < 0) {
                target_turn += TURN_DECAY;
                if (target_turn > 0) target_turn = 0;
            }
        }

        // 2. Set Targets
        motors.setTargetVelocity(fwd_cmd, target_turn);

        // 3. Update Motors (Ramping)
        motors.update(dt);

        // Loop Frequency ~50Hz
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    std::cout << "Stopping..." << std::endl;
    input.stop();
    // Motors destructor will stop motors
    
    return 0;
}
