"""
Datum IoT Platform - MicroPython Client

For ESP32/ESP8266/Raspberry Pi Pico with MicroPython firmware.

Upload this file as main.py to your device.

Configuration:
    Edit the CONFIG section below with your credentials.
"""

import network
import urequests as requests
import ujson as json
import time
import machine

# ============ CONFIGURATION ============
CONFIG = {
    "wifi_ssid": "your-wifi-ssid",
    "wifi_password": "your-wifi-password",
    "api_url": "http://your-server:8000",
    "device_id": "dev_xxxxxxxxxxxxxxxx",
    "api_key": "sk_live_xxxxxxxxxxxxxxxx",
    "send_interval": 60,  # seconds
}
# =======================================

def connect_wifi():
    """Connect to WiFi network"""
    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)
    
    if not wlan.isconnected():
        print(f"Connecting to {CONFIG['wifi_ssid']}...")
        wlan.connect(CONFIG["wifi_ssid"], CONFIG["wifi_password"])
        
        timeout = 30
        while not wlan.isconnected() and timeout > 0:
            time.sleep(1)
            timeout -= 1
        
        if wlan.isconnected():
            print(f"Connected! IP: {wlan.ifconfig()[0]}")
        else:
            print("Failed to connect to WiFi")
            machine.reset()
    
    return wlan

def read_sensors():
    """Read sensor data (replace with your actual sensors)"""
    import random
    return {
        "temperature": 20 + random.random() * 10,
        "humidity": 40 + random.random() * 30,
        "battery": 70 + random.random() * 30,
    }

def send_data(data):
    """Send sensor data to Datum API"""
    url = f"{CONFIG['api_url']}/dev/{CONFIG['device_id']}/data"
    headers = {
        "Authorization": f"Bearer {CONFIG['api_key']}",
        "Content-Type": "application/json"
    }
    
    try:
        response = requests.post(url, json=data, headers=headers)
        result = response.json()
        response.close()
        return result
    except Exception as e:
        print(f"Send error: {e}")
        return {"error": str(e)}

def poll_commands(wait=5):
    """Poll for pending commands"""
    url = f"{CONFIG['api_url']}/dev/{CONFIG['device_id']}/cmd/poll?wait={wait}"
    headers = {"Authorization": f"Bearer {CONFIG['api_key']}"}
    
    try:
        response = requests.get(url, headers=headers)
        result = response.json()
        response.close()
        return result.get("commands", [])
    except Exception as e:
        print(f"Poll error: {e}")
        return []

def acknowledge_command(cmd_id, result):
    """Acknowledge command execution"""
    url = f"{CONFIG['api_url']}/dev/{CONFIG['device_id']}/cmd/{cmd_id}/ack"
    headers = {
        "Authorization": f"Bearer {CONFIG['api_key']}",
        "Content-Type": "application/json"
    }
    
    try:
        response = requests.post(url, json={"status": "success", "result": result}, headers=headers)
        response.close()
    except Exception as e:
        print(f"Ack error: {e}")

def execute_command(cmd):
    """Execute a command and return result"""
    action = cmd.get("action", "unknown")
    params = cmd.get("params", {})
    
    print(f"  Executing: {action}")
    
    if action == "reboot":
        print("  Rebooting...")
        time.sleep(1)
        machine.reset()
    
    elif action == "set_interval":
        CONFIG["send_interval"] = params.get("seconds", 60)
        return {"new_interval": CONFIG["send_interval"]}
    
    return {"executed": action}

def main():
    print("\n🔌 Datumpy MicroPython Device")
    print(f"   Device ID: {CONFIG['device_id']}")
    
    wlan = connect_wifi()
    
    while True:
        # Reconnect if needed
        if not wlan.isconnected():
            wlan = connect_wifi()
        
        # Read and send data
        data = read_sensors()
        print(f"📤 Sending: {data}")
        result = send_data(data)
        print(f"   Response: {result}")
        
        # Check for commands
        commands_pending = result.get("commands_pending", 0)
        if commands_pending > 0:
            print(f"📥 {commands_pending} command(s) pending")
            commands = poll_commands(wait=5)
            
            for cmd in commands:
                cmd_id = cmd.get("command_id")
                exec_result = execute_command(cmd)
                acknowledge_command(cmd_id, exec_result)
        
        time.sleep(CONFIG["send_interval"])

if __name__ == "__main__":
    main()
