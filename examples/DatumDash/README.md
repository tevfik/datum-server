# GeekMagic TV to Datum Converter

This example firmware converts the **GeekMagic TV** (ESP8266 + ST7789) device into a Datum IoT connected device.

## Hardware
This firmware is designed for the standard GeekMagic TV hardware:
- **MCU**: ESP8266 (ESP12F)
- **Display**: ST7789 240x240 IPS LCD
- **Pin Mapping**:
  - `MOSI`: GPIO 13 (D7)
  - `SCLK`: GPIO 14 (D5)
  - `DC`:   GPIO 0  (D3)
  - `RST`:  GPIO 2  (D4)
  - `BL`:   GPIO 5  (D1)

## Dependencies
You need to install the following libraries in Arduino IDE:
1.  **DatumIoT** (this library)
2.  **Adafruit GFX Library**
3.  **Adafruit ST7789 Library**

## Features
- Connects to WiFi and Datum IoT Server.
- **OTA Updates**: Supports flashing new firmware over-the-air.
- **Visual Status**: Displays connection status and OTA progress bar on the screen.

## How to Flash (Initial)
1.  Open this sketch in Arduino IDE.
2.  Edit `ssid` and `password` with your WiFi credentials.
3.  Select Board: `NodeMCU 1.0 (ESP-12E Module)`.
4.  Connect your GeekMagic TV via USB-UART adapter (if not already running an OTA-capable firmware, you might need to solder headers or use the USB port if available).
    *   *Note: Many GeekMagic devices have a USB-C port for power/programming.*
5.  Upload the sketch.

## How to Update (OTA)
Once this firmware is running:
1.  The device IP will be shown on the screen.
2.  In Arduino IDE, select the network port corresponding to `geekmagic-tv` at that IP.
3.  Upload your changes. The screen will show a progress bar during the update.

## Recovery / Original Firmware
If the device becomes unresponsive or "bricked", you may refer to the original firmware repository for recovery instructions or to flash the stock firmware:
- https://github.com/bvweerd/geekmagic-tv-esp8266?tab=readme-ov-file
