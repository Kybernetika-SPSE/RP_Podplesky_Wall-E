#include "encoder.h"

#include <iostream>

#include <linux/gpio.h>
#include <poll.h>
#include <unistd.h>

Encoder::Encoder(std::string enc_label, std::string line_a, std::string line_b)
    : label(std::move(enc_label)),
      lineNameA(std::move(line_a)),
      lineNameB(std::move(line_b)) {}

Encoder::~Encoder() {
    shutdown();
}

bool Encoder::begin(bool dry_run) {
    shutdown();
    count.store(0);

    if (dry_run) {
        std::cout << "[ENCODER " << label << "] DRY-RUN režim (A=" << lineNameA
                  << ", B=" << lineNameB << ").\n";
        return true;
    }

    const bool okA = gpioEventA.openBothEdges(lineNameA, "walle_" + label + "_enca");
    const bool okB = gpioInputB.open(lineNameB, "walle_" + label + "_encb");
    if (!okA || !okB) {
        std::cerr << "[ENCODER " << label << "] CHYBA: Nepodařilo se otevřít GPIO linky "
                  << lineNameA << " / " << lineNameB << ".\n";
        gpioEventA.closeLine();
        gpioInputB.closeLine();
        return false;
    }

    running.store(true);
    isrThread = std::thread(&Encoder::interruptRoutine, this);
    std::cout << "[ENCODER " << label << "] Aktivní (A=" << lineNameA
              << ", B=" << lineNameB << ").\n";
    return true;
}

void Encoder::shutdown() {
    running.store(false);
    if (isrThread.joinable()) {
        isrThread.join();
    }
    gpioEventA.closeLine();
    gpioInputB.closeLine();
}

void Encoder::interruptRoutine() {
    const int fdA = gpioEventA.getFd();
    if (fdA < 0) return;

    struct pollfd pfd {};
    pfd.fd = fdA;
    pfd.events = POLLIN;

    while (running.load()) {
        const int ret = ::poll(&pfd, 1, 100);
        if (ret > 0 && (pfd.revents & POLLIN)) {
            struct gpioevent_data ev {};
            const ssize_t n = ::read(fdA, &ev, sizeof(ev));
            if (n == static_cast<ssize_t>(sizeof(ev))) {
                const int valA = (ev.id == GPIOEVENT_EVENT_RISING_EDGE) ? 1 : 0;
                const int valB = gpioInputB.getValue();
                if (valB >= 0) {
                    if (valA == valB) {
                        count.fetch_add(1);
                    } else {
                        count.fetch_sub(1);
                    }
                }
            }
        }
    }
}

long long Encoder::getCount() const {
    return count.load();
}

void Encoder::clearCount() {
    count.store(0);
}
