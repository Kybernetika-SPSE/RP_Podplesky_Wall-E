#pragma once

#include <atomic>
#include <string>
#include <thread>

#include "gpio_line.h"

class Encoder {
private:
    std::string label;
    std::string lineNameA;
    std::string lineNameB;

    GpioEdgeEventLine gpioEventA;
    GpioInputLine gpioInputB;

    std::atomic<long long> count{0};
    std::atomic<bool> running{false};
    std::thread isrThread;

    void interruptRoutine();

public:
    Encoder(std::string enc_label, std::string line_a, std::string line_b);
    ~Encoder();

    Encoder(const Encoder&) = delete;
    Encoder& operator=(const Encoder&) = delete;

    bool begin(bool dry_run = false);
    void shutdown();
    long long getCount() const;
    void clearCount();
};
