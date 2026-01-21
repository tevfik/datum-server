# TODO List

## Firmware Updates
- [ ] **Remote Config Support**: Port Remote Config (Shadow/Desired State) implementation from `esp32_camera_mqtt` to other examples:
    - `examples/arduino/datum_device.ino`
    - `examples/esp8266_relay_board/`
    - `examples/esp32_mqtt_sensor/`

## Backend
- [ ] **Database Migration**: Ensure production PostgreSQL databases execute `ALTER TABLE devices ADD COLUMN desired_state JSONB;`.
