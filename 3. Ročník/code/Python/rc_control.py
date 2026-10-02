import serial
import keyboard
import time
import sys
COM_PORT = 'COM3'
BAUD_RATE = 115200
print("=" * 50)
print(" WALL-E: HERNÍ OVLADAČ (Klávesnice)")
print("=" * 50)
print(f"Pokouším se připojit na port {COM_PORT}...")
try:
    ser = serial.Serial(COM_PORT, BAUD_RATE, timeout=0.1)
    print("✅ ÚSPĚŠNĚ PŘIPOJENO!")
except Exception as e:
    print(f"❌ CHYBA PŘIPOJENÍ k {COM_PORT}: {e}")
    print("\nPOZOR: Pravděpodobně máš ve VS Code stále otevřený Serial Monitor!")
    print("Sériový port může používat vždy jen jeden program. Zavři dole okno")
    print("terminálu ve VS Code (ikona koše) a zapni tento skript znovu.")
    sys.exit(1)
print("\n--- NÁVOD K OVLÁDÁNÍ ---")
print(" W / Šipka Nahoru  -> Jízda vpřed")
print(" S / Šipka Dolů    -> Jízda vzad")
print(" A / Šipka Doleva  -> Otočka na místě doleva")
print(" D / Šipka Doprava -> Otočka na místě doprava")
print(" Pustit klávesu    -> Okamžité zastavení")
print(" ESC               -> Konec programu\n")
last_sent = ''
while True:
    try:
        if keyboard.is_pressed('esc'):
            print("Ukončuji program a brzdím...")
            ser.write(b's')
            break
        cmd = ''
        if keyboard.is_pressed('up') or keyboard.is_pressed('w'):
            cmd = 'qe'
        elif keyboard.is_pressed('down') or keyboard.is_pressed('s'):
            cmd = 'ad'
        elif keyboard.is_pressed('left') or keyboard.is_pressed('a'):
            cmd = 'ae'
        elif keyboard.is_pressed('right') or keyboard.is_pressed('d'):
            cmd = 'qd'
        else:
            cmd = 's'
        if cmd != last_sent:
            ser.write(cmd.encode('utf-8'))
            last_sent = cmd
            if cmd == 's':
                print("[BRZDA] Všechny pásy stojí.")
            elif cmd == 'qe':
                print("[JÍZDA] Vpřed!")
            elif cmd == 'ad':
                print("[JÍZDA] Couváme!")
            elif cmd == 'ae':
                print("[TOČENÍ] Doleva!")
            elif cmd == 'qd':
                print("[TOČENÍ] Doprava!")
        time.sleep(0.05)
    except KeyboardInterrupt:
        ser.write(b's')
        break
    except Exception as e:
        print(f"Chyba při komunikaci: {e}")
        break
ser.close()
print("Sériový port odpojen. Konec.")
