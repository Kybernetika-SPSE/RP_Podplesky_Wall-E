#pragma once

// ============================================================================
// WALL-E Pinout — 1:1 podle schématu 4. Ročník/ELEKTRO/PCB/PCB.kicad_sch
// Konektor J11 (NVIDIA Jetson Orin Nano 40-pin expansion header)
// ============================================================================
//
// Fyzický pin J11 | Signál ze schématu | Tegra234 GPIO line | Popis
// ----------------|--------------------|--------------------|-------------------------
// Pin  1          | +3V3               | —                  | Napájení 3.3 V
// Pin  7          | R_LPWM             | PAC.06             | Pravý BTS7960 LPWM
// Pin  9          | GND                | —                  | Zem
// Pin 11          | R_RPWM             | PR.04              | Pravý BTS7960 RPWM
// Pin 13          | MOTOR_R_ENK_A      | PY.00              | Pravý enkodér kanál A
// Pin 15          | MOTOR_R_ENK_B      | PN.01              | Pravý enkodér kanál B
// Pin 19          | L_LPWM             | PZ.05              | Levý BTS7960 LPWM
// Pin 21          | L_RPWM             | PZ.04              | Levý BTS7960 RPWM
// Pin 23          | MOTOR_L_ENK_A      | PZ.03              | Levý enkodér kanál A
// Pin 25          | GND                | —                  | Zem
// Pin 29          | MOTOR_L_ENK_B      | PQ.05              | Levý enkodér kanál B
//
// POZOR pro JetPack 6:
//   Na JetPack 6 bylo odstraněno legacy /sys/class/gpio. Kód používá výhradně
//   moderní Linux GPIO character device (/dev/gpiochip*) a vyhledává linky
//   přímo podle Tegra234 jména (PAC.06, PZ.05, ...), takže nezávisí na offsetu.
//   Ujisti se přes `sudo /opt/nvidia/jetson-io/jetson-io.py`, že piny 7, 11,
//   19, 21, 13, 15, 23, 29 nejsou obsazené SPI/UART funkcí (jsou v GPIO režimu).
// ============================================================================

namespace walle_pins {

// Levý motor (BTS7960 LEFT)
inline constexpr const char* LEFT_LPWM_LINE  = "PZ.05";  // J11 Pin 19 (L_LPWM)
inline constexpr const char* LEFT_RPWM_LINE  = "PZ.04";  // J11 Pin 21 (L_RPWM)

// Pravý motor (BTS7960 RIGHT)
inline constexpr const char* RIGHT_LPWM_LINE = "PAC.06"; // J11 Pin 7  (R_LPWM)
inline constexpr const char* RIGHT_RPWM_LINE = "PR.04";  // J11 Pin 11 (R_RPWM)

// Levý enkodér (MOTOR_LEFT)
inline constexpr const char* LEFT_ENC_A_LINE  = "PZ.03"; // J11 Pin 23 (MOTOR_L_ENK_A)
inline constexpr const char* LEFT_ENC_B_LINE  = "PQ.05"; // J11 Pin 29 (MOTOR_L_ENK_B)

// Pravý enkodér (MOTOR_RIGHT)
inline constexpr const char* RIGHT_ENC_A_LINE = "PY.00"; // J11 Pin 13 (MOTOR_R_ENK_A)
inline constexpr const char* RIGHT_ENC_B_LINE = "PN.01"; // J11 Pin 15 (MOTOR_R_ENK_B)

}  // namespace walle_pins
