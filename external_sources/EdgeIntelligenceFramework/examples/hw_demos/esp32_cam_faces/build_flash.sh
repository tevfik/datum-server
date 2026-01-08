#!/bin/bash
# ESP32 Build and Flash Script
# Usage: ./build_flash.sh [port]

PORT=${1:-/dev/ttyUSB0}
PROJECT_NAME="esp32_cam_faces"

echo "╔════════════════════════════════════════════════════════════╗"
echo "║       ESP32-CAM Face Detection - Build & Flash             ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""

# Check ESP-IDF
if [ -z "$IDF_PATH" ]; then
    echo "Error: ESP-IDF not found!"
    echo "Run: source /path/to/esp-idf/export.sh"
    exit 1
fi

echo "ESP-IDF: $IDF_PATH"
echo "Port: $PORT"
echo ""

# Build
echo "Building..."
idf.py build

if [ $? -ne 0 ]; then
    echo "Build failed!"
    exit 1
fi

# Flash
echo ""
echo "Flashing to $PORT..."
idf.py -p $PORT flash

if [ $? -ne 0 ]; then
    echo "Flash failed! Check connection and port."
    exit 1
fi

# Monitor
echo ""
echo "Starting monitor (Ctrl+] to exit)..."
idf.py -p $PORT monitor
