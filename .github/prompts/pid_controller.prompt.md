---
name: "PID Controller & Track Sync"
description: "Generuje nebo ladí diskrétní PID regulátor pro synchronizaci otáček pásů Wall-E v C++"
---

Při implementaci nebo úpravě PID regulátoru v `4. Ročník/cpp_core/` dodržuj tato pravidla:

1. **Struktura regulátoru:**
   - Vstup: Žádaná rychlost (setpoint v pulsech/s z enkodérů) a aktuální naměřená rychlost levého a pravého pásu.
   - Výstup: Upravená střída PWM (-255 až +255) pro třídu `Motor`.
   - Parametry: $K_p$, $K_i$, $K_d$ s možností dynamického nastavení.

2. **Inženýrské ošetření (Robustnost):**
   - **Anti-windup ochrana:** Zabraň saturaci integrální složky při zablokování motoru nebo při maximálním PWM.
   - **Mrtvá zóna (Deadband):** Ošetři minimální PWM střídu (cca 25–35), pod kterou motor Pololu 37D nepřemůže tření převodovky a jen píská.
   - **Pevný časový krok:** Časový rozdíl $dt$ počítej přesně pomocí `std::chrono::duration<float>` z `steady_clock`.

3. **Výstup:**
   - Poskytni rovnou funkční kód v C++17 bez balastu, připravený k integraci do `main.cpp`.
