---
name: "Odometry & Route Logger"
description: "Zapisuje data z enkodérů do souboru a implementuje přehrávání projeté trasy"
---

Při práci s telemetrií a ukládáním tras v `4. Ročník/cpp_core/` dodržuj:

1. **Struktura dat logu:**
   - Časová značka (timestamp v milisekundách od startu).
   - Levý a pravý čítač pulsů enkodéru (`left_ticks`, `right_ticks`).
   - Přepočtená ujetá vzdálenost v metrech a odhadnutý úhel natočení $\theta$.

2. **Formát a zápis:**
   - Zápis do CSV souboru (např. `/var/log/walle_routes/route_YYYYMMDD_HHMMSS.csv`) na samostatném nízko-prioritním vlákně, aby neblokoval regulační smyčku motorů.
   - Bufferování zápisu na disk kvůli šetření microSD karty Jetsonu.

3. **Přehrávání trasy:**
   - Funkce pro zpětné čtení CSV a autonomní navrácení do výchozího bodu obrácením pořadí segmentů.
