#!/usr/bin/env python3
import os
import sys
import glob
import struct
import select

def find_mouse_device():
    # Pokusí se najít myš přes /proc/bus/input/devices
    if os.path.exists("/proc/bus/input/devices"):
        with open("/proc/bus/input/devices", "r") as f:
            content = f.read().split("\n\n")
            for dev in content:
                if "mouse" in dev.lower() or "logitech" in dev.lower():
                    for line in dev.split("\n"):
                        if line.startswith("H: Handlers="):
                            for h in line[12:].split():
                                if h.startswith("event"):
                                    return f"/dev/input/{h}"
    # Fallback na první event
    events = sorted(glob.glob("/dev/input/event*"))
    return events[0] if events else None

BUTTON_NAMES = {
    0x110: "LEVÉ TLAČÍTKO (BTN_LEFT)",
    0x111: "PRAVÉ TLAČÍTKO (BTN_RIGHT)",
    0x112: "KOLEČKO STISK (BTN_MIDDLE)",
    0x113: "BOČNÍ ZPĚT (BTN_SIDE)",
    0x114: "BOČNÍ VPŘED (BTN_EXTRA)",
    0x115: "FORWARD (BTN_FORWARD)",
    0x116: "BACK (BTN_BACK)",
}

def main():
    dev_path = sys.argv[1] if len(sys.argv) > 1 else find_mouse_device()
    if not dev_path:
        print("[CHYBA] Žádné vstupní zařízení nebylo nalezeno v /dev/input/!")
        sys.exit(1)

    print(f"[INFO] Otevírám zařízení: {dev_path}")
    try:
        fd = open(dev_path, "rb")
    except PermissionError:
        print(f"[CHYBA] Přístup odepřen k {dev_path}!")
        print(f"        Spusťte program se sudo: sudo python3 {sys.argv[0]}")
        print("        Nebo přidejte uživatele do skupiny input: sudo usermod -a -G input $USER")
        sys.exit(1)

    # Detekce velikosti struct input_event:
    # 64-bit Linux (Jetson aarch64): 2x 64-bit int (timeval) + u16 + u16 + s32 = 24 bajtů
    # 32-bit Linux: 2x 32-bit int + u16 + u16 + s32 = 16 bajtů
    is_64bit = struct.calcsize("P") == 8
    event_format = "qqHHi" if is_64bit else "iiHHi"
    event_size = struct.calcsize(event_format)

    print("========================================================")
    print("Testování myši spuštěno. Pro ukončení stiskněte Ctrl+C.")
    print("========================================================\n")

    acc_x, acc_y = 0, 0

    try:
        while True:
            data = fd.read(event_size)
            if not data or len(data) < event_size:
                break
            sec, usec, ev_type, ev_code, ev_value = struct.unpack(event_format, data)

            # EV_KEY (1) - Tlačítka
            if ev_type == 1:
                state = "STISKNUTO" if ev_value == 1 else ("UVOLNĚNO" if ev_value == 0 else "DRŽENO")
                btn = BUTTON_NAMES.get(ev_code, f"TLAČÍTKO 0x{ev_code:x}")
                print(f"[TLAČÍTKO] {btn:<28} -> {state}")

            # EV_REL (2) - Pohyb a kolečko
            elif ev_type == 2:
                if ev_code == 0:  # REL_X
                    acc_x += ev_value
                    print(f"[POHYB] Rel X: {ev_value:+4d} | Kumulativní X: {acc_x}")
                elif ev_code == 1:  # REL_Y
                    acc_y += ev_value
                    print(f"[POHYB] Rel Y: {ev_value:+4d} | Kumulativní Y: {acc_y}")
                elif ev_code == 8:  # REL_WHEEL
                    dir_str = "NAHORU (+)" if ev_value > 0 else "DOLŮ (-)"
                    print(f"[KOLEČKO] Krok: {ev_value:+2d} ({dir_str})")
                elif ev_code == 6:  # REL_HWHEEL
                    print(f"[HORIZ. KOLEČKO] Krok: {ev_value:+2d}")

    except KeyboardInterrupt:
        print("\n[INFO] Ukončeno uživatelem.")
    finally:
        fd.close()

if __name__ == "__main__":
    main()
