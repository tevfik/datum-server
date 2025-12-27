# Device Provisioning Tutorial

## Overview
This guide shows how to provision (register) a new IoT device with Datumpy.

## Prerequisites
- Datumpy server running (see main README)
- User account created
- Python 3.8+ or Arduino IDE

## Steps

### 1. Create Device via API

```bash
# Login to get auth token
curl -X POST http://localhost:8000/auth/login \
  -H "Content-Type: application/json" \
  -d '{"email": "user@example.com", "password": "your_password"}'

# Response: {"token": "eyJhbGc..."}

# Create device
curl -X POST http://localhost:8000/devices \
  -H "Authorization: Bearer YOUR_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"name": "Living Room Sensor", "type": "temperature_humidity"}'

# Response: 
# {
#   "device_id": "dev_abc123",
#   "api_key": "sk_live_xyz789..."
# }
```

### 2. Configure Device Firmware

Save the `device_id` and `api_key` from step 1.

**Python Example:**
```python
# See tests/device_simulator.py for full example
import requests
import time

DEVICE_ID = "dev_abc123"
API_KEY = "sk_live_xyz789..."
API_URL = "http://localhost:8000"

while True:
    data = {
        "temp": 25.5,
        "humidity": 60
    }
    
    response = requests.post(
        f"{API_URL}/data/{DEVICE_ID}",
        json=data,
        headers={"Authorization": f"Bearer {API_KEY}"}
    )
    
    print(f"Sent data: {response.status_code}")
    time.sleep(60)  # Send every minute
```

**Arduino Example:**
```cpp
// Coming soon in Phase 2
```

### 3. Verify Device is Active

```bash
# List your devices
curl http://localhost:8000/devices \
  -H "Authorization: Bearer YOUR_TOKEN"

# Should show your device with "status": "online"
```

## Next Steps
- See `tests/provisioning_test.py` for automated testing
- See API documentation at http://localhost:8000/docs
