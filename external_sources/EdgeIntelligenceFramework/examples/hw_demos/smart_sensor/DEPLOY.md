# Smart Sensor Hub - Deployment Guide

## Hardware

- ESP32-mini or ESP32
- DHT22 (temp/humidity)
- BH1750 (light sensor)
- Capacitive soil moisture sensor

## Wiring

| Sensor | ESP32 |
|--------|-------|
| DHT22 DATA | GPIO4 |
| BH1750 SDA | GPIO21 |
| BH1750 SCL | GPIO22 |
| Soil Moisture | GPIO34 (ADC) |

## Sensor Libraries

```bash
# Add to components/
git clone https://github.com/UncleRus/esp-idf-lib.git components/esp-idf-lib
```

## main/CMakeLists.txt

```cmake
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_adc dht bh1750
)
```

## MQTT Configuration

```c
// WiFi
#define WIFI_SSID "your_ssid"
#define WIFI_PASS "your_password"

// MQTT
#define MQTT_BROKER "mqtt://192.168.1.100"
#define MQTT_TOPIC "sensors/plant_monitor"
```

## Build & Flash

```bash
source ~/esp-idf/export.sh
./build_flash.sh /dev/ttyUSB0
```
