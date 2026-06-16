# Otázka X - Enkódery a prioritní kodéry

### Slovo úvodem
Pojem enkóder (v české terminologii kodér) označuje v elektronice dvě zásadní, ale konceptuálně zcela odlišné kategorie prvků. První kategorií jsou **fyzické rotační snímače polohy**, které převádějí mechanickou rotaci na digitální pulzy pro mikrokontroléry. Druhou kategorií jsou **kombinační logické obvody**, které provádějí převod informace z $2^n$ vstupních linek na $n$ výstupních linek (opak dekodéru). Oba tyto principy jsou klíčové pro transformaci reálných stavů do podoby, které rozumí digitální procesory.

---

## Rotační inkrementální enkódery

**Rotační enkóder** je elektromechanická součástka převádějící rotační pohyb hřídele na sérii digitálních pulzů. Používá se všude tam, kde je vyžadováno přesné odměřování úhlu natočení, rychlosti otáčení, nebo jako uživatelský vstup (např. digitální otočné ovladače hlasitosti a menu).

* **Princip fungování:** Uvnitř enkóderu se nachází rotační disk s pravidelně rozmístěnými kontaktními ploškami (u mechanických) nebo štěrbinami (u optických). Při otáčení disk periodicky spíná dva mechanické kontakty nebo přerušuje dva optické paprsky. Tím vznikají dva obdélníkové signály označované jako **Kanál A (CLK)** a **Kanál B (DT)**.
* **Kvadraturní signál (Fázový posun):** Klíčovou vlastností je, že dráhy pro kanál A a B jsou mechanicky posunuty tak, aby jimi generované obdélníkové signály byly fázově posunuty přesně o 90°.
* **Určení směru rotace:** Díky fázovému posunu 90° můžeme v libovolný okamžik změny signálu určit směr otočení:
  * Pokud signál A předbíhá signál B (při nástupné hraně na A je B v log. 0), rotace probíhá jedním směrem (CW).
  * Pokud signál B předbíhá signál A (při nástupné hraně na A je B v log. 1), rotace probíhá opačným směrem (CCW).

```text
Směr CW (Vpřed):       Směr CCW (Vzad):
A:  __||__||__||__     A:  __||__||__||__
B:  ___||__||__||_     B:  _||__||__||___
   (A roste dřív)         (B roste dřív)
```

**Typické vývody modulů pro MCU:**
* `CLK` (Channel A) – Hlavní hodinový pulz.
* `DT` (Channel B) – Datový pulz fázově posunutý o 90°.
* `SW` (Switch) – Výstup integrovaného tlačítka, které se sepne při stisku hřídele dolů (spíná k GND).
* `+` (VCC) – Napájecí napětí. U 3,3V logiky (jako má Raspberry Pi Pico) musí být připojen striktně na 3,3 V, aby nedošlo ke zničení vstupů mikrokontroléru.
* `GND` – Společná zem.

![Mapa pinů Raspberry Pi Pico](cesta/k/tvemu/obrazku/pico_pinout.png)
*Zdroj diagramu: [Oficiální dokumentace Raspberry Pi Ltd](https://www.raspberrypi.com/documentation/microcontrollers/raspberry-pi-pico.html)*

### Čtení pomocí MCU a kaskádové řízení LED
Čtení stavu enkóderu v hlavní smyčce programu (polling) je v reálných aplikacích naprosto nepoužitelné. Mechanické kontakty trpí silnými zákmity (bouncing) a při rychlém otočení hřídele mikrokontrolér pulzy zmešká. Pro spolehlivé vyhodnocení je kriticky nutné použít **hardwarová přerušení (interrupts)** reagující na změnu hran.

Následující ukázka v MicroPythonu demonstruje pokročilé ošetření kvadraturního signálu, synchronizaci tlačítka (bez blokování procesoru) a kaskádové rozsvěcování 3 LED diod pomocí PWM (bargraph efekt):

```python
from machine import Pin, PWM
import time

# Konfigurace pinů enkóderu (CLK, DT, SW)
pin_clk = Pin(14, Pin.IN, Pin.PULL_UP)
pin_dt = Pin(15, Pin.IN, Pin.PULL_UP)
pin_sw = Pin(13, Pin.IN, Pin.PULL_UP)

# Konfigurace tří LED s hardwarovým PWM na příslušných pinech
led_pins = [16, 17, 18]
pwms = [PWM(Pin(p)) for p in led_pins]

for pwm in pwms:
    pwm.freq(1000)
    pwm.duty_u16(0)

# Matematika pro kaskádové rozsvěcení
STEPS_PER_LED = 20
TOTAL_LEDS = len(pwms)
MAX_TOTAL_STEPS = STEPS_PER_LED * TOTAL_LEDS
STEP_VALUE = 65535 // STEPS_PER_LED

counter = 0
clk_last_state = pin_clk.value()
blink_requested = False

# Funkce pro přepočet a zápis PWM do všech diod podle aktuální pozice
def update_leds():
    for i, pwm in enumerate(pwms):
        led_start_step = i * STEPS_PER_LED
        led_end_step = led_start_step + STEPS_PER_LED

        if counter <= led_start_step:
            pwm.duty_u16(0) # Dioda ještě není na řadě
        elif counter >= led_end_step:
            pwm.duty_u16(65535) # Dioda je plně rozsvícená
        else:
            # Dioda je v procesu rozsvěcení
            pwm.duty_u16((counter - led_start_step) * STEP_VALUE)

# ISR pro rotaci enkóderu
def encoder_callback(pin):
    global counter, clk_last_state
    clk_state = pin_clk.value()
    
    if clk_state != clk_last_state:
        if pin_dt.value() != clk_state:
            if counter < MAX_TOTAL_STEPS:
                counter += 1
        else:
            if counter > 0:
                counter -= 1
        update_leds()
    clk_last_state = clk_state

# ISR pro stisk tlačítka enkóderu (nastaví pouze vlajku, neblokuje přerušení)
def button_callback(pin):
    global blink_requested
    blink_requested = True

# Registrace přerušení na obě hrany signálu CLK
pin_clk.irq(trigger=Pin.IRQ_FALLING | Pin.IRQ_RISING, handler=encoder_callback)
pin_sw.irq(trigger=Pin.IRQ_FALLING, handler=button_callback)

# Výchozí aktualizace stavu
update_leds()

# Hlavní smyčka obsluhující časově náročné a blokující operace (blikání)
while True:
    if blink_requested:
        # Uložení aktuálních hodnot PWM
        current_duties = [pwm.duty_u16() for pwm in pwms]
        
        # Sekvence bliknutí (vše zhasnout -> rozsvítit -> zhasnout)
        for pwm in pwms: pwm.duty_u16(0)
        time.sleep(0.1)
        for pwm in pwms: pwm.duty_u16(65535)
        time.sleep(0.1)
        for pwm in pwms: pwm.duty_u16(0)
        time.sleep(0.1)
        
        # Obnova předchozích hodnot jasu před bliknutím
        for i, pwm in enumerate(pwms):
            pwm.duty_u16(current_duties[i])
            
        blink_requested = False
        
    time.sleep(0.05)
```
*Poznámka: Implementace obsluhy enkóderu pomocí hardwarového přerušení a kaskádové PWM řízení jasu vychází ze standardní dokumentace MicroPythonu a byla optimalizována pro studijní účely.*

### Interaktivní simulace obvodu
Níže je spuštěn plnohodnotný hardwarový emulátor s deskou Raspberry Pi Pico, rotačním enkóderem a kaskádou tří LED diod řízených přes PWM střídu. Kliknutím na enkóder a otáčením šipek můžete sledovat plynulé rozsvěcování hardwaru v reálném čase. Stisknutím středového tlačítka vyvoláte bliknutí.

<iframe src="https://wokwi.com/projects/466996791548049409?embed=1" width="100%" height="600px" style="border:1px solid #ccc;"></iframe>

*Interaktivní simulace obvodu a běhu MicroPythonu je zprostředkována online nástrojem [Wokwi](https://wokwi.com).*

---

## Logické prioritní kodéry

**Logický kodér** je kombinační obvod, který provádí inverzní operaci k dekodéru. Má $2^n$ vstupů a $n$ výstupů. Pokud se na jeden ze vstupů přivede aktivní úroveň (např. log. 1), obvod na výstupech vygeneruje binární kód odpovídající indexu tohoto vstupu.

* **Problém standardního kodéru:** Pokud by se aktivovalo více vstupů současně (např. vstup 2 i vstup 3), standardní kodér by provedl prostou logickou operaci nad hradly a vrátil by neplatný, deformovaný výstup.
* **Prioritní kodér (Priority Encoder):** Tento problém řeší integrovaná vnitřní logika priorit (např. u známých obvodů 74148). Pokud je aktivních více vstupů současně, obvod vygeneruje binární kód toho vstupu, který má **nejvyšší matematickou prioritu** (nejvyšší index). Nižší aktivní vstupy jsou ignorovány.

### Pravdivostní tabulka prioritního kodéru 4-na-2
Uvažujme aktivní logickou 1 na vstupech ($D_0$ až $D_3$, kde $D_3$ má nejvyšší prioritu). Výstupy jsou $Y_1, Y_0$ a pomocný výstup $V$ (Valid), který indikuje, zda je vůbec alespoň jeden vstup aktivní. Znak `X` reprezentuje stav "Don't care" (libovolná hodnota).

| D3 | D2 | D1 | D0 | Y1 | Y0 | V |
|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| 0  | 0  | 0  | 0  | X  | X  | 0  |
| 0  | 0  | 0  | 1  | 0  | 0  | 1  |
| 0  | 0  | 1  | X  | 0  | 1  | 1  |
| 0  | 1  | X  | X  | 1  | 0  | 1  |
| 1  | X  | X  | X  | 1  | 1  | 1  |

Z tabulky přímo vyplývají minimalizované logické rovnice pro návrh obvodu pomocí standardních hradel:
* $Y_1 = D_2 + D_3$
* $Y_0 = D_3 + (D_1 \cdot \overline{D_2})$
* $V = D_0 + D_1 + D_2 + D_3$

### Interaktivní simulace prioritního kodéru ve Falstadu
Níže je sestaven funkční prioritní kodér 4-na-2 realizovaný pomocí diskrétních hradel (OR, AND, NOT) na základě odvozených logických rovnic. Můžete si vyzkoušet aktivovat více vstupů najednou a sledovat, jak vstup s vyšším indexem maskuje vstupy s nižší prioritou.

<iframe src="https://wokwi.com/projects/466997191099631617?embed=1" width="100%" height="500px" style="border:1px solid #ccc;"></iframe>
