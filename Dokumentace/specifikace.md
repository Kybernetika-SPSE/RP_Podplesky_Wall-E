# WALL-E Off-road Replica - Technická Specifikace

## Cíl Projektu
Funkční replika robota WALL-E (Off-road verze).
- **Cílová hmotnost:** 7–8 kg
- **Styl:** Heavy Duty (Ocel + 3D tisk), žádný levný plast.

## 1. Mechanická Konstrukce (Chassis)
- **Typ podvozku:** Pásový (Delta tvar - trojúhelník).
- **Materiál rámu:** Ocelové pláty (laser cut), sendvičová konstrukce.
- **Odpružení:** Nezávislé zavěšení každého pásu (kyvná polonáprava / swing axle).
- **Tlumiče:** Zadní tlumiče z RC Buggy 1:8 (olejové, délka 120mm).
- **Napínání pásů:** Mechanické, posuvný blok v drážce, tlačený šroubem M8 (délka 100mm).

## 2. Pohon (Powertrain)
- **Motory:** 2x JGB37-550 (12V, 160 RPM).
- **Driver motorů:** 2x BTS7960B (H-Můstek, 43A).
- **Přenos síly:** Ozubené hnací kolo (Sprocket) přímo na motoru nebo přes převod.

## 3. Mozek a Elektronika
- **Hlavní počítač:** Nvidia Jetson (Nano nebo Orin).
- **Logika:** 3.3V (Jetson).
  - *PWM Rozšíření:* PCA9685 (I2C, 16 kanálů) pro přesné řízení motorů.
  - *Poznámka:* Nutnost Logic Level Shifterů pro 5V drivery (pokud bude třeba).
- **Komunikace:** Wi-Fi / Bluetooth (Gamepad).

## 4. Napájení (Dual Battery System)
- **Okruh A (Pohon):** Li-Po 3S (11.1V) 3500mAh 60C -> BTS7960B.
- **Okruh B (Jetson):** Li-Po 3S (11.1V) 3500mAh -> Kabel DC5525 přímo do Jetsonu.
- **Ochrana:** Li-Po Alarmy (BX100) na každé baterii, pojistkové skříně (nožové pojistky).

## 5. Aktuální Stav Vývoje
- **HOTOVO:**
  - Design bočnic pásů (ocelové výpalky).
  - Napínací mechanismus (ocelový blok, závitová tyč M8, pružinová závlačka).
  - BOM (komponenty.md)
- **TODO:**
  - Finalizace 3D modelů pro tisk (kola, pásy).
  - Výroba ocelových dílů.
  - Zapojení elektroniky s Jetsonem (schéma, kód).
