# Datum IoT — Client Libraries

This directory contains client libraries for integrating with the Datum IoT Platform.

| Library | Language | Target | Path |
|---------|----------|--------|------|
| **DatumIoT** | C++ / Arduino | ESP8266, ESP32, Arduino | [`DatumIoT/`](DatumIoT/) |
| **datum-iot** | Python 3.9+ | Server / Edge / Scripts | [`python/`](python/) |
| **datum_sdk** | Dart / Flutter | Mobile (iOS, Android) | [`../sdk/dart/`](../sdk/dart/) |
| **@datum-iot/sdk** | TypeScript | Web / Node.js | [`../sdk/ts/`](../sdk/ts/) |

## Quick Links

- **Arduino/ESP**: Install `DatumIoT/` via the Arduino IDE Library Manager (zip) or PlatformIO
  `lib_deps = datum-iot/DatumIoT`.
- **Python**: `pip install ./python` (local) or `pip install datum-iot` (PyPI).
- **Dart**: Add `datum_sdk` to your `pubspec.yaml` (see `sdk/dart/README.md`).
- **TypeScript**: `npm install @datum-iot/sdk` or link from `sdk/ts/`.

## CA Certificate / TLS

The Arduino firmwares ship `datum_ca.h` which includes the ISRG Root X1 certificate used
by Let's Encrypt. To enable certificate verification in your firmware build:

```ini
; platformio.ini
build_flags = -DDATUM_TLS_VERIFY=1
```

Set `DATUM_TLS_VERIFY=0` (default) to skip verification during development.
