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

### Password Reset
```http
POST /auth/forgot-password
POST /auth/reset-password
PUT /auth/password (Authenticated)
```

---

## System

### Get Server Time
```http
GET /sys/time
```
Response: `{"unix": 1700000000, "iso": "2024-..."}`

### Get Client IP
```http
GET /sys/ip
```
Response: `192.168.1.1`

---

## Admin Endpoints
*(Require Admin Role - Authorization: Bearer JWT)*

### User Management
```http
POST /admin/users
GET /admin/users
GET /admin/users/{user_id}
PUT /admin/users/{user_id}
DELETE /admin/users/{user_id}
POST /admin/users/{username}/reset-password
```

### Device Management (Admin)
```http
GET /admin/dev
POST /admin/dev
GET /admin/dev/{device_id}
PUT /admin/dev/{device_id}
DELETE /admin/dev/{device_id}
```

### Key Management
```http
POST /admin/dev/{device_id}/rotate-key
POST /admin/dev/{device_id}/revoke-key
```

### System Configuration
```http
GET /admin/config
PUT /admin/config
PUT /admin/config/retention
PUT /admin/config/rate-limit
PUT /admin/config/alerts
```

### Logs & MQTT
```http
GET /admin/logs
DELETE /admin/logs
GET /admin/mqtt/stats
GET /admin/mqtt/clients
```

---

## User Device Management
*(User Context - Authorization: Bearer JWT)*

### Register Device
```http
POST /dev/register
Content-Type: application/json

{
  "device_uid": "mac_addr_or_id",
  "device_name": "Living Room Sensor"
}
```

### List Devices
```http
GET /dev
Authorization: Bearer {jwt_token}
```

### Create Device (Manual)
```http
POST /dev
Content-Type: application/json
{"name": "New Device", "type": "sensor"}
```

### Get Device
```http
GET /dev/{device_id}
```

### Delete Device
```http
DELETE /dev/{device_id}
```

---

## Device Operations (Data & Commands)
*(Hybrid Auth - Supports JWT (User) or API Key (Device))*

### Send Data (POST)
```http
POST /dev/{device_id}/data
Authorization: Bearer {api_key}
Content-Type: application/json

{"temperature": 25.5, "humidity": 60}
```
Response: `{"status": "ok", "commands_pending": 0}`

### Send Data (GET - For Constrained Devices)
For constrained devices (e.g., Simple Arduino/ESP8266 without JSON lib):
```http
GET /dev/{device_id}/data?temp=25.5&humidity=60&battery=90
Authorization: Bearer {api_key}
```
*Note: Some client libraries may append the key as a query param if header auth is not supported, but Header is preferred.*

### Get Latest Data
```http
GET /dev/{device_id}/data
```

### Get Data History
```http
GET /dev/{device_id}/data/history?last=1h
GET /dev/{device_id}/data/history?start=1766439614000&stop=1h&int=1m
```

| Param | Description | Example |
|-------|-------------|---------|
| `start` | Unix timestamp (ms) | `1766439614000` |
| `stop` | Duration | `1h`, `15m` |
| `last` | Quick range from now | `24h`, `7d` |
| `int` | Aggregation interval | `1m`, `1h`, `1d` |

---

## Device Commands

### Send Command (User sends to Device)
```http
POST /dev/{device_id}/cmd
Authorization: Bearer {jwt_token}
Content-Type: application/json

{"action": "reboot", "params": {"delay": 5}}
```

### List Commands
```http
GET /dev/{device_id}/cmd
```

### Poll Pending Commands (Device reads)
```http
GET /dev/{device_id}/cmd/pending
Authorization: Bearer {api_key}
```
Response: `{"commands": [{"id": "...", "action": "...", "params": {...}}]}`

### Acknowledge Command (Device confirms)
```http
POST /dev/{device_id}/cmd/{command_id}/ack
Authorization: Bearer {api_key}
Content-Type: application/json

{"status": "success", "result": {"executed": true}}
```

---

## Provisioning

### Generate Request (App)
```http
POST /dev/register
Authorization: Bearer {jwt_token}
```

### Activate Device (Device)
```http
POST /prov/activate
Content-Type: application/json

{"device_uid": "..."}
```

---

## Public Endpoints
*(No Auth Required)*

### Public Data Ingestion
```http
POST /pub/{device_id}
Content-Type: application/json
```

### Public Data Read
```http
GET /pub/{device_id}
```

---

## Miscellaneous

### Health Check
```http
GET /health
GET /ready
```

