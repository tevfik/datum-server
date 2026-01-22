# Device Provisioning Tutorial

## Overview
This guide shows how to provision (register) a new IoT device with Datum Server.

## Prerequisites
- Datum Server running (default port 8000)
- User account created
- Python 3.8+ or Arduino IDE

## Steps

### 1. Create Device via API

```bash
# Using CLI (datumctl) - Uses Admin API
datumctl device create --name "Living Room Sensor" --type "temperature_humidity"

# Using API directly (User API)
# 1. Login to get auth token
curl -X POST http://localhost:8000/auth/login \
  -H "Content-Type: application/json" \
  -d '{"email": "user@example.com", "password": "your_password"}'
  
# Response: {"token": "eyJhbGc..."}

# 2. Create device
curl -X POST http://localhost:8000/dev \
  -H "Authorization: Bearer <YOUR_TOKEN>" \
  -H "Content-Type: application/json" \
  -d '{"name": "Living Room Sensor", "type": "temperature_humidity"}'

# Response: 
# {
#   "device_id": "dev_abc123def456",
#   "api_key": "dk_0123456789abcdef",
#   "message": "Save this API key - it won't be shown again"
# }
```

### 2. Configure Device Firmware

Save the `device_id` and `api_key` from step 1.

**Python Example:**
```python
# See tests/device_simulator.py for full example
import requests
import time

DEVICE_ID = "dev_abc123def456"
API_KEY = "dk_0123456789abcdef"
API_URL = "http://localhost:8000"

while True:
    data = {
        "temp": 25.5,
        "humidity": 60
    }
    
    # Send telemetry via HTTP
    response = requests.post(
        f"{API_URL}/dev/{DEVICE_ID}/data",
        json=data,
        headers={"Authorization": f"Bearer {API_KEY}"}
    )
    
    print(f"Sent data: {response.status_code}")
    time.sleep(60)  # Send every minute
```

**Arduino / ESP32 Example:**
See [Device Integration Guide](device_integration_guide.md) for detailed CPP/Arduino code.

### 3. Verify Device is Active

```bash
# List your devices
curl http://localhost:8000/dev \
  -H "Authorization: Bearer <YOUR_TOKEN>"

# Should show your device with "status": "online" (if it sent data)
```

## Next Steps
- See `tests/provisioning_test.py` for automated testing.
- See API documentation at http://localhost:8000/docs (if Swagger is enabled).
