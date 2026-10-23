# Wall-E: Kompletní kontext projektu, architektura a stav vývoje

> [!NOTE]
> **Účel tohoto dokumentu:** Slouží jako ucelený předávací protokol a "Master Context" pro jakoukoliv AI i vývojáře. Při práci na jiném počítači stačí AI předat odkaz na tento soubor a okamžitě ví, jak je projekt postavený, jaká jsou pravidla, kde se nachází kód a co je dalším krokem.

---

## 1. Základní informace o projektu

* **Název práce:** Výroba modelu robota Wall-E (Dlouhodobá maturitní práce 2026/2027)
* **Škola:** VOŠ a SPŠE Plzeň, obor Technické lyceum (Třída 4. M)
* **Autor:** Tomáš Podpleský (`podplesky.tomas@icloud.com`)
* **Vedoucí práce:** Ing. Pavel Jedlička (v konverzacích zmiňován jako "Bublinak")
* **Git repozitář:** `https://github.com/Kybernetika-SPSE/RP_Podplesky_Wall-E.git` (hlavní větev: `master`)
* **Cíl projektu:** Návrh, stavba a zprovoznění funkčního modelu robota Wall-E s pásovým podvozkem, řízeného dálkově bezdrátovou myší pomocí palubního počítače NVIDIA Jetson Orin Nano.
* **Klíčové termíny (Milníky):**
  * `29. 10. 2026` – Návrh a výroba mechanických částí robota *(aktuální etapa)*
  * `26. 11. 2026` – Sestavení hardwaru a prvotní testování podvozku
  * `17. 12. 2026` – Implementace řídicího systému a vývoj softwaru
  * `28. 01. 2027` – Celková kompletace modelu a spuštění elektroniky
  * `25. 02. 2027` – Finální testování systému a technická dokumentace
  * `09. 04. 2027` – **Oficiální odevzdání maturitní práce**

---

## 2. Závazná pravidla pro asistenta (Persona & Coding Rules)

Pokud s Tomášem pracuje jakákoliv AI, **MUSÍ** dodržovat tyto 4 zásady:
1. **Stručnost a absolutní přesnost:** Žádné zbytečné omáčky, žádné omlouvání a vysvětlování toho, co už ví. Jít rovnou k věci, kód nebo technický krok ukázat přímo.
2. **Korektura a upřímnost:** Pokud uživatel navrhne programátorský nesmysl, špatnou elektro praxi nebo předražený nákup, **jasně ho zarazit a vysvětlit chybu**. Žádné falešné lichocení.
3. **Žádný balast v kódu:** Neopakovat celé obrovské soubory, pokud se změnilo jen pár řádků. Vypisovat a řešit jen to podstatné.
4. **Vysvětlovat jen na vyžádání:** Kód a zapojení rovnou předkládat funkční. Teorii přidávat pouze při výslovném dotazu nebo při reálném riziku poškození hardwaru / zkratu.

---

## 3. Struktura projektu na disku

Pracovní adresář: `c:\Users\podpl\Desktop\.Ročníkovka\Wall-E\`

```text
Wall-E/
├── README.md                      # Hlavní přehled repozitáře
├── 3. Ročník/                     # Starší podklady z 3. ročníku (výkresy, původní schémata)
│   ├── Blueprint/                 # Rozměrové skici Wall-E
│   ├── Dokumentace/               # Starší ročníková dokumentace (PDF)
│   ├── Elektronika/               # Původní schémata
│   └── VÝKRESY/                   # Původní technické výkresy podvozku
└── 4. Ročník/                     # AKTUÁLNÍ ROČNÍK (Zde probíhá veškerý vývoj!)
    ├── PREHLED_PROGRESS_A_PLANY.md# Tento soubor (Master context)
    ├── zadání v1_final_final.pdf  # Oficiální podepsané zadání maturitní práce
    ├── Zdroje.txt                 # Seznam zdrojů a komponent pro kusovník
    ├── napady.txt                 # Brainstorming a nápady na funkce
    ├── test_mouse.py              # Samostatný Python test vstupů z myši
    ├── cpp_core/                  # Hlavní C++ řídicí software pro Jetson
    │   ├── CMakeLists.txt         # CMake build skript (C++17)
    │   ├── include/
    │   │   ├── motor.h            # Třída pro řízení BTS7960 H-můstků
    │   │   └── encoder.h          # Třída pro čtení Hallových enkodérů
    │   └── src/
    │       ├── motor.cpp
    │       ├── encoder.cpp
    │       ├── main.cpp           # Hlavní řídicí smyčka a vlákna (evdev / HID++)
    │       └── test_mouse.cpp     # C++ diagnostika myši přes Linux evdev
    ├── ELEKTRO/
    │   └── PCB/                   # Návrh desky v KiCad 8.x
    │       ├── PCB.kicad_pro      # Projekt KiCadu
    │       ├── PCB.kicad_sch      # Schéma zapojení
    │       ├── PCB.kicad_pcb      # Layout plošného spoje
    │       └── Wall-E Power Hub v1.0.step # 3D model desky pro Fusion 360
    └── Modely/                    # CAD modely (Fusion 360)
        └── STEP/                  # 3D STEP soubory komponent (větráky, krabičky)
```

---

## 4. Hardwarová architektura a elektrické zapojení

### A. Napájecí větev (Power Tree)
1. **Zdroj:** Akumulátor 3S LiPo (nominál 11.1 V, plně nabitý 12.6 V).
2. **Hlavní vypínač a ochrana:**
   * Kolébkový spínač dimenzovaný na **20 A**.
   * Nožová autopojistka **20 A (žlutá)** v pouzdře na kladném pólu baterie. *(Pozor: 3A pojistka z minulé sady shoří okamžitě).*
3. **Silová větev (+12 V):**
   * Napájí přímo silové svorky obou budičů **BTS7960** pro stejnosměrné motory.
   * Napájí přímo **NVIDIA Jetson Orin Nano** přes jeho napájecí DC konektor (barrel jack 5.5/2.5 mm). Jetson má integrovaný regulátor (9–20 V).
4. **Logická větev (+5.0 V):**
   * Zajištěna nastavitelným step-down měničem **LM2596** (vstup 12 V, výstup seřízen na **5.0 V** trimrem před montáží!).
   * Napájí logické čipy obou BTS7960 driverů (VCC) a Hallovy senzory enkodérů na motorech Pololu.
   * **KRITICKÉ PRAVIDLO:** 5V výstup z LM2596 se **NIKDY NESMÍ** propojit s 5V piny na 40pinové liště Jetsonu! Zpětné napájení by poškodilo PMIC čip na Jetsonu.
5. **Filtrační ochrana:**
   * Na desce je osazen elektrolytický kondenzátor **1000 µF / 35 V Low-ESR** na 12V svorkovnici pro pohlcování indukčních špiček při reverzaci motorů.

### B. Vlastní DPS: Wall-E Power Hub v1.0 (KiCad)
Deska prošla kompletním přepracováním po připomínkách vedoucího práce ("žádné 0.2mm cesty pro napájení", "žádný Nürburgring"):
* **Tloušťky měděných cest:**
  * Silové cesty k motorům (+12 V a silová zem GND): **2.5 mm**
  * Napájecí cesty (12 V do LM2596, 5 V z LM2596, LED indikace): **0.8 mm**
  * Datové signály (PWM, EN, enkodéry, I2C): **0.4 mm** (striktně pod úhly 45°)
* **Měděné plochy (GND Plane):**
  * Oboustranné vylití mědi (`F.Cu` i `B.Cu`) spojené se signálovou i silovou zemí (GND). Propojení ostrovů realizováno prokovy (Vias).
* **Konektory na desce:**
  * `J11`: 40pin samice pro přímé propojení s GPIO Jetsonu.
  * `J8` / `J3`: 8pin konektory pro logiku levého a pravého driveru BTS7960 (VCC, GND, L_EN, R_EN, L_PWM, R_PWM; piny IS pro měření proudu jsou nezapojeny).
  * `J18` / `J17`: 4pin konektory vyhrazené pro enkodéry (Pin 1: +5V, Pin 2: GND, Pin 3: Signál A, Pin 4: Signál B).
  * `J14`, `J15`, `J16`: Robustní šroubovací svorkovnice pro silový vstup baterie a výstupy k driverům.
* **Mechanické otvory:**
  * 4x montážní otvor M2 (`MountingHole_2.2mm_M2`) pro připevnění na mosazné distanční sloupky.

### C. Mechanické uspořádání a chlazení (Box 2 – Engine Room)
* **Koncepce:** Větrný tunel pod podvozkem robota.
* **Uspořádání:**
  * Modul měniče LM2596 je instalován vertikálně **pod** deskou Power Hubu.
  * Po stranách jsou umístěny drivery BTS7960 s mohutnými hliníkovými chladiči.
  * Ventilátor **40×40×10 mm (12 V)** tlačí vzduch tunelem: ochlazuje LM2596 a profukuje žebra chladičů BTS7960.
* **Materiál 3D tisku:**
  * **ASA (Alzament ASA Black):** Teplotní odolnost 95–100 °C, UV stabilní, odolá zahřátí Jetsonu a driverů.
  * Záložní: **PETG** (odolnost do 80 °C).
  * **ZÁKAZ PLA:** PLA měkne při 55 °C a v uzavřeném podvozku by došlo ke zkroucení krabiček.

---

## 5. Softwarová architektura (`cpp_core`)

### A. Koncept dálkového ovládání myší
* Bezdrátová myš (Logitech nebo standardní USB wireless) má USB dongle zapojený v Jetsonu.
* V Linuxu je vyčítána přes rozhraní **`evdev`** (`/dev/input/event*`):
  * **Kolečko myši (`REL_WHEEL`):** Plynulá regulace rychlosti vpřed / vzad.
  * **Horizontální pohyb / osy:** Zatáčení (diferenciální řízení pásů).
  * **Střední tlačítko (`BTN_MIDDLE`):** Bezpečnostní STOP / Start (přepínání aktivního stavu).
  * **Boční tlačítka (`BTN_SIDE`, `BTN_EXTRA`):** Rychlá volba směru.

### B. Připravené diagnostické nástroje
1. **Python skript:** `4. Ročník/test_mouse.py`
   * Spuštění na Jetsonu: `sudo python3 test_mouse.py`
   * Automaticky detekuje myš v `/proc/bus/input/devices`, otevírá příslušný event a v reálném čase vypisuje relativní souřadnice, kroky kolečka a stisky všech tlačítek.
2. **C++ test:** `4. Ročník/cpp_core/src/test_mouse.cpp`
   * Sestavení: `cd cpp_core/build && cmake .. && make test_mouse`
   * Nativní binárka testující event stream.

### C. Řízení motorů a PID smyčka
* `motor.cpp`: Generuje PWM střídu pro piny `R_PWM` a `L_PWM` budiče BTS7960.
* `encoder.cpp`: Počítá pulsy z Hallových senzorů motorů pro určení přesné úhlové rychlosti a ujeté vzdálenosti.
* **Plánovaný PID regulátor:** Uzavřená diskrétní regulační smyčka pro vyrovnávání otáček obou pásů v reálném čase, aby robot při jízdě rovně neujížděl do stran.

---

## 6. Aktuální stav projektu a nejbližší úkoly (Co je na řadě)

### Co je HOTOVO:
- [x] Návrh a přepracování desky Power Hub v1.0 v KiCadu (silné cesty, vylitá zem GND, nová pouzdra pro enkodéry).
- [x] Export 3D modelu desky (.step) a ověření uspořádání ve Fusion 360.
- [x] Odeslání všech změn na GitHub (`origin/master` je plně synchronní).
- [x] Vytvoření testovacích nástrojů pro myš v Pythonu i C++.
- [x] Výběr nákupu komponent (128GB microSD karta třídy A2/V30, ASA filament, 20A autopojistka, 4010 ventilátor).

### Co zbývá udělat (TODO):
1. **Zprovoznění Jetsonu (OS JetPack):**
   * Vypálit stažený JetPack SD Card Image na 128GB microSD kartu (přes BalenaEtcher).
   * Spustit Jetson s monitorem a klávesnicí, projít úvodním nastavením Ubuntu (uživatel `tomas`, nastavení sítě a SSH).
   * Otestovat USB myš pomocí `test_mouse.py`.
2. **Dokončení mechaniky ve Fusion 360 (Box 2):**
   * Doplnit montážní otvory pro distanční sloupky M2.
   * Vymodelovat kabelové průchodky a úchyty pro 4010 ventilátor (otvory 2.8 mm pro samořezné M3 šrouby).
   * Vyexportovat STL/STEP a naslicovat pro tisk z ASA.
3. **Výroba DPS a oživení:**
   * Zadání desky do výroby (nebo fotocestou ve škole).
   * Osazení součástek (svorkovnice, piny, 1000µF kondenzátor).
   * Nastavení LM2596 trimrem na 5.0 V **před zapojením** do zbytku systému!
4. **Dopsání softwaru:**
   * Implementace PID regulace v C++ (`cpp_core`) pro synchronizaci obou pásů.
   * Zavedení bezpečnostního časovače (Dead Man's Switch) – zastavení pásů při výpadku spojení z myši > 200 ms.
   * Záznam telemetrie/odometrie do logu (`napady.txt` – ukládání ujetých tras pro návrat domů).
