# API Reference

## Authentication

### Register
```http
POST /auth/register
Content-Type: application/json

{"email": "user@example.com", "password": "min8chars"}
```
Response: `{"user_id": "usr_xxx", "token": "jwt..."}`

### Login
```http
POST /auth/login
Content-Type: application/json

{"email": "user@example.com", "password": "min8chars"}
```
Response: `{"token": "jwt...", "user_id": "usr_xxx", "role": "admin|user"}`

---

## System

### Check System Status
```http
GET /system/status
```
Response: `{"initialized": true, "platform_name": "...", "allow_register": true}`

> For admin endpoints, see [ADMIN.md](./ADMIN.md)

---

## Devices

### Create Device
```http
POST /devices
Authorization: Bearer {jwt_token}
Content-Type: application/json

{"name": "My Sensor", "type": "temperature"}
```
Response: `{"device_id": "dev_xxx", "api_key": "sk_live_xxx"}`

### List Devices
```http
GET /devices
Authorization: Bearer {jwt_token}
```

### Delete Device
```http
DELETE /devices/{device_id}
Authorization: Bearer {jwt_token}
```

---

## Data Ingestion

### Send Data (Authenticated - POST)
```http
POST /data/{device_id}
Authorization: Bearer {api_key}
Content-Type: application/json

{"temperature": 25.5, "humidity": 60}
```
Response: `{"status": "ok", "commands_pending": 0}`

### Send Data (Authenticated - GET for Constrained Devices)
For devices that cannot make POST requests with JSON bodies (ESP8266, Arduino, etc.):
```http
GET /device/{device_id}/push?key={api_key}&temp=25.5&humidity=60&battery=90
```
Response: `{"status": "ok", "fields_stored": 3, "commands_pending": 0}`

> **Note:** The `key` parameter is used for authentication and is not stored as sensor data. All other query parameters are automatically converted to appropriate types (numbers, booleans, strings).

### Send Data (Public)
```http
POST /public/data/{device_id}
Content-Type: application/json

{"temperature": 25.5}
```

---

## Data Query

### Get Latest
```http
GET /data/{device_id}
Authorization: Bearer {jwt_token}
```

### Get History
```http
GET /data/{device_id}/history?limit=100
Authorization: Bearer {jwt_token}
```

### Advanced Time Range Query
```http
GET /data/{device_id}/history?start=1766439614000&stop=1h&int=1m
Authorization: Bearer {jwt_token}
```

| Param | Description | Example |
|-------|-------------|---------|
| `start` | Unix timestamp (ms) | `1766439614000` |
| `stop` | Duration from start | `1d`, `1h`, `1m`, `1s` |
| `range` | Duration back from now | `1h`, `24h`, `7d`, `30d` |
| `int` | Aggregation interval | `1d`, `1h`, `1m`, `1s` |
| `limit` | Max results | `1000` (default) |
| `start_rfc` | RFC3339 start | `2025-12-22T20:00:00Z` |
| `end_rfc` | RFC3339 end | `2025-12-22T21:00:00Z` |

**Response:**
```json
{
  "device_id": "dev_xxx",
  "count": 24,
  "data": [
    {"timestamp": "...", "timestamp_ms": 1766439614000, "data": {"temperature": 25.5}}
  ],
  "range": {"start": "...", "start_ms": 1766439000000, "end": "...", "end_ms": 1766443200000},
  "interval": "1h0m0s"
}
```

**Examples:**
```bash
# Last hour, raw data
GET /data/{id}/history?range=1h

# From unix timestamp, 1 hour duration
GET /data/{id}/history?start=1766439614000&stop=1h

# Last 24 hours, averaged per hour
GET /data/{id}/history?range=24h&int=1h

# Last 7 days, averaged per day
GET /data/{id}/history?range=7d&int=1d
```

### Public Endpoints
```http
GET /public/data/{device_id}
GET /public/data/{device_id}/history?limit=100
```

---

## Commands

### Send Command (User → Device)
```http
POST /devices/{device_id}/commands
Authorization: Bearer {jwt_token}
Content-Type: application/json

{"action": "reboot", "params": {"delay": 5}}
```

### Poll Commands (Device)
```http
GET /device/{device_id}/commands
GET /device/{device_id}/commands/poll?wait=30
GET /device/{device_id}/commands/stream?wait=30
Authorization: Bearer {api_key}
```

### Acknowledge Command
```http
POST /device/{device_id}/commands/{command_id}/ack
Authorization: Bearer {api_key}
Content-Type: application/json

{"status": "success", "result": {"executed": true}}
```

---

## Analytics (Port 8001)

```http
GET /analytics/{device_id}/stats
GET /analytics/{device_id}/hourly?field=temperature
GET /analytics/{device_id}/daily?field=temperature
GET /analytics/{device_id}/anomalies?field=temperature&threshold=2.0
GET /public/analytics/{device_id}/stats
```

