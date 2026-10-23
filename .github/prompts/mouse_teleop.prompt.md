---
name: "Mouse Teleop & Safety"
description: "Implementuje a ladí čtení bezdrátové myši přes Linux evdev s bezpečnostním časovačem"
---

Při práci s ovládáním myši v `4. Ročník/cpp_core/` dodržuj:

1. **Evdev rozhraní:**
   - Čtení z `/dev/input/event*` v neblokujícím režimu (`O_NONBLOCK`).
   - Automatická detekce myši z `/proc/bus/input/devices`.

2. **Mapování vstupů pro podvozek:**
   - Kolečko myši (`REL_WHEEL`): Plynulé přidávání/ubírání rychlosti vpřed/vzad.
   - Osa X (`REL_X`): Diferenciální výchylka do zatáčení (levý pás zrychlí, pravý zpomalí nebo naopak).
   - Střední tlačítko (`BTN_MIDDLE`): Okamžitý Emergency Stop (vynulování PWM na obou můstcích).

3. **Dead Man's Switch (Kritické):**
   - Vždy implementuj bezpečnostní watchdog: pokud z myši nedorazí žádný event po dobu delší než **200 ms**, motory se okamžitě zastaví.
