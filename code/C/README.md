# WALL-E Jetson Control Code

This directory contains the C++ control software for the WALL-E robot using Nvidia Jetson.

## Requirements
*   **Hardware:**
    *   Nvidia Jetson (Nano / Orin Nano)
    *   PCA9685 PWM Driver (Connected via I2C Bus 1)
    *   SteelSeries Rival 3 Wireless Mouse (or compatible)
    *   BTS7960 Motor Drivers

*   **Software Dependencies:**
    *   `i2c-tools` & `libi2c-dev`
    *   `libevdev-dev`
    *   `cmake`
    *   `g++`

    Install command:
    ```bash
    sudo apt-get update
    sudo apt-get install -y i2c-tools libi2c-dev libevdev-dev cmake g++
    ```

## Wiring (PCA9685)
*   **SDA:** Pin 3 (Bus 1)
*   **SCL:** Pin 5 (Bus 1)
*   **VCC:** 3.3V
*   **GND:** GND
*   **Motor L:** Channel 0 (Fwd), Channel 1 (Bwd)
*   **Motor R:** Channel 2 (Fwd), Channel 3 (Bwd)

## How to Build
1.  Navigate to this directory:
    ```bash
    cd /path/to/wall-e/code
    ```
2.  Create build directory:
    ```bash
    mkdir build && cd build
    ```
3.  Run CMake:
    ```bash
    cmake ..
    ```
4.  Compile:
    ```bash
    make
    ```

## How to Run
You need to find the specific event device for your mouse.
List devices:
```bash
ls /dev/input/by-id/
```
Look for something like `usb-SteelSeries_Rival_3_Wireless-event-mouse`.

Run the program (needs root for I2C and Input access):
```bash
sudo ./walle_control /dev/input/by-id/usb-SteelSeries_Rival_3_Wireless-event-mouse
```

## Controls
*   **Left Click:** Move Forward (Smooth Acceleration)
*   **Right Click:** Move Backward
*   **Scroll Up:** Turn Left
*   **Scroll Down:** Turn Right
