# Wall-E: Přehled projektu, architektury a plánů

Tento dokument slouží jako centrální přehled technického řešení, aktuálního stavu prací a plánovaných kroků pro maturitní práci **Výroba modelu robota Wall-E (2026/2027)** na VOŠ a SPŠE Plzeň.

---

## 1. Cíl práce
* **Zadání:** Návrh a konstrukce funkčního konceptu modelu robota Wall-E s pásovým podvozkem, který bude dálkově ovládán pomocí bezdrátové myši.
* **Mozek robota:** NVIDIA Jetson Orin Nano Developer Kit (Ubuntu Linux / JetPack).
* **Vedoucí práce:** Ing. Pavel Jedlička
* **Termín odevzdání:** 9. dubna 2027

---

## 2. Hardwarová a elektrická architektura ("Jak je to vymyšlené")

### A. Napájecí strom (Power Distribution)
* **Zdroj energie:** 3S LiPo akumulátor (11.1 V nominál, 12.6 V plné nabití).
* **Jištění a spínání:**
  * Hlavní kolébkový spínač dimenzovaný na **20 A**.
  * Nožová pojistka **20 A (žlutá)** v pojistkovém pouzdru na kladné větvi.
* **Silová větev (+12 V):**
  * Přímé napájení výkonových H-můstků **BTS7960** pro stejnosměrné motory podvozku.
  * Přímé napájení **NVIDIA Jetson Orin Nano** přes DC barrel jack (Jetson vyžaduje 9–20 V, 12 V je ideální).
* **Logická větev (+5 V):**
  * Snižující DC-DC měnič **LM2596** (nastavený trimrem přesně na 5.0 V).
  * Napájí výhradně logiku obou driverů BTS7960 (piny VCC na logické liště) a Hallovy senzory enkodérů motorů.
  * **Pravidlo:** 5V větev z LM2596 se **nikdy nezapojuje** do GPIO Jetsonu! Jetson se napájí výhradně ze svého DC konektoru.
* **Filtrační ochrana:**
  * Na desce je osazen elektrolytický kondenzátor **1000 µF / 35 V (Low-ESR)** přímo mezi +12 V a GND pro vyhlazení napěťových špiček generovaných motory při prudkém brzdění a akceleraci.

### B. Vlastní řídicí deska (Wall-E Power Hub v1.0 PCB)
* **Návrh:** Dvouvrstvá DPS navržená v KiCadu.
* **Šířky cest:**
  * Motorová silovina (+12 V, GND k BTS): **2.5 mm**
  * Napájecí cesty (12 V do LM2596, 5 V z LM2596, LED): **0.8 mm**
  * Datové signály (PWM, EN, signály enkodérů, I2C): **0.4 mm**
* **Měděné plochy (Copper Pour):**
  * Oboustranné vylití zemní plochy (GND) na vrstvách `F.Cu` i `B.Cu` spojené prokovy pro minimální impedanci a stínění proti rušení.
* **Konektory:**
  * `J11`: 40-pinová lišta pro propojení s GPIO konektorem Jetsonu.
  * `J8` / `J3`: 8-pinové lišty pro logické signály driverů levého a pravého pásu.
  * `J18` / `J17`: Samostatné 4-pinové konektory pro vyvedení napájení (5V, GND) a signálů (A, B) z enkodérů motorů.
  * Šroubovací svorkovnice pro silové vstupy a výstupy (+12 V a GND).
* **Montáž:**
  * Otvory M2 (`MountingHole_2.2mm_M2`) odpovídající distančním sloupkům Jetsonu.

### C. Mechanika, chlazení a 3D tisk (Box 2 – Engine Room)
* **Koncepce:** Větrný aerodynamický tunel v podvozku robota.
* **Uspořádání:**
  * Modul LM2596 je umístěn vertikálně pod deskou Power Hubu.
  * Ventilátor **40×40×10 mm (12 V)** žene vzduch skrz tunel: ofukuje LM2596 pod deskou a pokračuje přes chladicí žebra obou driverů BTS7960 umístěných po stranách.
* **Materiál tisku:**
  * Primárně **ASA** (tepelná odolnost do 95–100 °C, odolá teplu z Jetsonu a driverů).
  * Záložní: **PETG** (odolnost do 80 °C).
  * **PLA je zakázáno** (při 55 °C měkne a deformuje se).

---

## 3. Stav softwaru (Software Core)

Projektový software je organizován v repozitáři ve složce `4. Ročník/cpp_core` (standard C++17, build přes CMake).

* **Testování myši (dokončeno):**
  * `test_mouse.py`: Standalone Python skript s nulovými závislostmi, který čte `/proc/bus/input/devices`, automaticky najde myš a v reálném čase loguje X/Y pohyb, směr kolečka i stisky tlačítek.
  * `src/test_mouse.cpp`: C++ implementace integrovaná v `CMakeLists.txt` pro testování přímo zkompilovanou binárkou.
* **Řízení motorů (`motor.cpp` / `motor.h`):**
  * Implementována třída pro ovládání H-můstků BTS7960 přes PWM piny Jetsonu (směr vpřed, vzad, brzda, volnoběh).
* **Čtení enkodérů (`encoder.cpp` / `encoder.h`):**
  * Čtení inkrementálních Hallových enkodérů z motorů Pololu.
* **Zadání myši pro robota:**
  * Pohyb vpřed/vzad: scrollování kolečkem / boční tlačítka.
  * Zatáčení: pohyb myši do stran (nebo analogové osy).
  * Bezpečnostní zastavení (Emergency Stop): kliknutí kolečkem (střední tlačítko).

---

## 4. Nápady a budoucí funkce (Feature Backlog)

1. **Záznam a ukládání projetých tras (Odometrie & Telemetrie):**
   * Průběžné ukládání ujeté vzdálenosti a natočení z enkodérů do souboru (`.csv` / `.json`).
   * Možnost zpětného přehrání trasy nebo funkce autonomního návratu do výchozího bodu ("Return to Home").
2. **PID regulace synchronizace pásů:**
   * Uzavřená regulační smyčka pro porovnávání pulsů z levého a pravého pásu v reálném čase, aby robot jel při přímém směru přesně rovně i při nerovném povrchu.
3. **Dead Man's Switch (Hlídání spojení):**
   * Automatické okamžité zastavení motorů, pokud z myši nedorazí žádný signál po dobu delší než 200 ms (ochrana proti ujetí robota při výpadku bezdrátového spojení).
4. **Vizuální subsystém (Kamera):**
   * Využití výpočetního výkonu Jetson Orin Nano pro rozpoznávání předmětů (např. plechovky, kostky) ve stylu filmu Wall-E.

---

## 5. Časový harmonogram kontrolních dnů

| Datum | Milník | Stav |
|---|---|---|
| **29. 10. 2026** | Návrh a výroba mechanických částí robota | 🔄 Probíhá (podvozek, krabičky) |
| **26. 11. 2026** | Sestavení hardwaru a prvotní testování podvozku | ⏳ Připraveno (deska hotová, čeká na oživení) |
| **17. 12. 2026** | Implementace řídicího systému a vývoj softwaru | ⏳ Rozpracováno (C++ jádro, test myši) |
| **28. 01. 2027** | Celková kompletace modelu a spuštění elektroniky | ⏳ Čeká |
| **25. 02. 2027** | Finální testování systému a vypracování dokumentace | ⏳ Čeká |
| **09. 04. 2027** | **Oficiální termín odevzdání maturitní práce** | 🏁 Cíl |
