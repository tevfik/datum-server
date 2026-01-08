#!/bin/bash
# ESP32 Voice Command - Build and Flash Script
# Usage: ./build_flash.sh [port]

PORT=${1:-/dev/ttyUSB0}

echo "╔════════════════════════════════════════════════════════════╗"
echo "║       ESP32 Voice Command Demo - Build & Flash             ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""

# Check ESP-IDF
if [ -z "$IDF_PATH" ]; then
    echo "Error: ESP-IDF not found!"
    echo "Run: source ~/esp-idf/export.sh"
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
    echo "Flash failed!"
    exit 1
fi

# Monitor
echo ""
echo "Starting monitor (Ctrl+] to exit)..."
idf.py -p $PORT monitor
