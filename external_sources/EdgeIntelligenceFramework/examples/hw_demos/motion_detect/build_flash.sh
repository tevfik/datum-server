#!/bin/bash
# Motion Detection - Build and Flash Script

PORT=${1:-/dev/ttyUSB0}

echo "╔════════════════════════════════════════════════════════════╗"
echo "║       Motion Detection - Build & Flash                     ║"
echo "╚════════════════════════════════════════════════════════════╝"

if [ -z "$IDF_PATH" ]; then
    echo "Error: ESP-IDF not found! Run: source ~/esp-idf/export.sh"
    exit 1
fi

idf.py build && idf.py -p $PORT flash monitor
