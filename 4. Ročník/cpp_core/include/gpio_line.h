#pragma once

#include <cstdint>
#include <string>

// Modern Linux /dev/gpiochip* wrapper using kernel <linux/gpio.h> ioctls.
// Works natively on JetPack 6 (kernel 5.15+) without external libgpiod dependency.

struct GpioLineLocation {
    std::string chip_path;
    std::uint32_t offset = 0;
    bool found = false;
};

GpioLineLocation findGpioLineByName(const std::string& line_name);

class GpioOutputLine {
private:
    int line_fd = -1;
    std::string name;

public:
    GpioOutputLine() = default;
    ~GpioOutputLine();

    GpioOutputLine(const GpioOutputLine&) = delete;
    GpioOutputLine& operator=(const GpioOutputLine&) = delete;

    bool open(const std::string& line_name, const std::string& consumer, int initial_value = 0);
    bool setValue(int value);
    void closeLine();
    bool isOpen() const { return line_fd >= 0; }
    const std::string& getName() const { return name; }
};

class GpioInputLine {
private:
    int line_fd = -1;
    std::string name;

public:
    GpioInputLine() = default;
    ~GpioInputLine();

    GpioInputLine(const GpioInputLine&) = delete;
    GpioInputLine& operator=(const GpioInputLine&) = delete;

    bool open(const std::string& line_name, const std::string& consumer);
    int getValue() const;
    void closeLine();
    bool isOpen() const { return line_fd >= 0; }
};

class GpioEdgeEventLine {
private:
    int event_fd = -1;
    std::string name;

public:
    GpioEdgeEventLine() = default;
    ~GpioEdgeEventLine();

    GpioEdgeEventLine(const GpioEdgeEventLine&) = delete;
    GpioEdgeEventLine& operator=(const GpioEdgeEventLine&) = delete;

    bool openBothEdges(const std::string& line_name, const std::string& consumer);
    int getFd() const { return event_fd; }
    void closeLine();
    bool isOpen() const { return event_fd >= 0; }
};
