"""
Datumpy IoT Device Simulator

Python example for devices to send data and receive commands.
Works with MicroPython (ESP32, Raspberry Pi Pico) or standard Python.

Usage:
    python device_simulator.py --device-id dev_xxx --api-key sk_live_xxx

Environment variables:
    DATUMPY_API_URL - API endpoint (default: http://localhost:8007)
    DEVICE_ID - Device ID
    API_KEY - Device API key
"""

import os
import sys
import time
import json
import random
import argparse

try:
    import requests
except ImportError:
    print("Installing requests...")
    os.system("pip install requests")
    import requests

# Configuration
API_URL = os.environ.get("DATUMPY_API_URL", "http://localhost:8007")
DEVICE_ID = os.environ.get("DEVICE_ID", "")
API_KEY = os.environ.get("API_KEY", "")

def send_data(device_id: str, api_key: str, data: dict) -> dict:
    """Send sensor data to the platform"""
    url = f"{API_URL}/data/{device_id}"
    headers = {
        "Authorization": f"Bearer {api_key}",
        "Content-Type": "application/json"
    }
    
    try:
        response = requests.post(url, json=data, headers=headers, timeout=10)
        return response.json()
    except Exception as e:
        return {"error": str(e)}

def poll_commands(device_id: str, api_key: str, wait: int = 30) -> list:
    """Long poll for pending commands"""
    url = f"{API_URL}/device/{device_id}/commands/poll?wait={wait}"
    headers = {"Authorization": f"Bearer {api_key}"}
    
    try:
        response = requests.get(url, headers=headers, timeout=wait + 5)
        data = response.json()
        return data.get("commands", [])
    except Exception as e:
        print(f"Poll error: {e}")
        return []

def acknowledge_command(device_id: str, api_key: str, command_id: str, result: dict) -> dict:
    """Acknowledge command execution"""
    url = f"{API_URL}/device/{device_id}/commands/{command_id}/ack"
    headers = {
        "Authorization": f"Bearer {api_key}",
        "Content-Type": "application/json"
    }
    
    try:
        response = requests.post(url, json={"status": "success", "result": result}, headers=headers)
        return response.json()
    except Exception as e:
        return {"error": str(e)}

def execute_command(cmd: dict) -> dict:
    """Execute a command and return result"""
    action = cmd.get("action", "unknown")
    params = cmd.get("params", {})
    
    print(f"  Executing: {action} with params: {params}")
    
    if action == "reboot":
        print("  [SIMULATED] Rebooting device...")
        return {"rebooted": True, "delay": params.get("delay", 0)}
    
    elif action == "set_interval":
        seconds = params.get("seconds", 60)
        print(f"  [SIMULATED] Setting interval to {seconds}s")
        return {"new_interval": seconds}
    
    elif action == "update_firmware":
        print("  [SIMULATED] Updating firmware...")
        return {"updated": True, "version": params.get("version", "1.0.0")}
    
    else:
        print(f"  Unknown action: {action}")
        return {"unknown_action": action}

def simulate_sensors() -> dict:
    """Generate fake sensor data"""
    return {
        "temperature": round(20 + random.random() * 10, 2),
        "humidity": round(40 + random.random() * 30, 1),
        "battery": round(70 + random.random() * 30, 1),
        "uptime": int(time.time()) % 86400
    }

def main():
    parser = argparse.ArgumentParser(description="Datumpy Device Simulator")
    parser.add_argument("--device-id", default=DEVICE_ID, help="Device ID")
    parser.add_argument("--api-key", default=API_KEY, help="API Key")
    parser.add_argument("--interval", type=int, default=10, help="Data send interval (seconds)")
    parser.add_argument("--api-url", default=API_URL, help="API URL")
    args = parser.parse_args()
    
    global API_URL
    API_URL = args.api_url
    
    if not args.device_id or not args.api_key:
        print("Error: --device-id and --api-key are required")
        print("Usage: python device_simulator.py --device-id dev_xxx --api-key sk_live_xxx")
        sys.exit(1)
    
    print(f"🔌 Datumpy Device Simulator")
    print(f"   Device ID: {args.device_id}")
    print(f"   API URL: {API_URL}")
    print(f"   Interval: {args.interval}s")
    print()
    
    while True:
        # 1. Send sensor data
        data = simulate_sensors()
        print(f"📤 Sending: {data}")
        result = send_data(args.device_id, args.api_key, data)
        print(f"   Response: {result}")
        
        # 2. Check for pending commands
        commands_pending = result.get("commands_pending", 0)
        if commands_pending > 0:
            print(f"📥 {commands_pending} command(s) pending, polling...")
            commands = poll_commands(args.device_id, args.api_key, wait=5)
            
            for cmd in commands:
                cmd_id = cmd.get("command_id")
                print(f"   Command: {cmd_id}")
                
                # Execute command
                exec_result = execute_command(cmd)
                
                # Acknowledge
                ack = acknowledge_command(args.device_id, args.api_key, cmd_id, exec_result)
                print(f"   Ack: {ack}")
        
        print()
        time.sleep(args.interval)

if __name__ == "__main__":
    main()
