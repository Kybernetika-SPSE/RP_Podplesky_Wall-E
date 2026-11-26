#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <hidapi/hidapi.h>
#include <linux/hidraw.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "encoder.h"
#include "motor.h"
#include "pins.h"

namespace fs = std::filesystem;

namespace {

// --- Control & Tuning Constants (1:1 with WALL-E DRY-RUN GUI simulator) ---

constexpr int PWM_MAX = 255;
constexpr int MOTOR_DEADBAND = 15;
constexpr float STICK_DEADZONE = 0.08f;

constexpr int SUPERSTRIKE_RAW_MIN = 300;
constexpr int SUPERSTRIKE_RAW_MAX = 720;
constexpr float SUPERSTRIKE_DEADZONE = 0.12f;
constexpr float SUPERSTRIKE_GAMMA = 2.4f;
constexpr float SUPERSTRIKE_SMOOTHING = 0.35f;
constexpr float SUPERSTRIKE_RELEASE_SMOOTHING = 0.65f;

constexpr float WHEEL_STEP = 30.0f;
constexpr float WHEEL_DECAY_DELAY_S = 0.5f;
constexpr float WHEEL_DECAY_RATE_PWM_S = 250.0f;
constexpr float WHEEL_ACTIVE_THRESHOLD = 5.0f;

constexpr float DPAD_ACCELERATION = 170.0f;
constexpr float CRUISE_ADJUST_RATE = 120.0f;
constexpr float CRUISE_TRIGGER_CANCEL_THRESHOLD = 0.10f;
constexpr float CRUISE_TRIGGER_RELEASE_THRESHOLD = 0.05f;
constexpr float MOTOR_ACCELERATION = 510.0f;

// Linux evdev button codes
constexpr unsigned int CODE_BTN_Z = 0x135;    // DualSense R1 in hid-generic
constexpr unsigned int CODE_BTN_TR = 0x137;   // DualSense R1 in hid-playstation
constexpr unsigned int CODE_BTN_TL2 = 0x138;  // DualSense Create in hid-generic
constexpr unsigned int CODE_BTN_TR2 = 0x139;  // DualSense Options in hid-generic

enum class InputMode { None, Mouse, DualSense };
enum class DualSenseProfile { HidGeneric, HidPlaystation };

struct InputDevice {
    std::string name;
    std::string event_path;
    unsigned int vendor = 0;
    unsigned int product = 0;
};

struct OpenInputDevice {
    int fd = -1;
    std::string path;
    DualSenseProfile ds_profile = DualSenseProfile::HidGeneric;
};

std::atomic<bool> g_running{true};

// Superstrike shared inputs
std::atomic<int> ss_left_raw{0};
std::atomic<int> ss_right_raw{0};
std::atomic<int> ss_direction_mode{1};
std::atomic<int> ss_pending_wheel_delta{0};

// DualSense shared inputs
std::atomic<float> ds_left_stick_x{0.0f};
std::atomic<float> ds_left_stick_y{0.0f};
std::atomic<float> ds_right_stick_x{0.0f};
std::atomic<float> ds_right_stick_y{0.0f};
std::atomic<float> ds_left_trigger{0.0f};
std::atomic<float> ds_right_trigger{0.0f};
std::atomic<bool> ds_dpad_up{false};
std::atomic<bool> ds_dpad_down{false};
std::atomic<bool> ds_dpad_left{false};
std::atomic<bool> ds_dpad_right{false};
std::atomic<bool> ds_r1_pressed{false};
std::atomic<bool> ds_create_pressed{false};
std::atomic<bool> ds_options_pressed{false};
std::atomic<bool> ds_stop_chord_active{false};

std::atomic<int> estop_toggle_seq{0};
std::atomic<int> cruise_toggle_seq{0};
std::atomic<InputMode> input_mode{InputMode::None};

void handle_signal(int) {
    g_running.store(false);
}

void clear_inputs() {
    ss_left_raw.store(0);
    ss_right_raw.store(0);
    ss_pending_wheel_delta.store(0);

    ds_left_stick_x.store(0.0f);
    ds_left_stick_y.store(0.0f);
    ds_right_stick_x.store(0.0f);
    ds_right_stick_y.store(0.0f);
    ds_left_trigger.store(0.0f);
    ds_right_trigger.store(0.0f);
    ds_dpad_up.store(false);
    ds_dpad_down.store(false);
    ds_dpad_left.store(false);
    ds_dpad_right.store(false);
    ds_r1_pressed.store(false);
    ds_create_pressed.store(false);
    ds_options_pressed.store(false);
    ds_stop_chord_active.store(false);
}

void set_input_mode(InputMode mode) {
    if (input_mode.exchange(mode) == mode) return;
    clear_inputs();
    switch (mode) {
        case InputMode::Mouse:
            std::cout << "[VSTUP] Aktivní Logitech Superstrike (primární).\n";
            break;
        case InputMode::DualSense:
            std::cout << "[VSTUP] Aktivní Sony DualSense.\n";
            break;
        case InputMode::None:
            std::cout << "[VSTUP] Čekám na Superstrike nebo DualSense...\n";
            break;
    }
}

// --- Pure Math Helpers (matching simulation.py) ---

float shape_superstrike_pressure(int raw) {
    const float lin = std::clamp(
        (raw - SUPERSTRIKE_RAW_MIN) /
            static_cast<float>(SUPERSTRIKE_RAW_MAX - SUPERSTRIKE_RAW_MIN),
        0.0f, 1.0f);
    if (lin <= SUPERSTRIKE_DEADZONE) return 0.0f;
    const float rescaled = (lin - SUPERSTRIKE_DEADZONE) / (1.0f - SUPERSTRIKE_DEADZONE);
    return std::clamp(std::pow(rescaled, SUPERSTRIKE_GAMMA), 0.0f, 1.0f);
}

std::pair<int, int> apply_proportional_steering(float base_pwm, float left_stick_x) {
    const float steer = std::clamp(left_stick_x, -1.0f, 1.0f);
    if (base_pwm == 0.0f || steer == 0.0f) {
        const int v = static_cast<int>(std::clamp(base_pwm, -255.0f, 255.0f));
        return {v, v};
    }
    // Forward (base > 0): LEFT (+x) slows LEFT track, RIGHT (-x) slows RIGHT track.
    // Reverse (base < 0): inverted.
    const float eff_steer = (base_pwm > 0.0f) ? steer : -steer;
    float left = base_pwm;
    float right = base_pwm;
    if (eff_steer > 0.0f) {
        left = base_pwm * (1.0f - eff_steer);
    } else {
        right = base_pwm * (1.0f + eff_steer);
    }
    return {
        static_cast<int>(std::clamp(left, -255.0f, 255.0f)),
        static_cast<int>(std::clamp(right, -255.0f, 255.0f)),
    };
}

// --- Standard CRC-32 (IEEE 802.3) for DualSense Bluetooth HID reports ---

std::uint32_t crc32_dualsense_bt(const std::uint8_t* data, std::size_t len) {
    std::uint32_t crc = 0xFFFFFFFFu;
    auto feed = [&](std::uint8_t byte) {
        crc ^= byte;
        for (int i = 0; i < 8; ++i) {
            crc = (crc & 1u) ? ((crc >> 1) ^ 0xEDB88320u) : (crc >> 1);
        }
    };
    feed(0xA2);  // Bluetooth HID output report seed
    for (std::size_t i = 0; i < len; ++i) {
        feed(data[i]);
    }
    return ~crc;
}

// --- DualSense Feedback Controller (Adaptive Triggers + Rumble + LEDs) ---

using TriggerFx = std::array<std::uint8_t, 11>;

constexpr TriggerFx TRIGGER_FX_OFF           = {0x00};
constexpr TriggerFx TRIGGER_FX_ESTOP_LOCK    = {0x01, 0x00, 0xFF};
constexpr TriggerFx TRIGGER_FX_CRUISE_DETENT = {0x02, 0x05, 0x50, 0xFF};
constexpr TriggerFx TRIGGER_FX_MANUAL_R2     = {0x01, 0x18, 0x45};
constexpr TriggerFx TRIGGER_FX_MANUAL_L2     = {0x01, 0x14, 0x70};

class DualSenseFeedback {
private:
    std::string hid_path;
    bool is_bluetooth = false;
    std::uint8_t seq = 0;

    std::string sysfs_rgb_intensity;
    std::string sysfs_rgb_brightness;
    std::array<std::string, 5> sysfs_players;
    bool has_sysfs_leds = false;

    std::chrono::steady_clock::time_point last_scan{};
    std::chrono::steady_clock::time_point last_write{};
    std::chrono::steady_clock::time_point pulse_until{};
    std::uint8_t pulse_l = 0;
    std::uint8_t pulse_r = 0;

    bool bt_enhanced_init_done = false;
    bool has_last_state = false;
    TriggerFx last_r2{};
    TriggerFx last_l2{};
    std::uint8_t last_rl = 0;
    std::uint8_t last_rr = 0;
    std::array<std::uint8_t, 3> last_rgb{0, 0, 0};
    std::uint8_t last_player_mask = 0;

    void discover(std::chrono::steady_clock::time_point now) {
        if (now - last_scan < std::chrono::seconds(2)) return;
        last_scan = now;

        hid_path.clear();
        is_bluetooth = false;
        bt_enhanced_init_done = false;
        std::error_code ec;
        if (fs::exists("/sys/class/hidraw", ec)) {
            for (const auto& entry : fs::directory_iterator("/sys/class/hidraw", ec)) {
                const std::string name = entry.path().filename().string();
                if (name.rfind("hidraw", 0) != 0) continue;
                std::ifstream uev(entry.path() / "device/uevent");
                std::string line;
                while (std::getline(uev, line)) {
                    if (line.rfind("HID_ID=", 0) == 0) {
                        unsigned int bus = 0, vid = 0, pid = 0;
                        if (std::sscanf(line.c_str() + 7, "%x:%x:%x", &bus, &vid, &pid) == 3) {
                            if (vid == 0x054C && (pid == 0x0CE6 || pid == 0x0DF2)) {
                                hid_path = "/dev/" + name;
                                is_bluetooth = (bus == 0x0005);
                                break;
                            }
                        }
                    }
                }
                if (!hid_path.empty()) break;
            }
        }

        // Check if hid-playstation sysfs LEDs exist (e.g. CachyOS / newer kernels)
        has_sysfs_leds = false;
        sysfs_rgb_intensity.clear();
        sysfs_rgb_brightness.clear();
        sysfs_players.fill("");
        if (fs::exists("/sys/class/leds", ec)) {
            for (const auto& entry : fs::directory_iterator("/sys/class/leds", ec)) {
                const std::string name = entry.path().filename().string();
                const std::string suffix = ":rgb:indicator";
                if (name.size() <= suffix.size() ||
                    name.compare(name.size() - suffix.size(), suffix.size(), suffix) != 0) {
                    continue;
                }
                const std::string resolved = fs::canonical(entry.path(), ec).string();
                if (resolved.find("054C:0CE6") != std::string::npos ||
                    resolved.find("054C:0DF2") != std::string::npos ||
                    resolved.find("054c:0ce6") != std::string::npos ||
                    resolved.find("054c:0df2") != std::string::npos) {
                    const std::string prefix = name.substr(0, name.size() - suffix.size());
                    const fs::path intensity = entry.path() / "multi_intensity";
                    if (fs::exists(intensity, ec)) {
                        sysfs_rgb_intensity = intensity.string();
                        const fs::path bright = entry.path() / "brightness";
                        if (fs::exists(bright, ec)) sysfs_rgb_brightness = bright.string();
                        for (int i = 0; i < 5; ++i) {
                            const fs::path pp = fs::path("/sys/class/leds") /
                                                (prefix + ":white:player-" + std::to_string(i + 1)) /
                                                "brightness";
                            if (fs::exists(pp, ec)) sysfs_players[i] = pp.string();
                        }
                        has_sysfs_leds = true;
                        break;
                    }
                }
            }
        }
    }

    void writeSysfsLeds(const std::array<std::uint8_t, 3>& rgb, std::uint8_t player_mask) {
        if (!has_sysfs_leds) return;
        if (!sysfs_rgb_brightness.empty() && !has_last_state) {
            std::ofstream bf(sysfs_rgb_brightness);
            if (bf.is_open()) bf << "255\n";
        }
        if (rgb != last_rgb || !has_last_state) {
            std::ofstream rf(sysfs_rgb_intensity);
            if (rf.is_open()) {
                rf << static_cast<int>(rgb[0]) << ' '
                   << static_cast<int>(rgb[1]) << ' '
                   << static_cast<int>(rgb[2]) << '\n';
            }
        }
        if (player_mask != last_player_mask || !has_last_state) {
            for (int i = 0; i < 5; ++i) {
                if (!sysfs_players[i].empty()) {
                    std::ofstream pf(sysfs_players[i]);
                    if (pf.is_open()) pf << (((player_mask >> i) & 1u) ? "1\n" : "0\n");
                }
            }
        }
    }

public:
    void pulse(std::uint8_t left, std::uint8_t right, int duration_ms) {
        pulse_l = left;
        pulse_r = right;
        pulse_until = std::chrono::steady_clock::now() + std::chrono::milliseconds(duration_ms);
    }

    void apply(const TriggerFx& r2_fx,
               const TriggerFx& l2_fx,
               std::uint8_t rumble_l,
               std::uint8_t rumble_r,
               const std::array<std::uint8_t, 3>& rgb,
               std::uint8_t player_mask,
               bool force = false) {
        const auto now = std::chrono::steady_clock::now();
        if (hid_path.empty() || !fs::exists(hid_path)) {
            discover(now);
        }

        if (now < pulse_until) {
            rumble_l = std::max(rumble_l, pulse_l);
            rumble_r = std::max(rumble_r, pulse_r);
        }

        const bool changed = !has_last_state ||
                             r2_fx != last_r2 || l2_fx != last_l2 ||
                             rumble_l != last_rl || rumble_r != last_rr ||
                             rgb != last_rgb || player_mask != last_player_mask;
        if (!force && !changed) return;
        if (!force && (now - last_write < std::chrono::milliseconds(50))) return;

        if (has_sysfs_leds) {
            writeSysfsLeds(rgb, player_mask);
        }

        if (!hid_path.empty()) {
            std::array<std::uint8_t, 47> common{};
            common[0] = 0x0F;  // rumble + haptics + R2 + L2 triggers
            // If sysfs LEDs are NOT present (Jetson hid-generic), update LEDs directly in HID report
            common[1] = has_sysfs_leds ? 0x00 : 0x14;  // 0x04 lightbar | 0x10 player LEDs
            common[2] = rumble_r;
            common[3] = rumble_l;
            std::copy(r2_fx.begin(), r2_fx.end(), common.begin() + 10);
            std::copy(l2_fx.begin(), l2_fx.end(), common.begin() + 21);
            if (!has_sysfs_leds) {
                common[43] = player_mask & 0x1F;
                common[44] = rgb[0];
                common[45] = rgb[1];
                common[46] = rgb[2];
            }

            const int fd = ::open(hid_path.c_str(), O_RDWR | O_NONBLOCK);
            if (fd >= 0) {
                // On hid-generic (Jetson kernel 5.15), DualSense over Bluetooth stays in
                // Basic HID mode and ignores 0x31 output reports until Feature Report 0x05
                // (41 bytes calibration) is read once to switch it into Enhanced BT mode.
                if (is_bluetooth && !bt_enhanced_init_done) {
                    std::array<std::uint8_t, 41> feat{};
                    feat[0] = 0x05;
                    (void)::ioctl(fd, HIDIOCGFEATURE(feat.size()), feat.data());
                    bt_enhanced_init_done = true;
                }

                if (!has_sysfs_leds) {
                    // Ensure lightbar is enabled at full brightness on hid-generic
                    common[38] = 0x02;  // valid_flag2: lightbar setup
                    common[41] = 0x02;  // DS_OUTPUT_LIGHTBAR_SETUP_LIGHT_OUT
                    common[42] = 0x00;  // led_brightness: 0x00 = HIGH
                }

                if (!is_bluetooth) {
                    std::array<std::uint8_t, 48> pkt{};
                    pkt[0] = 0x02;
                    std::copy(common.begin(), common.end(), pkt.begin() + 1);
                    (void)::write(fd, pkt.data(), pkt.size());
                } else {
                    std::array<std::uint8_t, 78> pkt{};
                    pkt[0] = 0x31;
                    pkt[1] = static_cast<std::uint8_t>((seq & 0x0F) << 4);
                    pkt[2] = 0x10;  // DS_OUTPUT_TAG
                    std::copy(common.begin(), common.end(), pkt.begin() + 3);
                    const std::uint32_t crc = crc32_dualsense_bt(pkt.data(), 74);
                    pkt[74] = static_cast<std::uint8_t>(crc & 0xFF);
                    pkt[75] = static_cast<std::uint8_t>((crc >> 8) & 0xFF);
                    pkt[76] = static_cast<std::uint8_t>((crc >> 16) & 0xFF);
                    pkt[77] = static_cast<std::uint8_t>((crc >> 24) & 0xFF);
                    (void)::write(fd, pkt.data(), pkt.size());
                    seq = static_cast<std::uint8_t>((seq + 1) & 0x0F);
                }
                ::close(fd);
            } else {
                hid_path.clear();
            }
        }

        last_r2 = r2_fx;
        last_l2 = l2_fx;
        last_rl = rumble_l;
        last_rr = rumble_r;
        last_rgb = rgb;
        last_player_mask = player_mask;
        has_last_state = true;
        last_write = now;
    }

    void resetOff() {
        pulse_until = std::chrono::steady_clock::time_point{};
        apply(TRIGGER_FX_OFF, TRIGGER_FX_OFF, 0, 0, {0, 0, 64}, 0x01, true);
    }
};

// --- Linux Input Discovery & Parsing ---

std::vector<InputDevice> list_input_devices() {
    std::ifstream devices("/proc/bus/input/devices");
    std::vector<InputDevice> result;
    if (!devices.is_open()) return result;

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

template <std::size_t N>
bool has_event_bit(const std::array<unsigned long, N>& bits, unsigned int code) {
    constexpr unsigned int BITS_PER_WORD = sizeof(unsigned long) * 8;
    const unsigned int word = code / BITS_PER_WORD;
    return word < bits.size() &&
           (bits[word] & (1UL << (code % BITS_PER_WORD))) != 0;
}

bool has_capability(int fd, unsigned int type, unsigned int code) {
    std::array<unsigned long, (KEY_MAX + sizeof(unsigned long) * 8) /
                                  (sizeof(unsigned long) * 8)> bits{};
    if (::ioctl(fd, EVIOCGBIT(type, sizeof(bits)), bits.data()) < 0) return false;
    return has_event_bit(bits, code);
}

DualSenseProfile detect_dualsense_profile(int fd, const std::string& event_path) {
    if (has_capability(fd, EV_KEY, BTN_DPAD_UP)) {
        return DualSenseProfile::HidPlaystation;
    }
    std::error_code ec;
    const std::string ev_name = fs::path(event_path).filename().string();
    const fs::path drv_link = fs::path("/sys/class/input") / ev_name / "device/device/driver";
    if (fs::exists(drv_link, ec)) {
        const std::string drv = fs::canonical(drv_link, ec).filename().string();
        if (drv == "playstation" || drv == "hid-playstation") {
            return DualSenseProfile::HidPlaystation;
        }
    }
    return DualSenseProfile::HidGeneric;
}

OpenInputDevice find_mouse_event() {
    for (const auto& device : list_input_devices()) {
        const bool logitech_id = device.vendor == 0x046D &&
                                 (device.product == 0xC54D || device.product == 0xC0A8);
        if (!logitech_id && device.name.find("Logitech") == std::string::npos) continue;
        const int fd = ::open(device.event_path.c_str(), O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;
        const bool is_mouse = has_capability(fd, EV_REL, REL_WHEEL) &&
                              has_capability(fd, EV_KEY, BTN_MIDDLE);
        if (is_mouse) return {fd, device.event_path, DualSenseProfile::HidGeneric};
        ::close(fd);
    }
    return {};
}

bool is_dualsense(const InputDevice& device) {
    const bool sony_id = device.vendor == 0x054C &&
                         (device.product == 0x0CE6 || device.product == 0x0DF2);
    return sony_id || device.name.find("DualSense") != std::string::npos;
}

OpenInputDevice find_dualsense_event() {
    for (const auto& device : list_input_devices()) {
        if (!is_dualsense(device)) continue;
        const int fd = ::open(device.event_path.c_str(), O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;
        const bool is_gamepad =
            has_capability(fd, EV_ABS, ABS_X) &&
            has_capability(fd, EV_ABS, ABS_Y) &&
            has_capability(fd, EV_ABS, ABS_RX) &&
            has_capability(fd, EV_ABS, ABS_RY) &&
            has_capability(fd, EV_ABS, ABS_Z) &&
            has_capability(fd, EV_ABS, ABS_RZ);
        if (is_gamepad) {
            const auto profile = detect_dualsense_profile(fd, device.event_path);
            std::cout << "[DUALSENSE] Profil: "
                      << (profile == DualSenseProfile::HidPlaystation ? "hid-playstation" : "hid-generic")
                      << " (" << device.event_path << ")\n";
            return {fd, device.event_path, profile};
        }
        ::close(fd);
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
    return ::ioctl(fd, EVIOCGABS(code), &axis) == 0 && axis.maximum > axis.minimum;
}

float stick_value(const input_absinfo& axis) {
    const float center = (axis.minimum + axis.maximum) / 2.0f;
    const float value = static_cast<float>(axis.value);
    const float normalized = value < center
        ? (center - value) / (center - axis.minimum)
        : -(value - center) / (axis.maximum - center);
    const float clamped = std::clamp(normalized, -1.0f, 1.0f);
    return std::abs(clamped) < STICK_DEADZONE ? 0.0f : clamped;
}

float trigger_value(const input_absinfo& axis) {
    return std::clamp((axis.value - axis.minimum) /
                          static_cast<float>(axis.maximum - axis.minimum),
                      0.0f, 1.0f);
}

void update_dualsense_axes(int fd, DualSenseProfile profile) {
    input_absinfo axis{};
    if (read_axis(fd, ABS_X, axis)) ds_left_stick_x.store(stick_value(axis));
    if (read_axis(fd, ABS_Y, axis)) ds_left_stick_y.store(stick_value(axis));

    if (profile == DualSenseProfile::HidPlaystation) {
        if (read_axis(fd, ABS_RX, axis)) ds_right_stick_x.store(stick_value(axis));
        if (read_axis(fd, ABS_RY, axis)) ds_right_stick_y.store(stick_value(axis));
        if (read_axis(fd, ABS_Z, axis)) ds_left_trigger.store(trigger_value(axis));
        if (read_axis(fd, ABS_RZ, axis)) ds_right_trigger.store(trigger_value(axis));
    } else {
        // hid-generic on Jetson Orin Nano kernel 5.15
        if (read_axis(fd, ABS_Z, axis)) ds_right_stick_x.store(stick_value(axis));
        if (read_axis(fd, ABS_RZ, axis)) ds_right_stick_y.store(stick_value(axis));
        if (read_axis(fd, ABS_RX, axis)) ds_left_trigger.store(trigger_value(axis));
        if (read_axis(fd, ABS_RY, axis)) ds_right_trigger.store(trigger_value(axis));
    }

    if (read_axis(fd, ABS_HAT0Y, axis)) {
        ds_dpad_up.store(axis.value < 0);
        ds_dpad_down.store(axis.value > 0);
    }
    if (read_axis(fd, ABS_HAT0X, axis)) {
        ds_dpad_left.store(axis.value < 0);
        ds_dpad_right.store(axis.value > 0);
    }
}

void handle_mouse_event(const input_event& event) {
    if (event.type == EV_REL && event.code == REL_WHEEL) {
        ss_pending_wheel_delta.fetch_add(event.value);
    } else if (event.type == EV_KEY && event.value == 1) {
        if (event.code == BTN_MIDDLE) {
            estop_toggle_seq.fetch_add(1);
        } else if (event.code == BTN_SIDE) {
            ss_direction_mode.store(-1);
        } else if (event.code == BTN_EXTRA) {
            ss_direction_mode.store(1);
        }
    }
}

void handle_dualsense_key(const input_event& event, DualSenseProfile profile) {
    if (event.value != 0 && event.value != 1) return;
    const bool pressed = (event.value == 1);

    if (profile == DualSenseProfile::HidPlaystation) {
        if (event.code == BTN_DPAD_UP) ds_dpad_up.store(pressed);
        else if (event.code == BTN_DPAD_DOWN) ds_dpad_down.store(pressed);
        else if (event.code == BTN_DPAD_LEFT) ds_dpad_left.store(pressed);
        else if (event.code == BTN_DPAD_RIGHT) ds_dpad_right.store(pressed);
        else if (event.code == CODE_BTN_TR) {
            const bool was = ds_r1_pressed.exchange(pressed);
            if (pressed && !was) cruise_toggle_seq.fetch_add(1);
        } else if (event.code == BTN_SELECT) ds_create_pressed.store(pressed);
        else if (event.code == BTN_START) ds_options_pressed.store(pressed);
    } else {
        if (event.code == CODE_BTN_Z) {
            const bool was = ds_r1_pressed.exchange(pressed);
            if (pressed && !was) cruise_toggle_seq.fetch_add(1);
        } else if (event.code == CODE_BTN_TL2) ds_create_pressed.store(pressed);
        else if (event.code == CODE_BTN_TR2) ds_options_pressed.store(pressed);
    }

    const bool chord = ds_create_pressed.load() && ds_options_pressed.load();
    const bool was_chord = ds_stop_chord_active.exchange(chord);
    if (chord && !was_chord) {
        estop_toggle_seq.fetch_add(1);
    }
}

void input_thread() {
    const bool hid_initialized = (hid_init() == 0);
    if (!hid_initialized) {
        std::cerr << "[CHYBA] HIDAPI se nepodařilo inicializovat.\n";
    }

    hid_device* hid = nullptr;
    OpenInputDevice mouse;
    OpenInputDevice dualsense;
    auto next_probe = std::chrono::steady_clock::now();
    auto next_presence_check = next_probe;
    auto last_lease = next_probe - std::chrono::seconds(10);
    unsigned char buffer[64]{};

    while (g_running.load()) {
        const auto now = std::chrono::steady_clock::now();
        if (now >= next_probe) {
            if (hid_initialized && !hid) {
                hid = hid_open(0x046D, 0xC54D, nullptr);
                if (!hid) hid = hid_open(0x046D, 0xC0A8, nullptr);
                if (hid && hid_set_nonblocking(hid, 1) < 0) {
                    hid_close(hid);
                    hid = nullptr;
                }
            }

            if (hid) {
                if (dualsense.fd >= 0) {
                    ::close(dualsense.fd);
                    dualsense = {};
                }
                if (mouse.fd < 0) mouse = find_mouse_event();
                set_input_mode(InputMode::Mouse);
            } else {
                if (mouse.fd >= 0) {
                    ::close(mouse.fd);
                    mouse = {};
                }
                bool newly_connected = false;
                if (dualsense.fd < 0) {
                    dualsense = find_dualsense_event();
                    newly_connected = (dualsense.fd >= 0);
                }
                set_input_mode(dualsense.fd >= 0 ? InputMode::DualSense : InputMode::None);
                if (newly_connected) {
                    update_dualsense_axes(dualsense.fd, dualsense.ds_profile);
                }
            }
            next_probe = now + std::chrono::seconds(1);
        }

        if (now >= next_presence_check) {
            const InputMode mode = input_mode.load();
            if (mode == InputMode::DualSense && !input_device_is_present(dualsense)) {
                std::cerr << "[VSTUP] DualSense odpojen — zastavuji motory.\n";
                ::close(dualsense.fd);
                dualsense = {};
                set_input_mode(InputMode::None);
                next_probe = now;
            }
            next_presence_check = now + std::chrono::milliseconds(200);
        }

        if (hid) {
            if (now - last_lease >= std::chrono::seconds(2)) {
                unsigned char report[20] = {0x11, 0xFF, 0x0C, 0x38, 0x02, 0x08};
                bool ok = (hid_write(hid, report, sizeof(report)) >= 0);
                if (ok) {
                    report[1] = 0x01;
                    ok = (hid_write(hid, report, sizeof(report)) >= 0);
                }
                if (!ok) {
                    hid_close(hid);
                    hid = nullptr;
                    if (mouse.fd >= 0) ::close(mouse.fd);
                    mouse = {};
                    set_input_mode(InputMode::None);
                } else {
                    last_lease = now;
                }
            }

            if (hid) {
                for (int i = 0; i < 64; ++i) {
                    const int size = hid_read(hid, buffer, sizeof(buffer));
                    if (size < 0) {
                        hid_close(hid);
                        hid = nullptr;
                        if (mouse.fd >= 0) ::close(mouse.fd);
                        mouse = {};
                        set_input_mode(InputMode::None);
                        break;
                    }
                    if (size == 0) break;
                    if (size >= 20 && buffer[0] == 0x11 && buffer[2] == 0x0C && buffer[3] == 0x10) {
                        const int l_raw = ((buffer[4] << 8) | buffer[5]) >> 6;
                        const int r_raw = ((buffer[6] << 8) | buffer[7]) >> 6;
                        ss_left_raw.store(l_raw);
                        ss_right_raw.store(r_raw);
                    }
                }
            }
        }

        const InputMode mode = input_mode.load();
        const int active_fd = (mode == InputMode::Mouse) ? mouse.fd
                            : (mode == InputMode::DualSense ? dualsense.fd : -1);
        if (active_fd >= 0) {
            for (int i = 0; i < 128; ++i) {
                input_event event{};
                const ssize_t count = ::read(active_fd, &event, sizeof(event));
                if (count == static_cast<ssize_t>(sizeof(event))) {
                    if (mode == InputMode::Mouse) {
                        handle_mouse_event(event);
                    } else {
                        if (event.type == EV_ABS) {
                            if (event.code == ABS_HAT0Y) {
                                ds_dpad_up.store(event.value < 0);
                                ds_dpad_down.store(event.value > 0);
                            } else if (event.code == ABS_HAT0X) {
                                ds_dpad_left.store(event.value < 0);
                                ds_dpad_right.store(event.value > 0);
                            } else {
                                update_dualsense_axes(dualsense.fd, dualsense.ds_profile);
                            }
                        } else if (event.type == EV_KEY) {
                            handle_dualsense_key(event, dualsense.ds_profile);
                        } else if (event.type == EV_SYN && event.code == SYN_DROPPED) {
                            update_dualsense_axes(dualsense.fd, dualsense.ds_profile);
                        }
                    }
                } else if (count == 0 ||
                           (count < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) {
                    if (mode == InputMode::Mouse) {
                        ::close(mouse.fd);
                        mouse = {};
                    } else {
                        ::close(dualsense.fd);
                        dualsense = {};
                    }
                    set_input_mode(InputMode::None);
                    next_probe = std::chrono::steady_clock::now();
                    break;
                } else {
                    break;
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    if (hid) hid_close(hid);
    if (mouse.fd >= 0) ::close(mouse.fd);
    if (dualsense.fd >= 0) ::close(dualsense.fd);
    if (hid_initialized) hid_exit();
}

}  // namespace

int main(int argc, char** argv) {
    bool dry_run = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--dry-run") {
            dry_run = true;
        }
    }

    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    std::cout << "======================================================\n";
    std::cout << "  WALL-E C++ Controller (SW PWM GPIO, Jetson Orin Nano)\n";
    if (dry_run) std::cout << "  REŽIM: --dry-run (bez zápisu na GPIO)\n";
    std::cout << "======================================================\n";

    // Piny podle 4. Ročník/ELEKTRO/PCB/PCB.kicad_sch (konektor J11)
    Motor left_motor("LEFT", walle_pins::LEFT_LPWM_LINE, walle_pins::LEFT_RPWM_LINE);
    Motor right_motor("RIGHT", walle_pins::RIGHT_LPWM_LINE, walle_pins::RIGHT_RPWM_LINE);
    Encoder left_encoder("LEFT", walle_pins::LEFT_ENC_A_LINE, walle_pins::LEFT_ENC_B_LINE);
    Encoder right_encoder("RIGHT", walle_pins::RIGHT_ENC_A_LINE, walle_pins::RIGHT_ENC_B_LINE);

    left_motor.begin(dry_run);
    right_motor.begin(dry_run);
    left_encoder.begin(dry_run);
    right_encoder.begin(dry_run);

    DualSenseFeedback ds_feedback;
    std::thread input(input_thread);

    bool controls_enabled = true;
    bool cruise_active = false;
    float cruise_speed = 0.0f;
    bool cruise_wait_r2_release = false;
    bool cruise_wait_l2_release = false;

    float dpad_speed = 0.0f;
    float left_motor_ramp = 0.0f;
    float right_motor_ramp = 0.0f;
    float wheel_speed = 0.0f;
    auto last_wheel_time = std::chrono::steady_clock::now();
    float ss_smooth_l = 0.0f;
    float ss_smooth_r = 0.0f;

    int last_estop_seq = 0;
    int last_cruise_seq = 0;
    InputMode last_mode = InputMode::None;

    auto prev_tick = std::chrono::steady_clock::now();
    auto last_status_log = prev_tick;

    auto zero_motion = [&]() {
        dpad_speed = 0.0f;
        left_motor_ramp = 0.0f;
        right_motor_ramp = 0.0f;
        wheel_speed = 0.0f;
        ss_smooth_l = 0.0f;
        ss_smooth_r = 0.0f;
        cruise_active = false;
        cruise_speed = 0.0f;
        cruise_wait_r2_release = false;
        cruise_wait_l2_release = false;
    };

    while (g_running.load()) {
        const auto now = std::chrono::steady_clock::now();
        const float dt = std::clamp(
            std::chrono::duration<float>(now - prev_tick).count(), 0.0f, 0.1f);
        prev_tick = now;

        const InputMode mode = input_mode.load();
        if (mode != last_mode) {
            zero_motion();
            last_mode = mode;
        }

        // Process E-STOP toggles
        const int cur_estop_seq = estop_toggle_seq.load();
        if (cur_estop_seq != last_estop_seq) {
            const int toggles = cur_estop_seq - last_estop_seq;
            last_estop_seq = cur_estop_seq;
            for (int i = 0; i < toggles; ++i) {
                controls_enabled = !controls_enabled;
            }
            if (!controls_enabled) {
                zero_motion();
                ds_feedback.pulse(190, 150, 140);
                std::cout << "--- E-STOP AKTIVOVÁN ---\n";
            } else {
                std::cout << "--- OVLÁDÁNÍ AKTIVNÍ ---\n";
            }
        }

        // Process DualSense R1 cruise toggles
        const int cur_cruise_seq = cruise_toggle_seq.load();
        if (cur_cruise_seq != last_cruise_seq) {
            const int toggles = cur_cruise_seq - last_cruise_seq;
            last_cruise_seq = cur_cruise_seq;
            if (mode == InputMode::DualSense && controls_enabled) {
                for (int i = 0; i < toggles; ++i) {
                    if (cruise_active) {
                        cruise_active = false;
                        cruise_speed = 0.0f;
                        cruise_wait_r2_release = false;
                        cruise_wait_l2_release = false;
                        ds_feedback.pulse(75, 115, 90);
                        std::cout << "[TEMPOMAT] VYPNUT (R1)\n";
                    } else {
                        const float r2 = ds_right_trigger.load();
                        const float l2 = ds_left_trigger.load();
                        cruise_speed = std::clamp((r2 - l2) * PWM_MAX, -255.0f, 255.0f);
                        cruise_active = true;
                        cruise_wait_r2_release = (r2 > CRUISE_TRIGGER_RELEASE_THRESHOLD);
                        cruise_wait_l2_release = (l2 > CRUISE_TRIGGER_RELEASE_THRESHOLD);
                        dpad_speed = 0.0f;
                        ds_feedback.pulse(75, 115, 90);
                        std::cout << "[TEMPOMAT] ZAPNUT: " << static_cast<int>(cruise_speed) << " PWM\n";
                    }
                }
            }
        }

        int final_left = 0;
        int final_right = 0;

        const float lx = ds_left_stick_x.load();
        const float ly = ds_left_stick_y.load();
        const float ry = ds_right_stick_y.load();
        const float l2 = ds_left_trigger.load();
        const float r2 = ds_right_trigger.load();
        const bool d_up = ds_dpad_up.load();
        const bool d_dn = ds_dpad_down.load();
        const bool d_lt = ds_dpad_left.load();
        const bool d_rt = ds_dpad_right.load();

        if (!controls_enabled) {
            zero_motion();
            final_left = 0;
            final_right = 0;
        } else if (mode == InputMode::Mouse) {
            const int wdelta = ss_pending_wheel_delta.exchange(0);
            if (wdelta != 0) {
                wheel_speed = std::clamp(wheel_speed + WHEEL_STEP * wdelta, -255.0f, 255.0f);
                last_wheel_time = now;
            }
            const float since_wheel = std::chrono::duration<float>(now - last_wheel_time).count();
            if (wheel_speed != 0.0f && since_wheel > WHEEL_DECAY_DELAY_S) {
                const float decay = WHEEL_DECAY_RATE_PWM_S * dt;
                wheel_speed = (wheel_speed > 0.0f) ? std::max(0.0f, wheel_speed - decay)
                                                   : std::min(0.0f, wheel_speed + decay);
            }

            const float raw_l = shape_superstrike_pressure(ss_left_raw.load());
            const float raw_r = shape_superstrike_pressure(ss_right_raw.load());
            const float alpha_l = (raw_l == 0.0f) ? SUPERSTRIKE_RELEASE_SMOOTHING : SUPERSTRIKE_SMOOTHING;
            const float alpha_r = (raw_r == 0.0f) ? SUPERSTRIKE_RELEASE_SMOOTHING : SUPERSTRIKE_SMOOTHING;
            ss_smooth_l += (raw_l - ss_smooth_l) * alpha_l;
            ss_smooth_r += (raw_r - ss_smooth_r) * alpha_r;
            if (raw_l == 0.0f && ss_smooth_l < 0.004f) ss_smooth_l = 0.0f;
            if (raw_r == 0.0f && ss_smooth_r < 0.004f) ss_smooth_r = 0.0f;

            if (std::abs(wheel_speed) > WHEEL_ACTIVE_THRESHOLD) {
                final_left = static_cast<int>(wheel_speed);
                final_right = static_cast<int>(wheel_speed);
            } else {
                const int dir = ss_direction_mode.load();
                final_left = static_cast<int>(ss_smooth_l * PWM_MAX) * dir;
                final_right = static_cast<int>(ss_smooth_r * PWM_MAX) * dir;
            }
        } else if (mode == InputMode::DualSense) {
            const int dpad_dir = static_cast<int>(d_up) - static_cast<int>(d_dn);

            if (cruise_active) {
                if (cruise_wait_r2_release && r2 <= CRUISE_TRIGGER_RELEASE_THRESHOLD) {
                    cruise_wait_r2_release = false;
                }
                if (cruise_wait_l2_release && l2 <= CRUISE_TRIGGER_RELEASE_THRESHOLD) {
                    cruise_wait_l2_release = false;
                }
                const bool r2_override = !cruise_wait_r2_release && (r2 >= CRUISE_TRIGGER_CANCEL_THRESHOLD);
                const bool l2_override = !cruise_wait_l2_release && (l2 >= CRUISE_TRIGGER_CANCEL_THRESHOLD);
                if (r2_override || l2_override) {
                    cruise_active = false;
                    cruise_speed = 0.0f;
                    cruise_wait_r2_release = false;
                    cruise_wait_l2_release = false;
                    ds_feedback.pulse(75, 115, 90);
                    std::cout << "[TEMPOMAT] VYPNUT (manuální R2/L2)\n";
                }
            }

            int target_left = 0;
            int target_right = 0;

            if (cruise_active) {
                dpad_speed = 0.0f;
                if (dpad_dir != 0) {
                    cruise_speed = std::clamp(
                        cruise_speed + dpad_dir * CRUISE_ADJUST_RATE * dt, -255.0f, 255.0f);
                }
                const float brake = std::clamp(-ly, 0.0f, 1.0f);
                const float effective_cruise = cruise_speed * (1.0f - brake);
                auto [tl, tr] = apply_proportional_steering(effective_cruise, lx);
                target_left = tl;
                target_right = tr;
            } else {
                const float target_dpad = dpad_dir * static_cast<float>(PWM_MAX);
                const float max_dpad = DPAD_ACCELERATION * dt;
                if (dpad_speed < target_dpad) dpad_speed = std::min(target_dpad, dpad_speed + max_dpad);
                else if (dpad_speed > target_dpad) dpad_speed = std::max(target_dpad, dpad_speed - max_dpad);

                const float throttle_trig = r2 - l2;
                const float turn = static_cast<float>(static_cast<int>(d_rt) - static_cast<int>(d_lt));
                if (dpad_dir != 0 || std::abs(dpad_speed) > 0.0f) {
                    const float base = dpad_speed + throttle_trig * PWM_MAX;
                    auto [sl, sr] = apply_proportional_steering(base, lx);
                    target_left = static_cast<int>(std::clamp(sl + turn * PWM_MAX, -255.0f, 255.0f));
                    target_right = static_cast<int>(std::clamp(sr - turn * PWM_MAX, -255.0f, 255.0f));
                } else {
                    auto [tl, tr] = apply_proportional_steering(throttle_trig * PWM_MAX, lx);
                    target_left = static_cast<int>(std::clamp(tl + (ly + turn) * PWM_MAX, -255.0f, 255.0f));
                    target_right = static_cast<int>(std::clamp(tr + (ry - turn) * PWM_MAX, -255.0f, 255.0f));
                }
            }

            const float max_motor = MOTOR_ACCELERATION * dt;
            left_motor_ramp += std::clamp(target_left - left_motor_ramp, -max_motor, max_motor);
            right_motor_ramp += std::clamp(target_right - right_motor_ramp, -max_motor, max_motor);
            final_left = static_cast<int>(left_motor_ramp);
            final_right = static_cast<int>(right_motor_ramp);
        }

        left_motor.setSpeed(final_left);
        right_motor.setSpeed(final_right);

        // Update DualSense LEDs, adaptive triggers & rumble
        if (mode == InputMode::DualSense) {
            const int eff_l = left_motor.getSpeed();
            const int eff_r = right_motor.getSpeed();
            const int l_abs = std::abs(eff_l);
            const int r_abs = std::abs(eff_r);
            const float avg_frac = std::clamp((l_abs + r_abs) / (2.0f * PWM_MAX), 0.0f, 1.0f);

            std::array<std::uint8_t, 3> rgb{0, 18, 0};
            std::uint8_t player_mask = 0;
            TriggerFx r2_fx = TRIGGER_FX_MANUAL_R2;
            TriggerFx l2_fx = TRIGGER_FX_MANUAL_L2;
            std::uint8_t rl = 0;
            std::uint8_t rr = 0;

            if (!controls_enabled) {
                rgb = {0, 15, 89};  // #000F59
                player_mask = 0;
                r2_fx = TRIGGER_FX_ESTOP_LOCK;
                l2_fx = TRIGGER_FX_ESTOP_LOCK;

                const bool dpad_any = d_up || d_dn || d_lt || d_rt;
                const float attempt = std::max({r2, l2, std::abs(ly), std::abs(ry), dpad_any ? 1.0f : 0.0f});
                if (attempt >= 0.08f) {
                    rl = static_cast<std::uint8_t>(std::clamp(160.0f + 95.0f * attempt, 160.0f, 255.0f));
                    rr = static_cast<std::uint8_t>(std::clamp(130.0f + 90.0f * attempt, 130.0f, 220.0f));
                }
            } else {
                if (l_abs >= 140) player_mask |= (1u << 0);
                if (l_abs >= MOTOR_DEADBAND) player_mask |= (1u << 1);
                if (cruise_active) player_mask |= (1u << 2);
                if (r_abs >= MOTOR_DEADBAND) player_mask |= (1u << 3);
                if (r_abs >= 140) player_mask |= (1u << 4);

                if (cruise_active) {
                    r2_fx = TRIGGER_FX_CRUISE_DETENT;
                    l2_fx = TRIGGER_FX_CRUISE_DETENT;
                    const float brake = std::clamp(-ly, 0.0f, 1.0f);
                    const float r_f = 160.0f + (255.0f - 160.0f) * brake;
                    const float g_f = 110.0f * brake;
                    const float b_f = 255.0f * (1.0f - brake);
                    const float scale = (brake > 0.0f) ? 1.0f : (0.45f + 0.55f * avg_frac);
                    rgb = {
                        static_cast<std::uint8_t>(std::clamp(r_f * scale, 0.0f, 255.0f)),
                        static_cast<std::uint8_t>(std::clamp(g_f * scale, 0.0f, 255.0f)),
                        static_cast<std::uint8_t>(std::clamp(b_f * scale, 0.0f, 255.0f)),
                    };
                    if (std::abs(cruise_speed) >= MOTOR_DEADBAND && brake > 0.0f) {
                        rl = static_cast<std::uint8_t>(55.0f * brake);
                        rr = static_cast<std::uint8_t>(80.0f * brake);
                    }
                } else if (l_abs > 0 || r_abs > 0) {
                    const auto intensity = static_cast<std::uint8_t>(
                        std::clamp(35.0f + 220.0f * avg_frac, 35.0f, 255.0f));
                    if (eff_l + eff_r < 0) rgb = {intensity, 0, 0};
                    else rgb = {0, intensity, 0};
                }
            }

            ds_feedback.apply(r2_fx, l2_fx, rl, rr, rgb, player_mask);
        }

        if (dry_run && (now - last_status_log >= std::chrono::milliseconds(500))) {
            last_status_log = now;
            std::cout << "[DRY-RUN] L=" << left_motor.getSpeed()
                      << " R=" << right_motor.getSpeed()
                      << " | E-STOP=" << (controls_enabled ? "OFF" : "ON")
                      << " | CRUISE=" << (cruise_active ? std::to_string(static_cast<int>(cruise_speed)) : "OFF")
                      << " | ENC L=" << left_encoder.getCount()
                      << " R=" << right_encoder.getCount() << "\n";
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    std::cout << "\n[SHUTDOWN] Zastavuji motory a uvolňuji ovladač...\n";
    left_motor.shutdown();
    right_motor.shutdown();
    left_encoder.shutdown();
    right_encoder.shutdown();
    ds_feedback.resetOff();
    if (input.joinable()) input.join();
    return 0;
}
