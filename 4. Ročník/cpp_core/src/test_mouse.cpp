#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <dirent.h>
#include <cstring>

// Nalezení vstupních zařízení myši v systému
std::string find_mouse_device() {
    std::ifstream devices("/proc/bus/input/devices");
    if (!devices.is_open()) return "";

    std::string line;
    std::string current_name;
    std::string current_handlers;

    while (std::getline(devices, line)) {
        if (line.rfind("N: Name=", 0) == 0) {
            current_name = line.substr(9);
        } else if (line.rfind("H: Handlers=", 0) == 0) {
            current_handlers = line.substr(12);
            if (current_handlers.find("mouse") != std::string::npos || 
                current_name.find("Mouse") != std::string::npos ||
                current_name.find("Logitech") != std::string::npos) {
                // Hledáme eventX
                std::istringstream iss(current_handlers);
                std::string handler;
                while (iss >> handler) {
                    if (handler.rfind("event", 0) == 0) {
                        return "/dev/input/" + handler;
                    }
                }
            }
        }
    }
    return "";
}

const char* button_name(int code) {
    switch (code) {
        case BTN_LEFT: return "LEVÉ TLAČÍTKO";
        case BTN_RIGHT: return "PRAVÉ TLAČÍTKO";
        case BTN_MIDDLE: return "KOLEČKO (STISK)";
        case BTN_SIDE: return "BOČNÍ TLAČÍTKO (SIDE / Zpět)";
        case BTN_EXTRA: return "BOČNÍ TLAČÍTKO (EXTRA / Vpřed)";
        case BTN_FORWARD: return "TLAČÍTKO VPŘED";
        case BTN_BACK: return "TLAČÍTKO ZPĚT";
        default: return "NEZNÁMÉ TLAČÍTKO";
    }
}

int main(int argc, char** argv) {
    std::string dev_path;

    if (argc > 1) {
        dev_path = argv[1];
    } else {
        std::cout << "[INFO] Automatické hledání myši v /proc/bus/input/devices...\n";
        dev_path = find_mouse_device();
        if (dev_path.empty()) {
            std::cout << "[VAROVÁNÍ] Myš nebyla automaticky detekována. Zkouším /dev/input/event0\n";
            dev_path = "/dev/input/event0";
        }
    }

    std::cout << "[INFO] Otevírám zařízení: " << dev_path << "\n";
    int fd = open(dev_path.c_str(), O_RDONLY);
    if (fd < 0) {
        std::cerr << "[CHYBA] Nelze otevřít " << dev_path << "!\n";
        std::cerr << "        Spusťte program jako root (sudo ./" << argv[0] << ") nebo přidejte uživatele do skupiny 'input':\n";
        std::cerr << "        sudo usermod -a -G input $USER\n";
        return 1;
    }

    char name[256] = "Neznámé zařízení";
    ioctl(fd, EVIOCGNAME(sizeof(name)), name);
    std::cout << "[INFO] Připojeno: " << name << "\n";
    std::cout << "========================================================\n";
    std::cout << "Testování myši spuštěno. Pro ukončení stiskněte Ctrl+C.\n";
    std::cout << "========================================================\n\n";

    struct input_event ev;
    int acc_x = 0;
    int acc_y = 0;

    while (read(fd, &ev, sizeof(ev)) > 0) {
        if (ev.type == EV_KEY) {
            std::string state = (ev.value == 1) ? "STISKNUTO" : ((ev.value == 0) ? "UVOLNĚNO" : "DRŽENO");
            std::cout << "[TLAČÍTKO] Kód: " << std::setw(3) << ev.code 
                      << " (" << button_name(ev.code) << ") -> " << state << "\n";
        } else if (ev.type == EV_REL) {
            if (ev.code == REL_X) {
                acc_x += ev.value;
                std::cout << "[POHYB] Relativní X: " << std::setw(4) << ev.value 
                          << " | Kumulativní X: " << acc_x << "\n";
            } else if (ev.code == REL_Y) {
                acc_y += ev.value;
                std::cout << "[POHYB] Relativní Y: " << std::setw(4) << ev.value 
                          << " | Kumulativní Y: " << acc_y << "\n";
            } else if (ev.code == REL_WHEEL) {
                std::string dir = (ev.value > 0) ? "NAHORU (+)" : "DOLŮ (-)";
                std::cout << "[KOLEČKO] Krok: " << ev.value << " (" << dir << ")\n";
            } else if (ev.code == REL_HWHEEL) {
                std::cout << "[HORIZ. KOLEČKO] Krok: " << ev.value << "\n";
            }
        }
    }

    close(fd);
    return 0;
}
