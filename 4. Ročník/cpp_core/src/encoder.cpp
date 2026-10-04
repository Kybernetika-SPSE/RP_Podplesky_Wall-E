#include "encoder.h"
#include <iostream>
#include <fstream>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>

Encoder::Encoder(int pin_a, int pin_b) : pinA(pin_a), pinB(pin_b), count(0), running(false) {
    gpioPathA = "/sys/class/gpio/gpio" + std::to_string(pinA);
    gpioPathB = "/sys/class/gpio/gpio" + std::to_string(pinB);
}

Encoder::~Encoder() {
    running = false;
    if (isrThread.joinable()) {
        isrThread.join();
    }
}

bool Encoder::writeSysfs(const std::string& path, const std::string& value) {
    std::ofstream file(path);
    if (!file.is_open()) return false;
    file << value;
    return true;
}

std::string Encoder::readSysfs(const std::string& path) {
    std::ifstream file(path);
    std::string value;
    if (file.is_open()) file >> value;
    return value;
}

void Encoder::begin() {
    // Export GPIOs
    std::ofstream exportFile("/sys/class/gpio/export");
    if (exportFile.is_open()) {
        exportFile << pinA << std::endl;
        exportFile << pinB << std::endl;
    }

    // Set directions to 'in'
    writeSysfs(gpioPathA + "/direction", "in");
    writeSysfs(gpioPathB + "/direction", "in");

    // Set edge detection for pin A to 'both' (sledování náběžné i sestupné hrany)
    writeSysfs(gpioPathA + "/edge", "both");

    running = true;
    count = 0;
    
    // Spuštění vlákna, které zachytává hardwarová přerušení (interrupts)
    isrThread = std::thread(&Encoder::interruptRoutine, this);
}

void Encoder::interruptRoutine() {
    std::string valuePathA = gpioPathA + "/value";
    std::string valuePathB = gpioPathB + "/value";

    int fdA = open(valuePathA.c_str(), O_RDONLY);
    if (fdA < 0) return;

    struct pollfd pfd;
    pfd.fd = fdA;
    pfd.events = POLLPRI; // Čekáme na hardwarové přerušení

    char buf[2];
    
    // Vyčištění bufferu před spuštěním
    lseek(fdA, 0, SEEK_SET);
    read(fdA, buf, sizeof(buf));

    while (running) {
        int ret = poll(&pfd, 1, 100); // Timeout 100ms pro možnost ukončení vlákna
        
        if (ret > 0 && (pfd.revents & POLLPRI)) {
            // Došlo ke změně stavu na pinu A!
            lseek(fdA, 0, SEEK_SET);
            read(fdA, buf, sizeof(buf));
            char valA = buf[0];

            // Přečteme stav pinu B pro zjištění směru otáčení
            std::string valB_str = readSysfs(valuePathB);
            if (!valB_str.empty()) {
                char valB = valB_str[0];
                
                // Kvadraturní logika: porovnání hrany A se stavem B
                if (valA == valB) {
                    count++;
                } else {
                    count--;
                }
            }
        }
    }
    close(fdA);
}

long long Encoder::getCount() {
    return count.load();
}

void Encoder::clearCount() {
    count.store(0);
}
