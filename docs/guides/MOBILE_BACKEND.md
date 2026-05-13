# Datum as a Mobile Backend

This guide describes how the **datum-server** serves as a complete backend for mobile applications. It covers every feature that the mobile client uses, from authentication to real-time data streaming.

---

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Authentication](#authentication)
3. [Device Management](#device-management)
4. [Telemetry & Data Ingestion](#telemetry--data-ingestion)
5. [Real-time Updates (SSE)](#real-time-updates-sse)
6. [Rule Engine](#rule-engine)
7. [Push Notifications (ntfy)](#push-notifications-ntfy)
8. [Document Store](#document-store)
9. [Commands API](#commands-api)
10. [File Storage (Buckets)](#file-storage-buckets)
11. [OTA Firmware Updates](#ota-firmware-updates)
12. [API Key Management](#api-key-management)
13. [Provisioning](#provisioning)
14. [Web of Things (WoT)](#web-of-things-wot)
15. [Webhooks](#webhooks)

---

## Architecture Overview

```
Mobile App
    │
    ├── REST API (HTTP/JSON) ──────> datum-server
    │       /auth                       │
    │       /dev                        ├── BuntDB (metadata, shadow state)
    │       /data                       ├── tstorage (time-series data)
    │       /db                         ├── MQTT broker (embedded)
    │       /dev/:id/commands           ├── Rule Engine (events → actions)
    │       /dev/:id/stream             └── ntfy (push notifications)
    │
    └── SSE Stream ───────────────> /dev/:id/events
```

The server exposes a single base URL (configurable via `PUBLIC_URL`). All endpoints use JSON over HTTP/HTTPS. Authentication is JWT-based for users and API-key-based for devices.

---

## Authentication

### Registration & Login

| Method | Endpoint | Description |
|--------|----------|-------------|
| `POST` | `/auth/register` | Create a new user account |
| `POST` | `/auth/login` | Obtain a JWT token |
| `POST` | `/auth/refresh` | Refresh an expired JWT token |
| `POST` | `/auth/logout` | Invalidate the current token |
| `GET`  | `/auth/me` | Get current user profile |
| `PUT`  | `/auth/me` | Update user profile |
| `POST` | `/auth/change-password` | Change password |

**JWT Token Usage:**
```
Authorization: Bearer <jwt_token>
```

### Password Reset Flow
1. `POST /auth/forgot-password` with `{"email": "user@example.com"}`
2. User receives a reset link via email
3. `POST /auth/reset-password` with `{"token": "...", "new_password": "..."}`

See [docs/guides/MOBILE_AUTH.md](MOBILE_AUTH.md) and [docs/guides/PASSWORD_RESET.md](PASSWORD_RESET.md) for full details.

### API Keys (for device-level access)

Users can create named API keys with device-level permissions:

```
POST /auth/keys
{"name": "my-sensor-key"}

DELETE /auth/keys/:key_id
GET    /auth/keys
```

---

## Device Management

Base path: `/dev` — requires JWT (`Authorization: Bearer`).

| Method | Endpoint | Description |
|--------|----------|-------------|
| `POST` | `/dev` | Create/register a new device |
| `GET`  | `/dev` | List all devices for the current user |
| `GET`  | `/dev/:device_id` | Get device details + shadow state |
| `PUT`  | `/dev/:device_id` | Update device metadata |
| `DELETE` | `/dev/:device_id` | Delete a device |

**Create a device:**
```json
POST /dev
{
  "name": "Living Room Sensor",
  "type": "sensor",
  "description": "Temperature & humidity"
}
```

**Response** includes a one-time `api_key` (also listed as `device_api_key`) that the physical device uses for data ingestion. Store it securely — it cannot be retrieved after creation.

**Device shadow state** is embedded in the `GET /dev/:id` response as `shadow`, reflecting the last-known reported values.

---

## Telemetry & Data Ingestion

### Device → Server (Device Auth)

Devices authenticate using the `X-API-Key` header or `Authorization: Bearer <device_api_key>`.

```
POST /data
X-API-Key: <device_api_key>

{
  "device_id": "abc123",
  "data": {
    "temperature": 23.5,
    "humidity": 60.0,
    "battery": 85
  }
}
```

Key behaviour:
- Data is written to the **time-series store** (tstorage) for history queries.
- Data also updates the **device shadow** in BuntDB for instant latest-state reads.
- If a **rule engine** trigger matches, actions fire asynchronously.

### User → Query Historical Data

```
GET /data/:device_id?start=<RFC3339>&end=<RFC3339>&limit=100
Authorization: Bearer <jwt>
```

Returns an array of `{ timestamp, data: { field: value } }` objects.

**Latest state only:**
```
GET /dev/:device_id
```

The `shadow` field in the device response contains the latest reported values.

---

## Real-time Updates (SSE)

The mobile app subscribes to a **Server-Sent Events** stream to receive live device updates.

```
GET /dev/:device_id/events
Authorization: Bearer <jwt>  (or X-API-Key for devices)
```

The server pushes `data:` events as JSON whenever new telemetry arrives for that device:

```
event: telemetry
data: {"device_id":"abc123","temperature":24.1,"humidity":59.8}

event: rule_fired
data: {"rule_id":"r-001","rule_name":"Temp Alert","fired_at":"2025-01-01T12:00:00Z"}
```

### Mobile Client Integration

In Flutter, use the `eventsource` or raw `http` package to consume SSE:

```dart
final request = http.Request('GET', Uri.parse('$baseUrl/dev/$deviceId/events'));
request.headers['Authorization'] = 'Bearer $token';
final response = await httpClient.send(request);
response.stream.transform(utf8.decoder).transform(const LineSplitter()).listen((line) {
  if (line.startsWith('data:')) {
    final json = jsonDecode(line.substring(5));
    // update UI
  }
});
```

---

## Rule Engine

The rule engine evaluates incoming data against user-defined rules and executes actions automatically. Rules are managed via the REST API.

Base path: `/dev/rules` — requires JWT.

| Method | Endpoint | Description |
|--------|----------|-------------|
| `POST`    | `/dev/rules` | Create a rule |
| `GET`     | `/dev/rules` | List all rules for the user |
| `GET`     | `/dev/rules/:id` | Get a specific rule |
| `PUT`     | `/dev/rules/:id` | Update a rule |
| `DELETE`  | `/dev/rules/:id` | Delete a rule |
| `POST`    | `/dev/rules/:id/trigger` | Manually trigger a rule |

### Rule Structure

```json
{
  "name": "High Temperature Alert",
  "trigger": {
    "type": "on_data",
    "device_id": "sensor-abc"
  },
  "logic": {
    "type": "conditions",
    "logic_op": "and",
    "conditions": [
      { "field": "temperature", "operator": "gt", "value": 30 }
    ]
  },
  "actions": [
    { "type": "notify", "config": { "title": "Hot!", "priority": "high" } },
    { "type": "log" }
  ]
}
```

### Trigger Types

| Type | Description |
|------|-------------|
| `on_data` | Fires when new telemetry arrives for the specified device |
| `scheduled` | Fires on a cron schedule (e.g. `@every 5m`) |
| `manual` | Only fires when explicitly triggered via API |

### Condition Operators

`gt`, `gte`, `lt`, `lte`, `eq`, `neq`

### Action Types

| Type | Description |
|------|-------------|
| `log` | Write to server log |
| `notify` | Send push notification via ntfy (see below) |
| `webhook` | Emit event to all registered webhook subscribers |
| `mqtt` | Publish a message to an MQTT topic |
| `command` | Send a command to the device |

### Advanced Logic: Lua Scripting

For complex multi-field calculations or stateful logic, set `"type": "lua"` and provide a Lua script:

```json
{
  "logic": {
    "type": "lua",
    "lua_script": "return data.temperature > 30 and data.humidity > 80"
  }
}
```

See [docs/guides/RULES.md](RULES.md) for full rule documentation.

---

## Push Notifications (ntfy)

datum-server integrates **[ntfy](https://ntfy.sh)** for push notifications to mobile devices. ntfy delivers notifications via its open-source pub/sub system without needing FCM/APNs credentials.

### How It Works

1. The mobile app subscribes to an ntfy topic unique to the user.
2. When a rule fires with `ActionNotify`, the server calls `NotifyUser(ownerID, title, message, priority)`.
3. The `notify.Dispatcher` routes the notification through the registered `NtfyClient` channel.
4. ntfy delivers the push notification to all subscribed mobile clients.

### ntfy Configuration

Set the following environment variables on the server:

```env
NTFY_SERVER=https://ntfy.sh          # or self-hosted ntfy URL
NTFY_USER_PREFIX=datum_user_          # topic prefix per user
```

A user's ntfy topic is `{NTFY_USER_PREFIX}{user_id}` (e.g. `datum_user_abc123`).

### Mobile App Setup

In the Flutter app, subscribe to the ntfy topic on login:

```dart
// The topic is derived from the user ID returned by /auth/me
final topic = 'datum_user_${user.id}';
final ntfyUrl = 'https://ntfy.sh/$topic';
```

Use the official [ntfy Flutter SDK](https://pub.dev/packages/ntfy) or subscribe with a plain HTTP SSE stream.

### Notification Priorities

| Priority | ntfy value | Use case |
|----------|-----------|----------|
| `"low"`  | 2 | Informational alerts |
| `"normal"` | 3 (default) | Standard alerts |
| `"high"` | 4 | Warnings |
| `"urgent"` | 5 | Critical/emergency |

### In-App Notifications (SSE)

For in-app notification badges, use the `/dev/:device_id/events` SSE stream which includes `rule_fired` events. These can drive in-app notification UI without requiring ntfy.

---

## Document Store

datum-server includes an embedded **per-user document database** accessible via the `/db` API. This is ideal for storing app settings, user preferences, profiles, or any structured JSON data.

Base path: `/db` — requires JWT.

| Method | Endpoint | Description |
|--------|----------|-------------|
| `POST`   | `/db/:collection` | Insert a document |
| `GET`    | `/db/:collection` | List documents in collection |
| `GET`    | `/db/:collection/:id` | Get a document |
| `PUT`    | `/db/:collection/:id` | Update a document |
| `DELETE` | `/db/:collection/:id` | Delete a document |
| `GET`    | `/db/:collection/search?q=...` | Search within a collection |

Collections are scoped to the authenticated user. User A cannot access User B's collections.

**Example — Store user settings:**
```
POST /db/settings
Authorization: Bearer <jwt>

{ "theme": "dark", "units": "celsius", "notifications_enabled": true }
```

**Example — Store device profiles:**
```
POST /db/device_profiles
{ "device_id": "abc", "location": "Kitchen", "room": "main" }
```

---

## Commands API

The mobile app can send one-way commands to devices. Devices poll for pending commands and acknowledge them.

### Send a Command (Mobile → Device)

```
POST /dev/:device_id/commands
Authorization: Bearer <jwt>

{
  "type": "set_threshold",
  "payload": { "temperature_max": 28 }
}
```

### Device Polls for Commands

```
GET /dev/:device_id/commands/pending
X-API-Key: <device_api_key>
```

Returns an array of pending commands. The device executes them and acknowledges:

```
POST /dev/:device_id/commands/:command_id/ack
X-API-Key: <device_api_key>

{ "status": "ok" }
```

See [docs/COMMAND_FEATURE.md](../COMMAND_FEATURE.md) for the full command lifecycle.

---

## File Storage (Buckets)

Buckets provide per-device file/binary storage. Useful for storing camera images, audio clips, or firmware artifacts.

Base path: `/dev/:device_id/bucket` — hybrid auth (User or Device).

| Method | Endpoint | Description |
|--------|----------|-------------|
| `POST`   | `/dev/:id/bucket/:key` | Upload a file |
| `GET`    | `/dev/:id/bucket/:key` | Download a file |
| `DELETE` | `/dev/:id/bucket/:key` | Delete a file |
| `GET`    | `/dev/:id/bucket` | List bucket contents |

**Example — Store an image:**
```
POST /dev/camera-01/bucket/snapshot.jpg
X-API-Key: <device_api_key>
Content-Type: image/jpeg

<binary data>
```

**Download from mobile:**
```
GET /dev/camera-01/bucket/snapshot.jpg
Authorization: Bearer <jwt>
```

See [docs/guides/BUCKETS.md](BUCKETS.md) for full details.

---

## OTA Firmware Updates

datum-server supports over-the-air firmware delivery. Store firmware images in buckets and serve them to devices:

1. Upload firmware via bucket API: `POST /dev/:id/bucket/firmware.bin`
2. Create a device command: `POST /dev/:id/commands` with `type: "ota_update"`
3. Device acknowledges and pulls firmware from the bucket URL.

See [docs/guides/FIRMWARE_UPDATE.md](FIRMWARE_UPDATE.md) for the complete OTA workflow.

---

## API Key Management

Users can create multiple named API keys. Each key grants device-level access (data ingestion, command polling) scoped to devices they own.

| Method | Endpoint | Description |
|--------|----------|-------------|
| `POST`   | `/auth/keys` | Create a new API key |
| `GET`    | `/auth/keys` | List all active keys |
| `DELETE` | `/auth/keys/:key_id` | Revoke a key |

Keys are shown only at creation time. They can be used in the `X-API-Key` header or as `Authorization: Bearer` for device authentication.

See [docs/guides/KEY_MANAGEMENT.md](KEY_MANAGEMENT.md).

---

## Provisioning

Zero-touch provisioning enables devices to self-register using a provisioning token:

1. Admin creates a provisioning template: `POST /admin/provisioning`
2. Template includes allowed device types, owner, and expiry.
3. Device sends `POST /pub/provision` with the token → receives its `api_key` in response.

The mobile app can manage provisioning templates and monitor provisioning status.

See [docs/guides/PROVISIONING.md](PROVISIONING.md) and [docs/guides/WIFI_PROVISIONING.md](WIFI_PROVISIONING.md).

---

## Web of Things (WoT)

Each device can have a **W3C Web of Things (WoT) Thing Description** attached. This is a standardized JSON-LD document describing the device's properties, actions, and events.

```
PUT /dev/:device_id/thing-description
Authorization: Bearer <jwt>  (or X-API-Key)

{
  "@context": "https://www.w3.org/2019/wot/td/v1",
  "title": "Temperature Sensor",
  "properties": {
    "temperature": { "type": "number", "unit": "celsius" }
  }
}
```

```
GET /dev/:device_id/thing-description
```

The mobile app uses WoT Thing Descriptions to dynamically render device UIs without hardcoded per-device UI code. See [docs/guides/wot_integration.md](wot_integration.md).

---

## Webhooks

Register external HTTP endpoints to receive real-time event notifications:

| Method | Endpoint | Description |
|--------|----------|-------------|
| `POST`   | `/dev/webhooks` | Register a webhook |
| `GET`    | `/dev/webhooks` | List webhooks |
| `DELETE` | `/dev/webhooks/:id` | Remove a webhook |

Webhook events include: `device.created`, `device.online`, `device.offline`, `data.received`, `rule.triggered`, `command.sent`, `provisioning.complete`.

HMAC-SHA256 signatures are supported for payload verification. See [docs/guides/WEBHOOKS.md](WEBHOOKS.md).

---

## Quick Start for Mobile Developers

```
1. Start datum-server (docker compose up)

2. Register a user:
   POST /auth/register {"email": "dev@example.com", "password": "..."}

3. Login and save JWT:
   POST /auth/login {"email": "...", "password": "..."}
   → {"token": "eyJ..."}

4. Create a device:
   POST /dev {"name": "My Sensor", "type": "sensor"}
   → {"id": "dev-123", "api_key": "sk_..."}

5. Push telemetry from device:
   POST /data {"device_id": "dev-123", "data": {"temp": 22.5}}
   X-API-Key: sk_...

6. Query latest state:
   GET /dev/dev-123   → includes "shadow": {"temp": 22.5}

7. Subscribe to live updates:
   GET /dev/dev-123/events (SSE stream)

8. Create a rule for alerts:
   POST /dev/rules {...}

9. Mobile app subscribes to ntfy topic for push notifications
```
