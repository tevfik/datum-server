import subprocess
import json
import time
from datetime import datetime, timedelta
import random
import math
import os
import sys

# User-specific paths (updated based on previous interaction)
DATUMCTL = "build/binaries/datumctl"
SERVER = "https://datum.bezg.in"
EMAIL = "tevfik.kadioglu@gmail.com"
PASSWORD = "t0751234k" 

def run_cmd(args):
    # Capture output
    cmd = [DATUMCTL, "--server", SERVER] + args
    try:
        res = subprocess.check_output(cmd, text=True, stderr=subprocess.STDOUT)
        return res
    except subprocess.CalledProcessError as e:
        # print(f"Error running {' '.join(cmd)}: {e.output}")
        raise e

def main():
    if not os.path.exists(DATUMCTL):
        print(f"Error: {DATUMCTL} binary not found. Please build it first (make build-cli)")
        sys.exit(1)

    print(f"Using server: {SERVER}")
    
    # 1. Login
    print("--- 1. Authenticating ---")
    try:
        # Register might fail if exists
        try:
            run_cmd(["register", "--email", EMAIL, "--password", PASSWORD])
        except:
            pass
            
        run_cmd(["login", "--email", EMAIL, "--password", PASSWORD])
        print("Logged in successfully.")
    except Exception as e:
        print(f"Login failed: {e}")
        return

    # 2. Re-create Device to get API Key
    # We must delete it first to get a fresh key, as we cannot retrieve key of existing device.
    print("\n--- 2. Re-creating Device to get API Key ---")
    dev_id = "sample-sensor-01"
    try:
        run_cmd(["device", "delete", dev_id, "--force"])
        print(f"Deleted old device {dev_id} (if existed)")
    except:
        pass 

    API_KEY = ""
    try:
        res = run_cmd(["device", "create", "--id", dev_id, "--name", "Sample Sensor", "--json"])
        dev_data = json.loads(res)
        API_KEY = dev_data.get("api_key")
        print(f"Created device: {dev_id}, Key: {API_KEY}")
    except Exception as e:
        print(f"Failed to create device: {e}")
        return

    if not API_KEY:
        print("Error: Could not retrieve API Key. Cannot post data.")
        return

    # 2.5 Logout to ensure Token is not used (Client prefers Token over Key)
    print("Logging out to force API Key usage...")
    try:
        run_cmd(["logout"])
    except:
        pass

    # 3. Populate Data
    print("\n--- 3. Populating Data (Last 24h) ---")
    now = datetime.now()
    start_time = now - timedelta(hours=24)
    current = start_time
    count = 0
    
    while current < now:
        ts_str = current.strftime("%Y-%m-%dT%H:%M:%SZ")
        
        # Simulate sensor data - richer set
        hour = current.hour
        
        # Temperature: Daily cycle + random noise
        temp_cycle = 20 + 5 * math.sin((hour - 6) * 2 * math.pi / 24) 
        temp = temp_cycle + random.uniform(-0.5, 0.5)
        
        # Humidity: Inverse to temp
        hum = 60 - 15 * math.sin((hour - 6) * 2 * math.pi / 24) + random.uniform(-2, 2)
        
        # Voltage: Slow discharge
        volt = 4.2 - (0.005 * (count % 200)) # Slower discharge cycle
        if volt < 3.3: volt = 4.2
        
        # RSSI (Signal Strength): Random fluctuation
        rssi = -60 + random.uniform(-10, 5)
        
        # CPU Usage: Load spikes during "day"
        cpu_load = 10 + random.uniform(0, 5)
        if 9 <= hour <= 18:
            cpu_load += random.uniform(20, 40) # Busy during day
        
        data_payload = json.dumps({
            "timestamp": ts_str, 
            "temperature": round(temp, 2),
            "humidity": round(hum, 1),
            "battery_voltage": round(volt, 2),
            "wifi_signal": int(rssi),
            "cpu_usage": round(cpu_load, 1)
        })
        
        try:
            # IMPORTANT: Use --api-key to Authenticate as Device
            subprocess.check_call(
                [DATUMCTL, "--server", SERVER, "data", "post", "--device", dev_id, "--api-key", API_KEY, "--data", data_payload], 
                stdout=subprocess.DEVNULL
            )
            
            if count % 20 == 0:
                print(f"Posted {ts_str} -> T:{round(temp,1)} C, CPU:{int(cpu_load)}%")
        except Exception as e:
            print(f"Failed to post {ts_str}: {e}")
            
        current += timedelta(minutes=5) # 5-minute interval for denser data
        count += 1
        
    print(f"\nDone! Posted ~{count} data points for {dev_id}.")
    print("Go to Data Explorer in Web UI to verify.")

if __name__ == "__main__":
    main()
