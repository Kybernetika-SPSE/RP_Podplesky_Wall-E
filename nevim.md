# Otázka X - Enkódery a prioritní kodéry

### Slovo úvodem
Pojem enkóder (v české terminologii kodér) označuje v elektronice dvě zásadní, ale konceptuálně zcela odlišné kategorie prvků. První kategorií jsou **fyzické rotační snímače polohy**, které převádějí mechanickou rotaci na digitální pulzy pro mikrokontroléry. Druhou kategorií jsou **kombinační logické obvody**, které provádějí převod informace z 2^n vstupních linek na n výstupních linek (opak dekodéru). Oba tyto principy jsou klíčové pro transformaci reálných stavů do podoby, které rozumí digitální procesory.

---

## Rotační inkrementální enkódery

**Rotační enkóder** je elektromechanická součástka převádějící rotační pohyb hřídele na sérii digitálních pulzů. Používá se všude tam, kde je vyžadováno přesné odměřování úhlu natočení, rychlosti otáčení, nebo jako uživatelský vstup (např. digitální otočné ovladače hlasitosti a menu).

* **Princip fungování:** Uvnitř enkóderu se nachází rotační disk s pravidelně rozmístěnými kontaktními ploškami (u mechanických) nebo štěrbinami (u optických). Při otáčení disk periodicky spíná dva mechanické kontakty nebo přerušuje dva optické paprsky. Tím vznikají dva obdélníkové signály označované jako **Kanál A (CLK)** a **Kanál B (DT)**.
* **Kvadraturní signál (Fázový posun):** Klíčovou vlastností je, že dráhy pro kanál A a B jsou mechanicky posunuty tak, aby jimi generované obdélníkové signály byly fázově posunuty přesně o 90°.
* **Určení směru rotace:** Díky fázovému posunu 90° můžeme v libovolný okamžik změny signálu určit směr otočení:
  * Pokud signál A předbíhá signál B (při nástupné hraně na A je B v log. 0), rotace probíhá jedním směrem (CW).
  * Pokud signál B předbíhá signál A (při nástupné hraně na A je B v log. 1), rotace probíhá opačným směrem (CCW).

    Směr CW (Vpřed):       Směr CCW (Vzad):
    A:  __||__||__||__     A:  __||__||__||__
    B:  ___||__||__||_     B:  _||__||__||___
       (A roste dřív)         (B roste dřív)

**Typické vývody modulů pro MCU:**
* `CLK` (Channel A) – Hlavní hodinový pulz.
* `DT` (Channel B) – Datový pulz fázově posunutý o 90°.
* `SW` (Switch) – Výstup integrovaného tlačítka, které se sepne při stisku hřídele dolů (spíná k GND).
* `+` (VCC) – Napájecí napětí (typicky 3.3V nebo 5V).
* `GND` – Společná zem.

### Čtení pomocí MCU
Čtení stavu enkóderu v hlavní smyčce programu (polling) je v reálných aplikacích naprosto nepoužitelné. Mechanické kontakty trpí silnými zákmity (bouncing) a při rychlém otočení hřídele mikrokontrolér pulzy zmešká. Pro spolehlivé vyhodnocení je kriticky nutné použít **hardwarová přerušení (interrupts)** reagující na změnu hran.

Následující ukázka v MicroPythonu demonstruje korektní ošetření kvadraturního signálu na Raspberry Pi Pico:

```python
from machine import Pin
import time

# Konfigurace pinů s vnitřním pull-up rezistorem
pin_clk = Pin(14, Pin.IN, Pin.PULL_UP)
pin_dt = Pin(15, Pin.IN, Pin.PULL_UP)

counter = 0
clk_last_state = pin_clk.value()

def encoder_callback(pin):
    global counter, clk_last_state
    clk_state = pin_clk.value()
    
    # Pokud došlo ke změně na CLK, ověříme směr
    if clk_state != clk_last_state:
        # Porovnáním aktuálního stavu DT se stavem CLK určíme směr rotace
        if pin_dt.value() != clk_state:
            counter += 1
        else:
            counter -= 1
            
    clk_last_state = clk_state

# Registrace přerušení na obě hrany (RISING i FALLING) pro maximální přesnost
pin_clk.irq(trigger=Pin.IRQ_FALLING | Pin.IRQ_RISING, handler=encoder_callback)

while True:
    print(f"Pozice: {counter}")
    time.sleep(0.1)
