# WALL-E Off-road Replica (Ročníkový Projekt)

Toto je repozitář pro stavbu funkční repliky robota WALL-E. Projekt klade důraz na robustní mechanickou konstrukci a moderní elektroniku.


## 🛠️ O Projektu
Cílem je vytvořit **Repliku robota Wall-E**, pomocí 3D tisku a obráběného kovu
*   **Šasi:** Ocelové pláty (Laser cut) + 3D tisk
*   **Podvozek:** Pásový system s nezávislým odpružením 
*   **Mozek:** Nvidia Jetson Orin Nano

## ⚙️ Hardware Specifikace
*   **Pohon:** 2x Motor JGB37-550 (12V, 160 RPM)
*   **Drivery:** 2x BTS7960B (43A H-Bridge)
*   **PWM Řízení:** PCA9685 (I2C)
*   **Napájení:** Duální systém (2x Li-Po 3S 3500mAh)
*   **Ovládání:** Bezdrátová myš / Gamepad

## 💻 Software
Software běží na **Nvidia Jetson** v jazyce **C++**.
*   Využívá **I2C** pro komunikaci s PCA9685.
*   Čte vstupy z `/dev/input/eventX` (myš/klávesnice).
*   Implementuje **Smooth Start/Stop** pro plynulý rozjezd pásů.
*   Zdrojové kódy naleznete ve složce [`code/`](./code).

## 📂 Struktura Repozitáře
*   [`code/`](./code) - Zdrojové kódy ovládacího softwaru (C++, CMake).
*   [`Dokumentace/`](./Dokumentace) - Technické specifikace, schémata zapojení a plány.
*   [`MODEL/`](./MODEL) - 3D Modely (Inventor/STEP) a podklady pro tisk.
*   [`komponenty.md`](./komponenty.md) - Seznam použitých dílů (BOM).


---
**Status:** 🚧 Ve vývoji (Mechanika 60%, Elektronika 40%, Software 20%)
