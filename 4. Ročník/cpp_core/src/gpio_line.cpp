#include "gpio_line.h"

#include <cstring>
#include <filesystem>
#include <iostream>
#include <vector>

#include <fcntl.h>
#include <linux/gpio.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace fs = std::filesystem;

GpioLineLocation findGpioLineByName(const std::string& line_name) {
    GpioLineLocation result;
    std::vector<std::string> chips;

    std::error_code ec;
    if (fs::exists("/dev", ec)) {
        for (const auto& entry : fs::directory_iterator("/dev", ec)) {
            const std::string fname = entry.path().filename().string();
            if (fname.rfind("gpiochip", 0) == 0) {
                chips.push_back(entry.path().string());
            }
        }
    }

    for (const auto& chip_path : chips) {
        const int chip_fd = ::open(chip_path.c_str(), O_RDONLY);
        if (chip_fd < 0) continue;

        struct gpiochip_info chip_info {};
        if (::ioctl(chip_fd, GPIO_GET_CHIPINFO_IOCTL, &chip_info) == 0) {
            for (std::uint32_t offset = 0; offset < chip_info.lines; ++offset) {
                struct gpioline_info line_info {};
                line_info.line_offset = offset;
                if (::ioctl(chip_fd, GPIO_GET_LINEINFO_IOCTL, &line_info) == 0) {
                    if (line_name == line_info.name) {
                        result.chip_path = chip_path;
                        result.offset = offset;
                        result.found = true;
                        ::close(chip_fd);
                        return result;
                    }
                }
            }
        }
        ::close(chip_fd);
    }

    return result;
}

GpioOutputLine::~GpioOutputLine() {
    closeLine();
}

bool GpioOutputLine::open(const std::string& line_name,
                          const std::string& consumer,
                          int initial_value) {
    closeLine();
    name = line_name;

    const GpioLineLocation loc = findGpioLineByName(line_name);
    if (!loc.found) {
        std::cerr << "[GPIO] Linka '" << line_name
                  << "' nebyla nalezena na žádném /dev/gpiochip*.\n";
        return false;
    }

    const int chip_fd = ::open(loc.chip_path.c_str(), O_RDONLY);
    if (chip_fd < 0) {
        std::cerr << "[GPIO] Nelze otevřít " << loc.chip_path << " (zkontroluj oprávnění / skupinu gpio).\n";
        return false;
    }

    struct gpiohandle_request req {};
    req.lineoffsets[0] = loc.offset;
    req.lines = 1;
    req.flags = GPIOHANDLE_REQUEST_OUTPUT;
    req.default_values[0] = initial_value ? 1 : 0;
    std::strncpy(req.consumer_label, consumer.c_str(), sizeof(req.consumer_label) - 1);

    const int ret = ::ioctl(chip_fd, GPIO_GET_LINEHANDLE_IOCTL, &req);
    ::close(chip_fd);

    if (ret < 0 || req.fd < 0) {
        std::cerr << "[GPIO] Nelze alokovat výstupní linku '" << line_name
                  << "' (" << loc.chip_path << " offset " << loc.offset << ").\n";
        return false;
    }

    line_fd = req.fd;
    return true;
}

bool GpioOutputLine::setValue(int value) {
    if (line_fd < 0) return false;
    struct gpiohandle_data data {};
    data.values[0] = value ? 1 : 0;
    return ::ioctl(line_fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data) == 0;
}

void GpioOutputLine::closeLine() {
    if (line_fd >= 0) {
        setValue(0);
        ::close(line_fd);
        line_fd = -1;
    }
}

GpioInputLine::~GpioInputLine() {
    closeLine();
}

bool GpioInputLine::open(const std::string& line_name, const std::string& consumer) {
    closeLine();
    name = line_name;

    const GpioLineLocation loc = findGpioLineByName(line_name);
    if (!loc.found) {
        std::cerr << "[GPIO] Vstupní linka '" << line_name << "' nenalezena.\n";
        return false;
    }

    const int chip_fd = ::open(loc.chip_path.c_str(), O_RDONLY);
    if (chip_fd < 0) return false;

    struct gpiohandle_request req {};
    req.lineoffsets[0] = loc.offset;
    req.lines = 1;
    req.flags = GPIOHANDLE_REQUEST_INPUT;
    std::strncpy(req.consumer_label, consumer.c_str(), sizeof(req.consumer_label) - 1);

    const int ret = ::ioctl(chip_fd, GPIO_GET_LINEHANDLE_IOCTL, &req);
    ::close(chip_fd);

    if (ret < 0 || req.fd < 0) {
        std::cerr << "[GPIO] Nelze alokovat vstupní linku '" << line_name << "'.\n";
        return false;
    }

    line_fd = req.fd;
    return true;
}

int GpioInputLine::getValue() const {
    if (line_fd < 0) return -1;
    struct gpiohandle_data data {};
    if (::ioctl(line_fd, GPIOHANDLE_GET_LINE_VALUES_IOCTL, &data) < 0) return -1;
    return data.values[0] ? 1 : 0;
}

void GpioInputLine::closeLine() {
    if (line_fd >= 0) {
        ::close(line_fd);
        line_fd = -1;
    }
}

GpioEdgeEventLine::~GpioEdgeEventLine() {
    closeLine();
}

bool GpioEdgeEventLine::openBothEdges(const std::string& line_name,
                                      const std::string& consumer) {
    closeLine();
    name = line_name;

    const GpioLineLocation loc = findGpioLineByName(line_name);
    if (!loc.found) {
        std::cerr << "[GPIO] Přerušovací linka '" << line_name << "' nenalezena.\n";
        return false;
    }

    const int chip_fd = ::open(loc.chip_path.c_str(), O_RDONLY);
    if (chip_fd < 0) return false;

    struct gpioevent_request req {};
    req.lineoffset = loc.offset;
    req.handleflags = GPIOHANDLE_REQUEST_INPUT;
    req.eventflags = GPIOEVENT_REQUEST_BOTH_EDGES;
    std::strncpy(req.consumer_label, consumer.c_str(), sizeof(req.consumer_label) - 1);

    const int ret = ::ioctl(chip_fd, GPIO_GET_LINEEVENT_IOCTL, &req);
    ::close(chip_fd);

    if (ret < 0 || req.fd < 0) {
        std::cerr << "[GPIO] Nelze alokovat přerušení na lince '" << line_name << "'.\n";
        return false;
    }

    event_fd = req.fd;
    return true;
}

void GpioEdgeEventLine::closeLine() {
    if (event_fd >= 0) {
        ::close(event_fd);
        event_fd = -1;
    }
}
