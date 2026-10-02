import hid
import time
import threading

class LogitechAnalogMouse:
    def __init__(self):
        self.VID = 0x046D
        self.PIDs = [0xC54D, 0xC0A8] # Wireless and Wired PIDs
        self.dev = None
        self.running = False
        self._thread = None
        
        self.left_pressure = 0.0
        self.right_pressure = 0.0

        # HID++ Constants
        self.DEVICE_INDEX = 0x01 # 0xFF for wired, but we'll try 0x01/0xFF dynamically
        self.PRESSURE_FEATURE_INDEX = 0x0C
        self.REPORT_LONG = 0x11

    def connect(self):
        """Finds and connects to the Logitech Superstrike mouse."""
        for device_info in hid.enumerate(self.VID, 0):
            if device_info['product_id'] in self.PIDs and device_info['interface_number'] == 2:
                self.dev = hid.device()
                self.dev.open_path(device_info['path'])
                self.dev.set_nonblocking(True)
                self.DEVICE_INDEX = 0xFF if device_info['product_id'] == 0xC0A8 else 0x01
                print(f"Připojeno k myši: {device_info['product_string']}")
                return True
        print("Myš nenalezena. Ujistěte se, že je připojena.")
        return False

    def _send_lease_request(self):
        """Sends a request to the mouse to start streaming analog data for 8 seconds."""
        flags = 0x02
        lease_seconds = 8
        # address = (function_id=3 << 4) | sw_id=0x08
        address = 0x38
        
        payload = [flags, lease_seconds]
        # Pad payload to 16 bytes
        payload.extend([0] * (16 - len(payload)))
        
        report = [self.REPORT_LONG, self.DEVICE_INDEX, self.PRESSURE_FEATURE_INDEX, address] + payload
        try:
            self.dev.write(report)
        except Exception as e:
            print(f"Chyba při odesílání požadavku: {e}")

    def _read_loop(self):
        last_lease_time = 0
        
        while self.running:
            now = time.time()
            # Renew lease every 2 seconds
            if now - last_lease_time > 2.0:
                self._send_lease_request()
                last_lease_time = now
                
            try:
                data = self.dev.read(64)
            except OSError:
                time.sleep(0.01)
                continue

            if data and len(data) >= 20:
                # Parse HID++ Pressure Notification
                if data[0] == self.REPORT_LONG and data[2] == self.PRESSURE_FEATURE_INDEX:
                    addr = data[3]
                    if addr == 0x10: # MODE3_ADDR
                        # Left button ADC (10-bit)
                        left_raw = (data[4] << 8 | data[5]) >> 6
                        # Right button ADC (10-bit)
                        right_raw = (data[6] << 8 | data[7]) >> 6
                        
                        # Normalize 0-1024 to 0.0 - 1.0 (Approximate active range is usually ~300 to ~900)
                        # Přemapování tak, aby 0 byla puštěná a 1 plně stisknutá
                        left_clamped = max(0, min(1000, left_raw - 300))
                        right_clamped = max(0, min(1000, right_raw - 300))
                        
                        self.left_pressure = left_clamped / 700.0
                        self.right_pressure = right_clamped / 700.0

            time.sleep(0.001)

    def start(self):
        """Starts the background thread reading mouse data."""
        if not self.connect():
            return
        
        self.running = True
        self._thread = threading.Thread(target=self._read_loop, daemon=True)
        self._thread.start()
        print("Snímání tlaku spuštěno...")

    def stop(self):
        """Stops reading and releases the device."""
        self.running = False
        if self._thread:
            self._thread.join()
        if self.dev:
            self.dev.close()
            print("Myš odpojena.")

# ==========================================
# PŘÍKLAD POUŽITÍ (Test na Jetsonu / PC)
# ==========================================
if __name__ == "__main__":
    mouse = LogitechAnalogMouse()
    mouse.start()
    
    try:
        while True:
            # Tady by byl kód pro řízení motorů Wall-Eho
            # Např. motor_speed = int(mouse.left_pressure * 255)
            print(f"Levé tlačítko: {mouse.left_pressure*100:5.1f}% | Pravé tlačítko: {mouse.right_pressure*100:5.1f}%", end="\r")
            time.sleep(0.05)
    except KeyboardInterrupt:
        pass
    finally:
        mouse.stop()
