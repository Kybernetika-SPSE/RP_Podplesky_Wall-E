#include "InputHandler.h"
#include <fcntl.h>
#include <unistd.h>
#include <libevdev/libevdev.h>
#include <iostream>

InputHandler::InputHandler(const std::string& path) 
    : device_path(path), running(false), left_btn_pressed(false), right_btn_pressed(false), scroll_accumulator(0) {}

InputHandler::~InputHandler() {
    stop();
}

bool InputHandler::init() {
    // Simple check if file exists and is readable
    int fd = open(device_path.c_str(), O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        std::cerr << "Failed to open input device: " << device_path << std::endl;
        return false;
    }
    close(fd);
    return true;
}

void InputHandler::start() {
    running = true;
    input_thread = std::thread(&InputHandler::inputLoop, this);
}

void InputHandler::stop() {
    running = false;
    if (input_thread.joinable()) {
        input_thread.join();
    }
}

int InputHandler::getScrollDelta() {
    return scroll_accumulator.exchange(0);
}

void InputHandler::inputLoop() {
    struct libevdev *dev = NULL;
    int fd;
    int rc = 1;

    fd = open(device_path.c_str(), O_RDONLY | O_NONBLOCK);
    rc = libevdev_new_from_fd(fd, &dev);
    
    if (rc < 0) {
        std::cerr << "Failed to init libevdev" << std::endl;
        return;
    }

    std::cout << "Input device: " << libevdev_get_name(dev) << std::endl;

    while (running) {
        struct input_event ev;
        rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
        
        if (rc == 0) {
            if (ev.type == EV_KEY) {
                if (ev.code == BTN_LEFT) {
                    left_btn_pressed = (ev.value == 1 || ev.value == 2); // 1=Press, 2=Repeat, 0=Release
                } else if (ev.code == BTN_RIGHT) {
                    right_btn_pressed = (ev.value == 1 || ev.value == 2);
                }
            } else if (ev.type == EV_REL && ev.code == REL_WHEEL) {
                // Scroll Up usually gives positive value, Down negative
                scroll_accumulator += ev.value;
            }
        } else if (rc == -EAGAIN) {
            // No events, sleep a tiny bit to save CPU
            usleep(1000); // 1ms
        }
    }

    libevdev_free(dev);
    close(fd);
}
