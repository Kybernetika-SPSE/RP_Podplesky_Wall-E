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

Pracovní adresáře:
* **CachyOS Linux (hlavní vývoj):** `/home/tomasp/Plocha/škola/Wall-e/`
* **Windows Acer (CAD / KiCad / Word):** `c:\Users\podpl\Desktop\.Ročníkovka\Wall-E\`
* **NVIDIA Jetson Orin Nano (headless přes SSH `jetson-usb` / `192.168.55.1`):** `/home/tomas/Desktop/Wall-e/` (a synchronizovaný DRY-RUN simulátor v `/home/tomas/testy/` ↔ `/home/tomasp/jetson-sync/testy/` přes Syncthing)

```text
Wall-E/
├── README.md                      # Hlavní přehled repozitáře
├── 3. Ročník/                     # Starší podklady z 3. ročníku (výkresy, původní schémata)
│   ├── Blueprint/                 # Rozměrové skici Wall-E
│   ├── Dokumentace/               # Starší ročníková dokumentace (PDF)
│   ├── Elektronika/               # Původní schémata
│   └── VÝKRESY/                   # Původní technické výkresy podvozku
└── 4. Ročník/                     # AKTUÁLNÍ ROČNÍK (Zde probíhá veškerý vývoj!)
    ├── PREHLED_PROGRESS_A_PLANY.md# Tento soubor (Master context pro všechny PC a AI)
    ├── zadání v1_final_final.pdf  # Oficiální podepsané zadání maturitní práce
    ├── Zdroje.txt                 # Seznam zdrojů a komponent pro kusovník
    ├── napady.txt                 # Brainstorming a nápady na funkce
    ├── test_mouse.py              # Samostatný Python test vstupů z myši
    ├── cpp_core/                  # Hlavní C++ řídicí software pro Jetson (C++17)
    │   ├── CMakeLists.txt         # CMake build skript (hidapi-hidraw + pthread)
    │   ├── include/
    │   │   ├── pins.h             # Centrální mapa pinů podle PCB.kicad_sch (Tegra234 jména)
    │   │   ├── gpio_line.h        # Moderní Linux /dev/gpiochip* wrapper (<linux/gpio.h>)
    │   │   ├── motor.h            # SW PWM 250 Hz BTS7960 driver (připraveno na PCA9685 backend)
    │   │   ├── encoder.h          # Kvadraturní enkodéry přes GPIO_GET_LINEEVENT_IOCTL
    │   │   └── web_debug.h        # Vestavěný HTTP live debug server (port 8080)
    │   └── src/
    │       ├── gpio_line.cpp
    │       ├── motor.cpp
    │       ├── encoder.cpp
    │       ├── web_debug.cpp      # Dark-mode live telemetrie pro iPhone hotspot i notebook
    │       ├── main.cpp           # Superstrike + DualSense (tempomat, LED, triggery, vibrace)
    │       └── test_mouse.cpp     # C++ diagnostika myši přes Linux evdev
    ├── ELEKTRO/
    │   └── PCB/                   # Návrh desky v KiCad 10.x
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
4. **Logická větev (+5.0 V a +3.3 V):**
   * **+5.0 V:** Nastavitelný step-down měnič **LM2596** (vstup 12 V, výstup seřízen na **5.0 V** trimrem před montáží!) — napájí výhradně logiku obou BTS7960 (`VCC`, `L_EN`, `R_EN`), případně bzučák / WS2812B.
   * **+3.3 V (z Jetson Pin 1):** Napájí Hallovy enkodéry a logiku I2C modulů (PCA9685 atd.), aby na 3.3 V GPIO piny Jetsonu nikdy nešel 5 V signál!
   * **KRITICKÉ PRAVIDLO:** 5V výstup z LM2596 se **NIKDY NESMÍ** propojit s 5V ani 3.3V piny na 40pinové liště Jetsonu! Zpětné napájení by poškodilo PMIC čip na Jetsonu.
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
* **Konektory na desce (v1.0 — aktuální schéma):**
  * `J11`: 40pin samice pro přímé propojení s GPIO Jetsonu (`R_LPWM`=Pin 7/`PAC.06`, `R_RPWM`=Pin 11/`PR.04`, `R_ENK_A`=Pin 13/`PY.00`, `R_ENK_B`=Pin 15/`PN.01`, `L_LPWM`=Pin 19/`PZ.05`, `L_RPWM`=Pin 21/`PZ.04`, `L_ENK_A`=Pin 23/`PZ.03`, `L_ENK_B`=Pin 29/`PQ.05`).
  * `J8` / `J3`: 8pin konektory pro logiku levého a pravého driveru BTS7960 (VCC, GND, L_EN, R_EN, L_PWM, R_PWM; piny IS pro měření proudu jsou nezapojeny).
  * `J18` / `J17`: 4pin konektory vyhrazené pro enkodéry (`+3V3`, `GND`, Signál A, Signál B).
  * `J14`, `J15`, `J16`: Robustní šroubovací svorkovnice pro silový vstup baterie a výstupy k driverům.
* **Plánovaný upgrade DPS před objednáním — Power Hub v2.0 (rozhodnuto 26. 11. 2026 po problému s JetPack 6 `TRISTATE` pinmuxem a spáleným breadboardem):**
  * **I2C PCA9685** modul (adresa `0x40`, 1 kHz HW 12-bit PWM): `VCC=+3V3`, `GND=GND`, `SDA=Pin 3`, `SCL=Pin 5`, `OE=Pin 7` (+ `10 kΩ` pull-up na `+3V3` a 2pin konektor `J_ESTOP_HW` pro fyzický E-STOP hříbek). Výstupy: `CH0=L_LPWM`, `CH1=L_RPWM`, `CH2=R_LPWM`, `CH3=R_RPWM` (`CH4..15` rezerva).
  * **Modul LM2596 přímo na PCB** (`IN+=+12V_MOT`, `OUT+` přes 2pin jumper **`JP_5V`** na `+5V` pro seřízení 5,00 V naprázdno) + TVS `SMBJ5.0A` + `100 µF` + `100 nF`.
  * **Samostatná 12 V větev pro Jetson:** pojistka `4 A` + `1000 µF / 25 V` + TVS `SMBJ15A` → svorkovnice `J_JETSON_PWR`.
  * **3 stavové LEDky** (`+12V` zelená + `1 kΩ`, `+5V` žlutá + `470 Ω`, `+3V3` modrá + `330 Ω`) + **6pin testovací lišta** (`GND, 12V, 5V, 3V3, SDA, SCL`).
  * **Enkodéry (`+3V3`):** `1 kΩ` sériové ochranné odpory na linkách `R_ENK_A` (Pin 13), `R_ENK_B` (Pin 15), `L_ENK_A` (Pin 23), `L_ENK_B` (Pin 29) + `100 nF` kondenzátory u obou konektorů.
  * **3× volný 4pin I2C port** (`+3V3, GND, SCL, SDA`) pro `INA219` (baterie/proud), `MPU6050` (gyro), `SSD1306` OLED (IP adresa), `VL53L0X` (překážky).
  * **5 V aktivní bzučák** (Pin 11 přes `1 kΩ` na NPN `BC547`/`2N2222`), **WS2812B LED konektor** (Pin 19 přes `330 Ω`), rezervní GPIO header (Pin 21, 31, 33 + GND).
* **Mechanické otvory:**
  * 4x montážní otvor M2 (`MountingHole_2.2mm_M2`) pro připevnění na mosazné distanční sloupky.

### C. Mechanické uspořádání, podvozek a chlazení

#### 1. Koncepce pásového podvozku (Detailní mechanický návrh)
* **Pohon a hnací kolo:**
  * Hnací kolo je umístěno **nahoře** (vrchol trojúhelníkového profilu pásu, průměr kola $D = 104\text{ mm}$, šířka 25 mm).
  * **Motor:** Pololu 37D s převodovkou 100:1 (100 ot/min při 12 V, točivý moment 3,3 Nm, 6mm D-hřídel).
  * **Převodový stupeň motor–hnací kolo:** V generátoru GT2 je rozpracována řemenice se **16 zuby, roztečí 2 mm, otvorem 6 mm, pro řemen šířky 9 mm a vodicí přírubou 1,5 mm**. Obě řemenice mají být v jedné rovině. Konkrétní délka řemenu zatím není určena; musí vycházet ze skutečné pracovní vzdálenosti os a rozsahu pohybu napínacího modulu.
  * Vzhledem k nedostatku místa je rozpracováno uložení motoru svisle dolů a dvojice kuželových kol (*mitre gears*) **1:1** pro změnu směru pohonu o 90°. Původní zkušební nastavení bylo 17/17 zubů, modul 0,30 mm a tlakový úhel 14,5°. Následně byl zvažován nový předběžný návrh 17/17, modul 1,5 mm, tlakový úhel 20° a šířka ozubení 4 mm; tento návrh ani záběr nejsou ověřené a před výrobou se musí zkontrolovat geometrie, vůle a únosnost.
  * **Rychlost a tah:** Výsledná rychlost robota je **0,55 m/s (cca 2,0 km/h)**; motory poskytují na pásech **souhrnný tah cca 13 kg**.
* **Integrovaná řemenice v těle kola (Monolit z ASA):**
  * Z boku hnacího kola je přímo vymodelována řemenice GT2 jako součást jednoho dílu; ozubení je určené pro 9mm řemen. Nastavení generátoru: **16 zubů, otvor 6 mm, šířka řemenu 9 mm, výška vodicí příruby 1,5 mm**. Rozměr otvoru generátoru je nutné sladit s osou a ložisky v konečné geometrii.
  * Výhoda: Odpadá samostatná hliníková řemenice, červíky a riziko protočení na hřídeli; síla motoru se přenáší přímo do těla kola.
* **Napínání pohonného řemenu:**
  * Rozpracovaný koncept používá výkyvný pohonný modul nesoucí motor, kuželová kola a výstupní řemenici. Pružina má otáčet celým modulem a udržovat napnutí GT2 řemenu; samostatná napínací kladka se zatím nepředpokládá.
  * Kuželová kola, jejich hřídele a uložení se mají pohybovat jako jeden tuhý modul, aby se při výkyvu neměnila jejich vzájemná poloha. Osa výstupní řemenice musí být rovnoběžná s osou řemenice na hnacím kole.
  * Dřívější odhady délky řemenu vycházely z předpokládané vzdálenosti os, nikoli z ověřeného pracovního rozsahu. Délku je třeba určit až po změření skutečných pracovních krajních poloh a ověření zdvihu pružinového modulu; teoretický doraz není běžná pracovní poloha.
  * V rané verzi držáku byla navržena vůle přibližně **1 mm** mezi stěnou držáku a hnacím kolem i mezi protější stěnou a řemenicí. Vůli je nutné ověřit po celé dráze pohybu.
* **100% vystředění pásu v ose podvozku:**
  * Mezi vnějšími trojúhelníkovými pláty je rozteč **80 mm**. Osa pásu leží přesně uprostřed na kótě **40,0 mm**.
  * Hnací kolo (25 mm) sedí symetricky od 27,5 mm do 52,5 mm.
  * U-blok nesoucí ložiska a vodicí tyče má vnitřní dutinu 38 mm a asymetrické stěny (vlevo 27,5 mm, vpravo 20,5 mm), což umožňuje elegantně pojmout integrovanou řemenici bez jakéhokoliv vyosení pásu.
* **Krajní vodicí kola pásu (přední a zadní idler) – koncepční návrh:**
  * Zvažuje se odpružené uložení každého krajního kola na kyvném rameni s osou otáčení v ohybu ramene a pružinou působící na jeho konec. Pružina má udržovat napnutí pásu.
  * Při propružení se kolo pohybuje po oblouku nahoru a do strany; směr síly pružiny, trajektorii, zdvih a vliv na napnutí pásu je nutné ověřit v CADu včetně kolizí v krajních polohách.
  * Jde o návrh z náčrtů, nikoli o ověřenou geometrii nebo hotový CAD mechanismus.
* **Podpěrná kolečka spodní větve pásu – první prototyp:**
  * Zvažují se kývavé dvojice koleček se středovým čepem, případně odpružení celého modulu.
  * Pro první prototyp mohou být podpěrná kolečka pevná. Odpružení spodní větve se doplní, pokud zkoušky ukážou, že pás na nerovnostech ztrácí kontakt nebo se chová nestabilně.
  * Počet, rozteče a finální způsob uložení koleček nejsou tímto konceptem potvrzeny; ověřit podle aktuálního CADu.
* **Uložení hřídelí a ložisek pohonu:**
  * Pro 6mm hřídel byla probírána řada ložisek 626 (**6 × 19 × 6 mm**) a kovové rameno s otvorem přibližně 18,9 mm. Konkrétní uložení ani tolerance nejsou ověřené; před výrobou určit požadovaný fit podle skutečného ložiska a materiálu ramene a ověřit na zkušebním kusu.

#### 2. Uspořádání elektroniky a chlazení (Box 2 – Engine Room)
* **Koncepce:** Větrný aerodynamický tunel pod podvozkem robota.
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

### A. Dálkové ovládání (1:1 s DRY-RUN GUI simulátorem)
* **Priorita vstupů:** `Logitech PRO X2 SUPERSTRIKE` > `Sony DualSense` > `NONE` (hot-plug s okamžitým zastavením při odpojení).
* **Logitech PRO X2 SUPERSTRIKE (`046d:c54d` / `046d:c0a8`):**
  * Analogový přítlak přes HID++ 2.0 s kalibrovanou progresivní expo křivkou (`raw 300..720`, deadzone `0.12`, `γ = 2.4`, vyhlazení `0.35 / release 0.65`).
  * Kolečko (`REL_WHEEL`): společná jízda (±30 PWM/krok, decay `250 PWM/s` po `0.5 s`).
  * `Side` = vzad (`-1`), `Extra` = vpřed (`+1`), `Middle` = `E-STOP`.
* **Sony DualSense (`054c:0ce6` / `054c:0df2`, USB i Bluetooth):**
  * Automatická detekce `hid-playstation` (CachyOS) vs. `hid-generic` (Jetson 5.15) + přímý parser 78B Bluetooth `0x31` reportů z `/dev/hidraw*` po přepnutí přes `HIDIOCGFEATURE(0x05)`.
  * **`R2` / `L2`:** analogový plyn vpřed / vzad (`throttle = R2 - L2`).
  * **Proporcionální zatáčení levým thumbstickem (`LX`):** zpomaluje vnitřní pás úměrně aktuální rychlosti (např. při 25 % a páčce do půlky jde vnitřní pás na 12,5 %); při couvání (`L2` / záporný tempomat) pracuje **inverzně**.
  * **Tempomat (`R1`):** zachytí rychlost z `R2 - L2`; `D-Pad ▲/▼` plynule mění a drží rychlost (`120 PWM/s`); `L-páčka ↓` proporcionálně brzdí; manuální stisk `R2`/`L2` (po uvolnění z aktivace) nebo druhý stisk `R1` tempomat vypne.
  * **`Create + Options`:** `E-STOP`.
  * **Zpětná vazba DualSense (USB & BT, vlastní CRC-32):**
    * **RGB lightbar:** `#000F59` = E-STOP, zelená = vpřed (jas podle rychlosti), červená = vzad (jas podle rychlosti), fialová = tempomat (plynule do oranžové při brzdění).
    * **5 bílých Player LED:** `[L-high, L-active, Tempomat, R-active, R-high]`.
    * **Adaptivní triggery:** E-STOP zámek `0xFF`, tempomat tvrdá zarážka `0xFF`, manuální pedálový odpor.
    * **Vibrace:** potvrzovací pulzy (`E-STOP`, `R1`), silná výstražná vibrace při snaze jet v E-STOPu, jemné brnění při brzdění v tempomatu.

### B. Řízení motorů, enkodéry a Live Web Debug (`:8080`)
* **`gpio_line.cpp`:** Moderní Linux `/dev/gpiochip*` (`<linux/gpio.h>`) s vyhledáváním linek podle Tegra234 jména (legacy `/sys/class/gpio` na JetPack 6 neexistuje).
* **`motor.cpp`:** Aktuálně SW PWM 250 Hz přes GPIO (`--dry-run` podporován); plánováno doplnit **I2C PCA9685** backend pro PCB v2.0.
* **`encoder.cpp`:** Kvadraturní čtení přes `GPIO_GET_LINEEVENT_IOCTL` na obě hrany pinu A.
* **`web_debug.cpp`:** Vestavěný HTTP server na `0.0.0.0:8080` (dark mode, 20 Hz `/api/state` JSON, živé PWM bary, enkodéry, vstupy, tlačítka `E-STOP` a `Vynulovat enkodéry`) — dostupný přes iPhone hotspot (`http://172.20.10.3:8080`) i USB-C (`http://192.168.55.1:8080`).
* **Závislosti a systémová nastavení:** Na Jetsonu je doinstalován `libhidapi-dev` (build: `cmake -S . -B build && cmake --build build -j6`, spuštění: `sudo ./build/walle_controller` nebo `--dry-run`). Na CachyOS je aktivní udev pravidlo `/etc/udev/rules.d/70-logitech-superstrike.rules` (číslo `< 73` kvůli `TAG+="uaccess"` + chmod `0666` pro DualSense sysfs LED).

---

## 6. Aktuální stav projektu a nejbližší úkoly (Co je na řadě)

### Co je HOTOVO (aktualizováno 26. 11. 2026):
- [x] Návrh desky Power Hub v1.0 v KiCadu + export 3D modelu (.step).
- [x] Zprovoznění Jetsonu Orin Nano (JetPack 6 / Ubuntu 22.04, headless režim `multi-user.target`, SSH přes USB-C `192.168.55.1` i iPhone hotspot `172.20.10.3`, Syncthing obousměrný sync).
- [x] Bluetooth spárování DualSense s Jetsonem (`50:EE:32:D0:AF:4F`, `Trusted: yes`) + vyřešení Enhanced BT `0x31` reportů na kernelu 5.15.
- [x] Kompletní C++ jádro `4. Ročník/cpp_core` (Superstrike + DualSense + tempomat + proporcionální zatáčení + LED/triggery/vibrace + `/dev/gpiochip*` SW PWM + enkodéry + Live Web Debug Monitor na portu `8080`) — zkompilováno a ověřeno na Jetsonu i CachyOS.
- [x] DRY-RUN Python Tkinter simulátor v `/home/tomasp/jetson-sync/testy/` (48/48 unit testů OK, desktop ikona v GNOME).
- [x] Výpočet převodového poměru a rychlosti robota (převod 1:1, GT2 řemen, 0,55 m/s, 13 kg tah) a vystředění pásu na 40,0 mm.

### Aktuálně probíhá / Nejbližší TODO:
1. **Upgrade DPS na Power Hub v2.0 (KiCad) & objednání výroby:**
   * Překreslit schéma podle návrhu v sekci 4.B (přidat **I2C PCA9685** s `OE` HW E-STOP, **LM2596 modul + jumper `JP_5V`**, 3 stavové LEDky, `1 kΩ` na enkodéry, 3× volný I2C port, bzučák, WS2812B, testovací piny). *(Breadboard prototypování odloženo, čeká se na čistou PCB.)*
2. **Software v `4. Ročník/cpp_core` (lze psát a testovat v `--dry-run` před příchodem PCB):**
   * Přidat **I2C PCA9685 backend** (`/dev/i2c-7`, `0x40`, 1 kHz, přepínatelný s GPIO / `--dry-run`).
   * Rozšířit **Live Web Debug Monitor (`:8080`)**: top-down schéma podvozku WALL-E, živé grafy PWM/enkodérů, systémová telemetrie Jetsonu (INA3221 napětí/W, teplota CPU/GPU).
   * Implementovat **Nahrávání a přehrávání tras (Record / Replay)** z webu / ovladače.
   * Implementovat **PID regulátor** rychlosti pásů + **Dead Man's Switch (200 ms)** + `systemd` autostart službu na Jetsonu.
3. **Dokončení mechaniky ve Fusion 360 (Box 2) & 3D tisk z ASA:**
   * Dokončit CAD výkyvného pohonného modulu, odpružených krajních kol, průchodek a úchytů ventilátoru 4010, export STL/STEP a tisk z ASA.
