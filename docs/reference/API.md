# API Reference

## Common Headers

All API responses include the following headers:

| Header | Description |
|--------|-------------|
| `X-Request-ID` | Unique request identifier (UUID v4). Clients can provide their own value; otherwise the server generates one. Useful for log correlation and debugging. |

## Request Limits

| Limit | Default | Env Var | Description |
|-------|---------|---------|-------------|
| Max Body Size | 5 MB | `MAX_REQUEST_BODY_BYTES` | Maximum HTTP request body size. Requests exceeding this are rejected with 413. |
| Query Limit | 10,000 | `QUERY_MAX_LIMIT` | Maximum data points returned per history query. |
| Batch Size | 50,000 | *(fixed)* | Maximum data points per ingestion batch. Batches exceeding this are rejected with 400. |

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

## System (Initial Setup & Status)

### Check System Status
```http
GET /sys/status
```
Response: `{"initialized": true, "platform_name": "...", "allow_register": true}`

### Setup System
```http
POST /sys/setup
Content-Type: application/json

{"platform_name": "My IoT", "admin_email": "admin@example.com", "admin_password": "..."}
```
*Used only once to initialize the first admin.*

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
| `int` | Aggregation interval | `1m`, `1h`, `1d` || `limit` | Max results (default: 10000, configurable via `QUERY_MAX_LIMIT`) | `5000` |
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

### Time Sync
```http
GET /sys/time
```
Response: `{"unix": 1700000000, "iso": "2024-..."}`

### Health Check
```http
GET /health
GET /ready
```

