#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <functional>

class InputHandler {
public:
    InputHandler(const std::string& device_path);
    ~InputHandler();

    bool init();
    void start();
    void stop();

    // Callbacks/Polling accessors
    // We can expose current state directly
    bool isLeftPressed() const { return left_btn_pressed; }
    bool isRightPressed() const { return right_btn_pressed; }
    
    // Returns accumulated scroll delta since last call, then resets
    int getScrollDelta();

private:
    std::string device_path;
    std::atomic<bool> running;
    std::thread input_thread; // Reads /dev/input/eventX

    std::atomic<bool> left_btn_pressed;
    std::atomic<bool> right_btn_pressed;
    std::atomic<int> scroll_accumulator;

    void inputLoop();
};
