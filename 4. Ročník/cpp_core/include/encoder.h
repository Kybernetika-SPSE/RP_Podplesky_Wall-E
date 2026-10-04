#pragma once
#include <atomic>
#include <thread>
#include <string>

class Encoder {
private:
    int pinA;
    int pinB;
    std::string gpioPathA;
    std::string gpioPathB;
    std::atomic<long long> count;
    std::atomic<bool> running;
    std::thread isrThread;

    bool writeSysfs(const std::string& path, const std::string& value);
    std::string readSysfs(const std::string& path);
    void interruptRoutine();

public:
    Encoder(int pin_a, int pin_b);
    ~Encoder();
    
    void begin();
    long long getCount();
    void clearCount();
};
