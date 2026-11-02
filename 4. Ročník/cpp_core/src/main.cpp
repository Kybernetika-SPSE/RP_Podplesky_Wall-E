#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <hidapi/hidapi.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "motor.h"

namespace {

enum class InputMode { None, Mouse, DualSense };

struct InputDevice {
    std::string name;
    std::string event_path;
    unsigned int vendor = 0;
    unsigned int product = 0;
};

struct OpenInputDevice {
    int fd = -1;
    std::string path;
};

std::atomic<float> left_pressure{0.0f};
std::atomic<float> right_pressure{0.0f};
std::atomic<int> direction_mode{1};
std::atomic<float> wheel_speed{0.0f};
std::atomic<std::chrono::steady_clock::time_point> last_wheel_time;
std::atomic<float> left_stick{0.0f};
std::atomic<float> right_stick{0.0f};
std::atomic<float> left_trigger{0.0f};
std::atomic<float> right_trigger{0.0f};
std::atomic<bool> dpad_up_pressed{false};
std::atomic<bool> dpad_down_pressed{false};
std::atomic<bool> create_pressed{false};
std::atomic<bool> options_pressed{false};
std::atomic<bool> stop_chord_active{false};
std::atomic<InputMode> input_mode{InputMode::None};
std::atomic<bool> controls_enabled{true};

void clear_inputs() {
    left_pressure.store(0.0f);
    right_pressure.store(0.0f);
    wheel_speed.store(0.0f);
    left_stick.store(0.0f);
    right_stick.store(0.0f);
    left_trigger.store(0.0f);
    right_trigger.store(0.0f);
    dpad_up_pressed.store(false);
    dpad_down_pressed.store(false);
    create_pressed.store(false);
    options_pressed.store(false);
    stop_chord_active.store(false);
}

void set_input_mode(InputMode mode) {
    if (input_mode.exchange(mode) == mode) return;
    clear_inputs();
    switch (mode) {
        case InputMode::Mouse:
            std::cout << "[VSTUP] Detekována analogová myš.\n";
            break;
        case InputMode::DualSense:
            std::cout << "[VSTUP] Detekován DualSense.\n";
            break;
        case InputMode::None:
            std::cout << "[VSTUP] Čekám na myš nebo Bluetooth DualSense.\n";
            break;
    }
}

std::vector<InputDevice> list_input_devices() {
    std::ifstream devices("/proc/bus/input/devices");
    std::vector<InputDevice> result;
    if (!devices.is_open()) {
        std::cerr << "[CHYBA] Nelze přečíst /proc/bus/input/devices.\n";
        return result;
    }

    InputDevice current;
    std::string line;
    auto add_current = [&]() {
        if (!current.event_path.empty()) result.push_back(current);
        current = InputDevice{};
    };

    while (std::getline(devices, line)) {
        if (line.empty()) {
            add_current();
        } else if (line.rfind("N: Name=\"", 0) == 0) {
            const auto end = line.rfind('"');
            if (end > 9) current.name = line.substr(9, end - 9);
        } else if (line.rfind("I: Bus=", 0) == 0) {
            std::sscanf(line.c_str(), "I: Bus=%*x Vendor=%x Product=%x",
                        &current.vendor, &current.product);
        } else if (line.rfind("H: Handlers=", 0) == 0) {
            std::istringstream handlers(line.substr(12));
            std::string handler;
            while (handlers >> handler) {
                if (handler.rfind("event", 0) == 0) {
                    current.event_path = "/dev/input/" + handler;
                    break;
                }
            }
        }
    }
    add_current();
    return result;
}

bool has_event_bit(const std::vector<unsigned long>& bits, unsigned int code) {
    constexpr unsigned int BITS_PER_WORD = sizeof(unsigned long) * 8;
    const unsigned int word = code / BITS_PER_WORD;
    return word < bits.size() &&
           (bits[word] & (1UL << (code % BITS_PER_WORD))) != 0;
}

bool has_capability(int fd, unsigned int type, unsigned int code) {
    std::array<unsigned long, (KEY_MAX + sizeof(unsigned long) * 8) /
                                  (sizeof(unsigned long) * 8)> bits{};
    if (ioctl(fd, EVIOCGBIT(type, sizeof(bits)), bits.data()) < 0) return false;
    return has_event_bit(bits, code);
}

OpenInputDevice find_mouse_event() {
    for (const auto& device : list_input_devices()) {
        if (device.name.find("Logitech") == std::string::npos) continue;
        const int fd = open(device.event_path.c_str(), O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;
        const bool is_mouse = has_capability(fd, EV_REL, REL_WHEEL) &&
                              has_capability(fd, EV_KEY, BTN_MIDDLE);
        if (is_mouse) return {fd, device.event_path};
        close(fd);
    }
    return {};
}

bool is_dualsense(const InputDevice& device) {
    const bool sony_id = device.vendor == 0x054c &&
                         (device.product == 0x0ce6 || device.product == 0x0df2);
    return sony_id || device.name.find("DualSense") != std::string::npos;
}

OpenInputDevice find_dualsense_event() {
    for (const auto& device : list_input_devices()) {
        if (!is_dualsense(device)) continue;
        const int fd = open(device.event_path.c_str(), O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;
        const bool is_gamepad =
            has_capability(fd, EV_ABS, ABS_X) &&
            has_capability(fd, EV_ABS, ABS_Y) &&
            has_capability(fd, EV_ABS, ABS_RX) &&
            has_capability(fd, EV_ABS, ABS_RY) &&
            has_capability(fd, EV_ABS, ABS_Z) &&
            has_capability(fd, EV_ABS, ABS_RZ);
        if (is_gamepad) return {fd, device.event_path};
        close(fd);
    }
    return {};
}

bool input_device_is_present(const OpenInputDevice& open_device) {
    if (open_device.fd < 0) return false;
    const auto devices = list_input_devices();
    return std::any_of(devices.begin(), devices.end(),
                       [&](const InputDevice& device) {
                           return device.event_path == open_device.path;
                       });
}

bool read_axis(int fd, unsigned int code, input_absinfo& axis) {
    return ioctl(fd, EVIOCGABS(code), &axis) == 0 && axis.maximum > axis.minimum;
}

float stick_value(const input_absinfo& axis) {
    const float center = (axis.minimum + axis.maximum) / 2.0f;
    const float value = static_cast<float>(axis.value);
    const float normalized = value < center
        ? (center - value) / (center - axis.minimum)
        : -(value - center) / (axis.maximum - center);
    const float clamped = std::clamp(normalized, -1.0f, 1.0f);
    return std::abs(clamped) < 0.08f ? 0.0f : clamped;
}

float trigger_value(const input_absinfo& axis) {
    return std::clamp((axis.value - axis.minimum) /
                          static_cast<float>(axis.maximum - axis.minimum),
                      0.0f, 1.0f);
}

void update_dualsense_axes(int fd) {
    input_absinfo axis{};
    if (read_axis(fd, ABS_Y, axis)) left_stick.store(stick_value(axis));
    if (read_axis(fd, ABS_RY, axis)) right_stick.store(stick_value(axis));
    if (read_axis(fd, ABS_Z, axis)) left_trigger.store(trigger_value(axis));
    if (read_axis(fd, ABS_RZ, axis)) right_trigger.store(trigger_value(axis));
}

void handle_mouse_event(const input_event& event) {
    if (event.type == EV_REL && event.code == REL_WHEEL) {
        float speed = wheel_speed.load();
        if (event.value > 0) speed = std::min(255.0f, speed + 30.0f);
        if (event.value < 0) speed = std::max(-255.0f, speed - 30.0f);
        wheel_speed.store(speed);
        last_wheel_time.store(std::chrono::steady_clock::now());
    }

    if (event.type == EV_KEY && event.value == 1) {
        if (event.code == BTN_MIDDLE) {
            controls_enabled.store(!controls_enabled.load());
            std::cout << (controls_enabled.load()
                              ? "--- OVLADANI ZAPNUTO ---\n"
                              : "--- OVLADANI VYPNUTO (STOP) ---\n");
        }
        if (event.code == BTN_SIDE) direction_mode.store(-1);
        if (event.code == BTN_EXTRA) direction_mode.store(1);
    }
}

void handle_dualsense_event(const input_event& event) {
    if (event.type != EV_KEY || (event.value != 0 && event.value != 1)) return;

    const bool pressed = event.value == 1;
    if (event.code == BTN_DPAD_UP) dpad_up_pressed.store(pressed);
    if (event.code == BTN_DPAD_DOWN) dpad_down_pressed.store(pressed);
    if (event.code == BTN_SELECT) create_pressed.store(pressed);
    if (event.code == BTN_START) options_pressed.store(pressed);

    const bool stop_chord = create_pressed.load() && options_pressed.load();
    const bool chord_was_pressed = stop_chord_active.exchange(stop_chord);
    if (stop_chord && !chord_was_pressed) {
        const bool enabled = !controls_enabled.load();
        controls_enabled.store(enabled);
        std::cout << (enabled ? "--- OVLADANI ZAPNUTO ---\n"
                              : "--- NOUZOVE ZASTAVENI ---\n");
    }
}

void input_thread() {
    const bool hid_initialized = hid_init() == 0;
    if (!hid_initialized) {
        std::cerr << "[CHYBA] HIDAPI se nepodařilo inicializovat.\n";
    }

    hid_device* hid = nullptr;
    OpenInputDevice mouse;
    OpenInputDevice dualsense;
    auto next_probe = std::chrono::steady_clock::now();
    auto next_presence_check = next_probe;
    auto next_wait_message = next_probe;
    auto last_lease = next_probe - std::chrono::seconds(10);
    unsigned char buffer[64]{};

    while (true) {
        const auto now = std::chrono::steady_clock::now();
        if (now >= next_probe) {
            if (hid_initialized && !hid) {
                hid = hid_open(0x046D, 0xC54D, nullptr);
                if (!hid) hid = hid_open(0x046D, 0xC0A8, nullptr);
                if (hid && hid_set_nonblocking(hid, 1) < 0) {
                    std::cerr << "[CHYBA] HIDAPI nepovolilo neblokující čtení.\n";
                    hid_close(hid);
                    hid = nullptr;
                }
            }

            if (hid) {
                if (dualsense.fd >= 0) {
                    close(dualsense.fd);
                    dualsense = {};
                }
                if (mouse.fd < 0) mouse = find_mouse_event();
                set_input_mode(mouse.fd >= 0 ? InputMode::Mouse : InputMode::None);
            } else {
                if (mouse.fd >= 0) {
                    close(mouse.fd);
                    mouse = {};
                }
                bool newly_connected = false;
                if (dualsense.fd < 0) {
                    dualsense = find_dualsense_event();
                    newly_connected = dualsense.fd >= 0;
                }
                set_input_mode(dualsense.fd >= 0 ? InputMode::DualSense : InputMode::None);
                if (newly_connected) update_dualsense_axes(dualsense.fd);
                if (dualsense.fd < 0 && now >= next_wait_message) {
                    std::cout << "[INFO] Není připojená myš; hledám spárovaný "
                                 "DualSense připojený přes Bluetooth.\n";
                    next_wait_message = now + std::chrono::seconds(5);
                }
            }
            next_probe = now + std::chrono::seconds(1);
        }

        if (now >= next_presence_check) {
            const InputMode mode = input_mode.load();
            OpenInputDevice* active_device =
                mode == InputMode::Mouse ? &mouse
                : mode == InputMode::DualSense ? &dualsense
                                               : nullptr;
            if (active_device && !input_device_is_present(*active_device)) {
                std::cerr << "[CHYBA] Ovladač byl odpojen; zastavuji motory.\n";
                close(active_device->fd);
                *active_device = {};
                set_input_mode(InputMode::None);
                next_probe = now;
            }
            next_presence_check = now + std::chrono::milliseconds(200);
        }

        if (hid) {
            if (now - last_lease >= std::chrono::seconds(2)) {
                unsigned char report[20] = {0x11, 0xFF, 0x0C, 0x38, 0x02, 0x08};
                if (hid_write(hid, report, sizeof(report)) < 0) {
                    std::cerr << "[CHYBA] Selhalo obnovení HID++ spojení s myší.\n";
                    hid_close(hid);
                    hid = nullptr;
                    if (mouse.fd >= 0) close(mouse.fd);
                    mouse = {};
                    set_input_mode(InputMode::None);
                } else {
                    report[1] = 0x01;
                    if (hid_write(hid, report, sizeof(report)) < 0) {
                        std::cerr << "[CHYBA] Selhalo obnovení HID++ spojení s myší.\n";
                        hid_close(hid);
                        hid = nullptr;
                        if (mouse.fd >= 0) close(mouse.fd);
                        mouse = {};
                        set_input_mode(InputMode::None);
                    } else {
                        last_lease = now;
                    }
                }
            }

            if (hid) {
                const int size = hid_read(hid, buffer, sizeof(buffer));
                if (size < 0) {
                    std::cerr << "[CHYBA] HIDAPI ztratilo spojení s myší.\n";
                    hid_close(hid);
                    hid = nullptr;
                    if (mouse.fd >= 0) close(mouse.fd);
                    mouse = {};
                    set_input_mode(InputMode::None);
                } else if (size >= 20 && buffer[0] == 0x11 &&
                           buffer[2] == 0x0C && buffer[3] == 0x10) {
                    const int left_raw = ((buffer[4] << 8) | buffer[5]) >> 6;
                    const int right_raw = ((buffer[6] << 8) | buffer[7]) >> 6;
                    left_pressure.store(
                        std::clamp((left_raw - 300) / 700.0f, 0.0f, 1.0f));
                    right_pressure.store(
                        std::clamp((right_raw - 300) / 700.0f, 0.0f, 1.0f));
                }
            }
        }

        const int active_fd = input_mode.load() == InputMode::Mouse
                                  ? mouse.fd
                                  : (input_mode.load() == InputMode::DualSense
                                         ? dualsense.fd
                                         : -1);
        if (active_fd >= 0) {
            input_event event{};
            const ssize_t count = read(active_fd, &event, sizeof(event));
            if (count == static_cast<ssize_t>(sizeof(event))) {
                if (input_mode.load() == InputMode::Mouse) {
                    handle_mouse_event(event);
                } else {
                    if (event.type == EV_ABS) update_dualsense_axes(dualsense.fd);
                    if (event.type == EV_KEY) handle_dualsense_event(event);
                    if (event.type == EV_SYN && event.code == SYN_DROPPED)
                        update_dualsense_axes(dualsense.fd);
                }
            } else if (count == 0 ||
                       (count < 0 && errno != EAGAIN && errno != EWOULDBLOCK &&
                        errno != EINTR)) {
                if (input_mode.load() == InputMode::Mouse) {
                    close(mouse.fd);
                    mouse = {};
                } else {
                    close(dualsense.fd);
                    dualsense = {};
                }
                set_input_mode(InputMode::None);
                next_probe = std::chrono::steady_clock::now();
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

}  // namespace

int main() {
    std::cout << "Inicializace robota Wall-E (Jetson C++ Port)...\n";
    std::cout << "Ovládání myší má přednost; bez jejího donglu se hledá DualSense.\n";

    Motor left_motor(0, 1);
    Motor right_motor(2, 3);
    left_motor.begin();
    right_motor.begin();

    std::thread input(input_thread);

    float dpad_speed = 0.0f;
    float left_motor_speed = 0.0f;
    float right_motor_speed = 0.0f;
    auto previous_control_update = std::chrono::steady_clock::now();
    while (true) {
        const auto now = std::chrono::steady_clock::now();
        const float delta_seconds = std::clamp(
            std::chrono::duration<float>(now - previous_control_update).count(),
            0.0f, 0.1f);
        previous_control_update = now;

        if (!controls_enabled.load()) {
            clear_inputs();
            dpad_speed = 0.0f;
            left_motor_speed = 0.0f;
            right_motor_speed = 0.0f;
            left_motor.setSpeed(0);
            right_motor.setSpeed(0);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            continue;
        }

        int final_left = 0;
        int final_right = 0;
        const InputMode mode = input_mode.load();
        if (mode != InputMode::DualSense) dpad_speed = 0.0f;

        if (mode == InputMode::DualSense) {
            const int dpad_direction =
                static_cast<int>(dpad_up_pressed.load()) -
                static_cast<int>(dpad_down_pressed.load());
            const float target_dpad_speed = dpad_direction * 255.0f;
            constexpr float DPAD_ACCELERATION = 170.0f;
            const float max_speed_change = DPAD_ACCELERATION * delta_seconds;
            if (dpad_speed < target_dpad_speed) {
                dpad_speed = std::min(target_dpad_speed,
                                      dpad_speed + max_speed_change);
            } else if (dpad_speed > target_dpad_speed) {
                dpad_speed = std::max(target_dpad_speed,
                                      dpad_speed - max_speed_change);
            }

            const float turn = right_trigger.load() - left_trigger.load();
            if (dpad_direction != 0 || std::abs(dpad_speed) > 0.0f) {
                final_left = static_cast<int>(
                    std::clamp(dpad_speed + turn * 255.0f, -255.0f, 255.0f));
                final_right = static_cast<int>(
                    std::clamp(dpad_speed - turn * 255.0f, -255.0f, 255.0f));
            } else {
                const float left =
                    std::clamp(left_stick.load() + turn, -1.0f, 1.0f);
                const float right =
                    std::clamp(right_stick.load() - turn, -1.0f, 1.0f);
                final_left = static_cast<int>(left * 255.0f);
                final_right = static_cast<int>(right * 255.0f);
            }
        } else if (mode == InputMode::Mouse) {
            float current_wheel = wheel_speed.load();
            const auto now = std::chrono::steady_clock::now();
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                     now - last_wheel_time.load())
                                     .count();

            if (elapsed > 500) {
                if (current_wheel > 0)
                    current_wheel = std::max(0.0f, current_wheel - 5.0f);
                if (current_wheel < 0)
                    current_wheel = std::min(0.0f, current_wheel + 5.0f);
                wheel_speed.store(current_wheel);
            }

            if (std::abs(current_wheel) > 5.0f) {
                final_left = static_cast<int>(current_wheel);
                final_right = static_cast<int>(current_wheel);
            } else {
                constexpr float DEADZONE = 0.08f;
                float left = left_pressure.load();
                float right = right_pressure.load();
                if (left < DEADZONE) left = 0.0f;
                if (right < DEADZONE) right = 0.0f;
                const int direction = direction_mode.load();
                final_left = static_cast<int>(left * 255.0f) * direction;
                final_right = static_cast<int>(right * 255.0f) * direction;
            }
        }

        if (mode == InputMode::DualSense) {
            constexpr float MOTOR_ACCELERATION = 510.0f;
            const float max_speed_change = MOTOR_ACCELERATION * delta_seconds;
            left_motor_speed += std::clamp(
                final_left - left_motor_speed, -max_speed_change, max_speed_change);
            right_motor_speed += std::clamp(
                final_right - right_motor_speed, -max_speed_change, max_speed_change);
            final_left = static_cast<int>(left_motor_speed);
            final_right = static_cast<int>(right_motor_speed);
        } else {
            left_motor_speed = 0.0f;
            right_motor_speed = 0.0f;
        }

        left_motor.setSpeed(final_left);
        right_motor.setSpeed(final_right);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    input.join();
    return 0;
}
