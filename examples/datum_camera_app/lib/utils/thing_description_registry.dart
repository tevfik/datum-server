import '../models/thing_description.dart';

class ThingDescriptionRegistry {
  
  static ThingDescription get(String deviceType) {
    switch (deviceType) {
      case 'camera':
        return _cameraTD;
      case 'relay_board':
        return _relayTD;
      default:
        return _genericTD;
    }
  }

  static final ThingDescription _cameraTD = ThingDescription.fromJson({
    "title": "Datum Camera",
    "device_type": "camera",
    "properties": {
      "status": {"type": "string", "description": "Status"},
      "uptime": {"type": "integer", "unit": "s", "description": "Uptime"},
      "rssi": {"type": "integer", "unit": "dBm", "description": "Signal Strength"},
      "local_ip": {"type": "string", "description": "Local IP"},
      "public_ip": {"type": "string", "description": "Global IP"},
      "ssid": {"type": "string", "description": "WiFi Network"},
      "bssid": {"type": "string", "description": "AP MAC"},
      "fw_ver": {"type": "string", "description": "Firmware Version"},
      "free_heap": {"type": "integer", "unit": "B", "description": "Free Memory"},
      "reset_reason": {"type": "string", "description": "Last Reset"}
    },
    "actions": {
      "snapshot": {"description": "Take Photo"},
      "set_resolution": {
        "description": "Set Stream Resolution",
        "input": {"type": "string", "enum": ["QVGA", "VGA", "SVGA", "HD"]}
      },
      "set_led": {
         "description": "LED Control",
         "input": {
             "type": "object",
             "properties": {
               "brightness": {"type": "integer", "min": 0, "max": 100},
               "color": {"type": "string", "format": "hex"}
             }
         }
      },
      "set_orientation": {
         "description": "Flip/Mirror",
         "input": {
             "type": "object",
             "properties": {
               "hmirror": {"type": "boolean"},
               "vflip": {"type": "boolean"}
             }
         }
      }
    }
  });

  static final ThingDescription _relayTD = ThingDescription.fromJson({
    "title": "Smart Relay Board",
    "device_type": "relay_board",
    "properties": {
      "relay_0": {"type": "boolean", "description": "Relay 1"},
      "relay_1": {"type": "boolean", "description": "Relay 2"},
      "relay_2": {"type": "boolean", "description": "Relay 3"},
      "relay_3": {"type": "boolean", "description": "Relay 4"},
      "wifi_rssi": {"type": "integer", "unit": "dBm", "description": "Signal"},
      "battery_adc": {"type": "integer", "description": "Battery"},
      "local_ip": {"type": "string", "description": "Local IP"},
      "public_ip": {"type": "string", "description": "Global IP"},
      "ssid": {"type": "string", "description": "WiFi Network"},
      "bssid": {"type": "string", "description": "AP MAC"},
      "channel": {"type": "integer", "description": "WiFi Ch"},
      "fw_ver": {"type": "string", "description": "Firmware"},
      "free_heap": {"type": "integer", "unit": "B", "description": "Free Memory"},
      "reset_reason": {"type": "string", "description": "Last Reset"}
    },
    "actions": {
      "relay_control": {"description": "Toggle Relays"}
    }
  });

  static final ThingDescription _genericTD = ThingDescription.fromJson({
    "title": "Generic Device",
    "device_type": "generic",
    "properties": {
      "rssi": {"type": "integer", "description": "Signal"}
    }
  });
}
