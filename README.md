# WALL-E Replika – Výroba a řízení modelu robota

Tento repozitář obsahuje kompletní podklady pro návrh, mechanickou stavbu, návrh elektroniky a vývoj řídicího softwaru funkční repliky robota **WALL-E**. Projekt vzniká na **VOŠ a SPŠE Plzeň** v rámci studijního oboru *Technické lyceum (78-42-M/01)*.

* **Autor:** Tomáš Podpleský
* **Vedoucí práce:** Ing. Pavel Jedlička
* **Škola:** VOŠ a SPŠE Plzeň, Koterovská 85

---

## 📋 Oficiální zadání prací

Vývoj projektu je rozdělen do dvou hlavních etap podle studijních ročníků:

* **3. ročník – Ročníková práce (2025/2026):**
  * 📄 [Zadání RP (PDF)](./3.%20Ročník/Zadani_RP_Podpleský.pdf) | [Textová verze](./3.%20Ročník/Zadani_RP_Podpleský.txt)
  * *Téma:* Model robota Wall-E
  * *Cíl:* Návrh a sestavení základního mechanického konceptu modelu se zaměřením na stabilitu, odpružení a pohyb pásového podvozku.

* **4. ročník – Dlouhodobá maturitní práce (2026/2027):**
  * 📄 [Zadání DMP (PDF)](./4.%20Ročník/zadání%20v1_final_final.pdf)
  * *Téma:* Výroba modelu robota Wall-E
  * *Cíl:* Návrh a konstrukce plně funkčního konceptu modelu s pásovým podvozkem, dálkově ovládaného pomocí bezdrátové myši s využitím analogových spínačů.
  * *Termín odevzdání:* 9. dubna 2027

---

## ⚙️ Technická specifikace a architektura

### 1. Šasi a mechanická konstrukce
* **Materiály:** Kombinace konstrukčních ocelových plátů (laserové pálení) a aditivní výroby (3D tisk dílů z technických termoplastů).
* **Podvozek:** Nezávisle odpružený pásový systém s mechanismem napínání pásů pro spolehlivý přenos točivého momentu v terénu.
* **3D data:** CAD modely a výkresová dokumentace v Autodesk Inventor / formátech STEP, DWG a STL.

### 2. Výpočetní jednotka
* **Hardware:** **NVIDIA Jetson Orin Nano**
* **Úloha:** Zpracování vstupních periferií v reálném čase, generování řídicích signálů pro budiče motorů, plánovaná telemetrie/odometrie a autonomní funkce.

### 3. Pohon a výkonová část
* **Motory:** 2× stejnosměrný převodový motor JGB37-550 (12 V).
* **Budiče (Drivery):** 2× H-můstek BTS7960B (špičkový proud až 43 A) pro obousměrné řízení a regulaci otáček pomocí PWM.
* **Chlazení:** Optimalizovaný vzduchový tunel (Wind Tunnel) s aktivním chlazením (40×40×10 mm ventilátor).

### 4. Vlastní elektronika a napájení
* **Distribuce napájení (Power Hub):** Zakázková deska plošných spojů vyvíjená v prostředí **KiCad** ([`4. Ročník/ELEKTRO/PCB/`](./4.%20Ročník/ELEKTRO/PCB/)).
* **Měniče napětí:** Step-Down moduly LM2596 / HW-411 pro stabilní napájení Jetsonu a logiky.
* **Akumulátory:** Duální bateriový subsystém (Li-Po 3S).

### 5. Řídicí software a ovládání
* **Protokol ovládání:** Dálkové řízení bezdrátovou myší Logitech s využitím reverzního inženýrství protokolu **Logitech HID++ 2.0**. Z USB/HID reportů jsou v reálném čase vyčítána 10bitová surová data přítlaku magnetických spínačů tlačítek.
* **Implementace:**
  * Prototypové skripty v Pythonu ([`analog_mouse.py`](./4.%20Ročník/analog_mouse.py), [`test_mouse.py`](./4.%20Ročník/test_mouse.py)) pro ověření HID komunikace.
  * Výkonové jádro v C++ ([`cpp_core/`](./4.%20Ročník/cpp_core/)) využívající Linux `evdev` rozhraní pro minimální latenci a plynulý náběh výkonu motorů (Smooth Start/Stop).

---

## 📂 Struktura repozitáře

```text
Wall-E/
├── README.md                          # Hlavní přehled projektu a dokumentace
├── .gitignore                         # Konfigurace ignorovaných souborů pro Git
│
├── 3. Ročník/                         # Etapa 3. ročníku (Základní koncept a mechanika)
│   ├── Zadani_RP_Podpleský.pdf        # Oficiální zadání ročníkové práce
│   ├── Zadani_RP_Podpleský.txt        # Textová verze zadání RP
│   ├── code/                          # Původní zdrojové kódy (C, C++, Python)
│   ├── Elektronika/                   # Původní schémata zapojení (Příloha I)
│   ├── MODEL/                         # 3D sestavy a modely (Inventor)
│   ├── STEP/                          # Exporty sestav ve formátu STEP
│   ├── STL/                           # Podklady pro 3D tisk
│   ├── VÝKRESY/                       # Výrobní výkresy (DWG, PDF) pro laser cut
│   ├── FOTKY/                         # Foto a render dokumentace podvozku a pásů
│   └── prezentace/                    # Prezentace k obhajobě ročníkové práce
│
└── 4. Ročník/                         # Etapa 4. ročníku (DMP – Finální elektronika a řízení)
    ├── zadání v1_final_final.pdf      # Oficiální zadání dlouhodobé maturitní práce
    ├── ELEKTRO/                       # KiCad projekt zakázkové desky (Wall-E Power Hub)
    │   └── PCB/                       # Schémata, DPS a 3D STEP modely desky
    ├── cpp_core/                      # C++ řídicí jádro (CMake, evdev rozhraní)
    ├── Modely/                        # Doplňující 3D STEP/STL modely komponent (drivery, měniče)
    ├── analog_mouse.py                # Skript pro čtení analogových dat tlačítek z HID++
    ├── test_mouse.py                  # Testovací utilita pro ověření vstupů myši
    ├── Wall-E_Harmonogram.ics         # Kalendářní harmonogram vývoje a konzultací
    ├── Zdroje.txt                     # Rešerše odborných zdrojů, knihoven a podkladů
    └── napady.txt                     # Backlog plánovaných funkcí (odometrie, telemetrie)
```

---

