#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <cmath>
#include <hidapi/hidapi.h>
#include <linux/input.h>
#include <fcntl.h>
#include <unistd.h>
#include <algorithm>
#include "motor.h"

// Globální sdílené proměnné pro komunikaci mezi vlákny
std::atomic<float> left_pressure(0.0f);
std::atomic<float> right_pressure(0.0f);
std::atomic<int> direction_mode(1); // 1 = Dopředu, -1 = Dozadu
std::atomic<float> wheel_speed(0.0f); // 0 až 255.0
std::atomic<std::chrono::steady_clock::time_point> last_wheel_time;

// Globální přepínač zapnuto/vypnuto
std::atomic<bool> controls_enabled(true); 

// Vlákno pro vyčítání 10bitového tlaku přes Logitech HID++
void hidpp_thread() {
    hid_init();
    hid_device* dev = hid_open(0x046D, 0xC54D, NULL); // Zkusí Wireless přijímač
    if (!dev) dev = hid_open(0x046D, 0xC0A8, NULL); // Zkusí kabel
    
    if (!dev) {
        std::cerr << "Chyba: Nepodařilo se připojit k analogové myši přes HIDAPI.\n";
        return;
    }
    hid_set_nonblocking(dev, 1);
    
    auto last_lease = std::chrono::steady_clock::now() - std::chrono::seconds(10);
    unsigned char buf[64];
    
    while (true) {
        auto now = std::chrono::steady_clock::now();
        // Obnovování lease každé 2 vteřiny
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_lease).count() >= 2) {
            unsigned char report[20] = {0x11, 0xFF, 0x0C, 0x38, 0x02, 0x08}; 
            hid_write(dev, report, sizeof(report));
            report[1] = 0x01; 
            hid_write(dev, report, sizeof(report));
            last_lease = now;
        }

        int res = hid_read(dev, buf, sizeof(buf));
        if (res >= 20 && buf[0] == 0x11 && buf[2] == 0x0C && buf[3] == 0x10) {
            int left_raw = ((buf[4] << 8) | buf[5]) >> 6;
            int right_raw = ((buf[6] << 8) | buf[7]) >> 6;
            
            float lc = std::max(0, std::min(1000, left_raw - 300)) / 700.0f;
            float rc = std::max(0, std::min(1000, right_raw - 300)) / 700.0f;
            
            left_pressure.store(lc);
            right_pressure.store(rc);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    hid_close(dev);
    hid_exit();
}

// Vlákno pro standardní tlačítka a kolečko z Linux Event rozhraní
void evdev_thread() {
    const char* dev_path = "/dev/input/event0"; // TODO: UPRAVIT NA VÁŠ SYSTÉM!
    int fd = open(dev_path, O_RDONLY | O_NONBLOCK);
    if (fd < 0) return;
    
    struct input_event ev;
    while (true) {
        if (read(fd, &ev, sizeof(ev)) > 0) {
            // Scrollování kolečkem
            if (ev.type == EV_REL && ev.code == REL_WHEEL) {
                float cur = wheel_speed.load();
                if (ev.value > 0) cur = std::min(255.0f, cur + 30.0f); // Dopředu
                if (ev.value < 0) cur = std::max(-255.0f, cur - 30.0f); // Dozadu
                wheel_speed.store(cur);
                last_wheel_time.store(std::chrono::steady_clock::now());
            }
            
            // Tlačítka myši
            if (ev.type == EV_KEY && ev.value == 1) { // 1 = Stisknuto (rising edge)
                // Stisknutí kolečka myši - Zastavení / Spuštění ovládání
                if (ev.code == BTN_MIDDLE) {
                    controls_enabled.store(!controls_enabled.load());
                    if (controls_enabled.load()) {
                        std::cout << "--- OVLADANI ZAPNUTO ---\n";
                    } else {
                        std::cout << "--- OVLADANI VYPNUTO (STOP) ---\n";
                    }
                }
                
                // Boční tlačítka pro směr pásů
                if (ev.code == BTN_SIDE) direction_mode.store(-1); // Zadní = Dozadu
                if (ev.code == BTN_EXTRA) direction_mode.store(1); // Přední = Dopředu
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
    close(fd);
}

int main() {
    std::cout << "Inicializace robota Wall-E (Jetson C++ Port)...\n";
    std::cout << "Ovladani je defaultne ZAPNUTE. Pro nouzove zastaveni kliknete koleckem.\n";
    
    Motor left_motor(0, 1); 
    Motor right_motor(2, 3);
    left_motor.begin();
    right_motor.begin();

    std::thread t_hid(hidpp_thread);
    std::thread t_ev(evdev_thread);
    
    // Mrtvá zóna (Deadzone) - pod tuto hodnotu stisku motory nereagují
    const float DEADZONE = 0.08f; // 8% stisku
    
    while (true) {
        // Zastavení všech pohybů, pokud je ovládání vypnuté (stisknuto kolečko)
        if (!controls_enabled.load()) {
            wheel_speed.store(0.0f); // Vynulujeme i kolečko
            left_motor.setSpeed(0);
            right_motor.setSpeed(0);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            continue;
        }

        float cur_wheel = wheel_speed.load();
        auto now = std::chrono::steady_clock::now();
        auto lw_time = last_wheel_time.load();
        auto diff_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - lw_time).count();

        // Plynulé zpomalení kolečka (po 0.5s bez točení)
        if (diff_ms > 500) {
            if (cur_wheel > 0) cur_wheel = std::max(0.0f, cur_wheel - 5.0f);
            if (cur_wheel < 0) cur_wheel = std::min(0.0f, cur_wheel + 5.0f);
            wheel_speed.store(cur_wheel);
        }

        int final_left = 0;
        int final_right = 0;

        // Kolečko přebíjí analogová tlačítka
        if (std::abs(cur_wheel) > 5.0f) {
            final_left = static_cast<int>(cur_wheel);
            final_right = static_cast<int>(cur_wheel);
        } else {
            float l_pres = left_pressure.load();
            float r_pres = right_pressure.load();

            // Aplikace mrtvé zóny (Blackzone/Deadzone) pro odstranění nechtěných lehkých dotyků
            if (l_pres < DEADZONE) l_pres = 0.0f;
            if (r_pres < DEADZONE) r_pres = 0.0f;

            int dir = direction_mode.load();
            final_left = static_cast<int>(l_pres * 255.0f) * dir;
            final_right = static_cast<int>(r_pres * 255.0f) * dir;
        }

        left_motor.setSpeed(final_left);
        right_motor.setSpeed(final_right);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    t_hid.join();
    t_ev.join();
    return 0;
}
