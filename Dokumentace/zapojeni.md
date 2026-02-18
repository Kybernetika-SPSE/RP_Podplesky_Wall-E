# Plán Zapojení Elektroniky (Wiring Plan)

## 1. Power Distribution (Napájení)

### Okruh A (Pohon / High Power)
*   **Zdroj:** Li-Po 3S 3500mAh (11.1V - 12.6V)
*   **Ochrana:**
    *   Nožová pojistka (Automotive fuse) - 30A (pro oba motory dohromady)
    *   Li-Po Alarm (BX100) na servisní konektor
*   **Rozvod:**
    *   Vstup do Driveru L (BTS7960B): VCC / GND
    *   Vstup do Driveru R (BTS7960B): VCC / GND

### Okruh B (Logic & Compute)
*   **Zdroj:** Li-Po 3S 3500mAh (11.1V - 12.6V)
*   **Ochrana:**
    *   Nožová pojistka - 5A nebo 10A
    *   Li-Po Alarm (BX100)
*   **Rozvod:**
    *   DC Jack 5.5x2.5mm -> Jetson Orin Nano / Nano (Barrel Jack)
*   **Poznámka:** Země (GND) obou okruhů **MUSÍ BÝT SPOJENÉ** pro funkční signály PWM, pokud nepoužíváme opto-izolaci na driverech (BTS7960 má opto-izolaci na vstupech, ale často se spojují země pro jistotu, i když u BTS7960 to není striktně nutné, pokud se napájí VCC logic z Jetsonu. Zkontrolujeme datasheet driveru).

## 2. Motor Control (Jetson -> BTS7960)

### Návrh s PCA9685 (Doporučeno):
*   **Jetson Pin 3 (SDA) -> PCA9685 SDA**
*   **Jetson Pin 5 (SCL) -> PCA9685 SCL**
*   **Jetson 3.3V -> PCA9685 VCC**
*   **Jetson GND -> PCA9685 GND**
*   **PCA9685 Output 0 -> Motor L_PWM_FWD**
*   **PCA9685 Output 1 -> Motor L_PWM_BWD**
*   **PCA9685 Output 2 -> Motor R_PWM_FWD**
*   **PCA9685 Output 3 -> Motor R_PWM_BWD**

## 3. Diagram logiky
(Viz soubor `vizualizace_zapojeni.md`)

## Další Kroky
1.  Potvrdit použití PCA9685 (velmi doporučeno pro Jetson).
2.  Navrhnout držák pro elektroniku (Jetson + Drivery + Baterie).
