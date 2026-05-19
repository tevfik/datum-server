# Datum IoT Platform — Requirements Document

**Version**: 1.8.1  
**Status**: Current  
**Last Updated**: 2026-05-01

---

## 1. Overview

Datum IoT Server is a high-performance, self-hosted IoT data collection and device management platform. It provides secure device onboarding, real-time telemetry ingestion, time-series storage, device command dispatch, and a rule engine — accessible via REST API, MQTT, and SSE/WebSocket.

---

## 2. Functional Requirements

### 2.1 Authentication & Authorization

| ID | Requirement |
|----|-------------|
| AUTH-01 | The system MUST support user registration with email and password (minimum 8 characters). |
| AUTH-02 | The system MUST issue short-lived JWT access tokens (default: 15 minutes, configurable via `JWT_ACCESS_EXPIRY_MINUTES`). |
| AUTH-03 | The system MUST issue long-lived refresh tokens for session renewal (default: 30 days, configurable via `JWT_REFRESH_EXPIRY`). |
| AUTH-04 | The system MUST support persistent user API keys prefixed with `ak_` for non-interactive clients. |
| AUTH-05 | The system MUST support Role-Based Access Control (RBAC) with two roles: `user` and `admin`. |
| AUTH-06 | The system MUST support per-user multi-session management (list, revoke individual sessions). |
| AUTH-07 | The system MUST support password reset via time-limited tokens (default: 15 minutes). Password reset flows MUST NOT leak whether an email is registered (anti-enumeration). |
| AUTH-08 | The system MUST support OAuth 2.0 login via Google and GitHub. |
| AUTH-09 | The system MUST suspend login for accounts with `status = "suspended"`. |
| AUTH-10 | Admin users MUST be able to create, list, update (role/status), and delete any user account. |
| AUTH-11 | The system MUST support push notification token registration per user (FCM, APNS). |

### 2.2 Device Management

| ID | Requirement |
|----|-------------|
| DEV-01 | Each user MUST be able to create, list, update, and delete devices. |
| DEV-02 | Devices MUST be identified by a server-generated UUID (`dev_xxx`) and optionally a hardware UID (e.g., MAC address). |
| DEV-03 | Devices MUST support token-based hybrid authentication using short-lived tokens (`dk_...`) derived from a master secret. |
| DEV-04 | Token rotation MUST support a configurable grace period (default: 7 days) during which the previous token remains valid, ensuring zero-downtime firmware upgrades. |
| DEV-05 | Device keys MUST be revocable by the device owner or an admin, immediately invalidating all tokens. |
| DEV-06 | Each device MUST have a "shadow state" (last reported sensor values) accessible without querying the full time-series history. |
| DEV-07 | The system MUST support device Thing Descriptions (WoT TD) for semantic device metadata. |
| DEV-08 | The system MUST support desired device configuration ("desired state") for remote configuration push. |
| DEV-09 | The system MUST limit the number of devices per user (configurable via `DEVICE_MAX_PER_USER`, default: 100). |
| DEV-10 | Admin users MUST be able to list all devices across all users, force-delete any device, ban/suspend devices. |
| DEV-11 | The system MUST detect duplicate device UIDs at registration time and prevent double registration. |

### 2.3 Device Provisioning

| ID | Requirement |
|----|-------------|
| PROV-01 | The system MUST support zero-touch provisioning via a QR-code / UID flow where the device polls for configuration. |
| PROV-02 | Provisioning requests MUST expire after a configurable timeout. |
| PROV-03 | A user MUST be able to cancel a pending provisioning request. |
| PROV-04 | On provisioning completion, a device record, API key, and (optionally) token MUST be created atomically. |
| PROV-05 | The system MUST provide a status endpoint so the firmware can poll for provisioning completion. |

### 2.4 Telemetry & Data

| ID | Requirement |
|----|-------------|
| DATA-01 | Devices MUST be able to submit JSON telemetry data via `POST /dev/{device_id}/data` using API key or device token authentication. |
| DATA-02 | The system MUST store numeric telemetry values in time-series storage (partitioned by 1-hour windows). |
| DATA-03 | The system MUST enforce data retention and automatically purge time-series data older than the configured limit (default: 7 days). |
| DATA-04 | The system MUST support batch telemetry upload (`POST /dev/{device_id}/data/batch`) with a maximum batch size of 50,000 data points. |
| DATA-05 | Users MUST be able to query historical data with optional start/end time range and a configurable limit (max: `QUERY_MAX_LIMIT`, default: 10,000). |
| DATA-06 | The system MUST expose a "latest data" endpoint returning the most recent shadow state without time-series query overhead. |
| DATA-07 | Public data endpoints MUST only expose device data when: the device status is `active`, and the requesting IP is NOT a private/internal address. |
| DATA-08 | The system MUST update the device's `last_seen` timestamp on every data submission. |
| DATA-09 | The system MUST merge reported telemetry metrics into `shadow_state` and maintain a per-device metrics list for efficient querying. |

### 2.5 Command Dispatch

| ID | Requirement |
|----|-------------|
| CMD-01 | Users MUST be able to enqueue commands for a device (`action` + optional `params`). |
| CMD-02 | Devices MUST be able to poll for pending commands via `GET /dev/{device_id}/commands`. |
| CMD-03 | Commands MUST have a configurable TTL (default: 24 hours); expired commands MUST be excluded from the pending list. |
| CMD-04 | Devices MUST be able to acknowledge commands with an optional result payload. |
| CMD-05 | The system MUST expose a command count endpoint for use by firmware to decide polling frequency. |

### 2.6 Rule Engine

| ID | Requirement |
|----|-------------|
| RULE-01 | Each user MUST be able to create, list, update, and delete rules scoped to their devices. |
| RULE-02 | Rules MUST support conditions on numeric fields using operators: `gt`, `gte`, `lt`, `lte`, `eq`, `neq`, `contains`. |
| RULE-03 | Rule actions MUST include: `webhook` (HTTP POST), `log` (structured log entry), `mqtt` (publish to topic). |
| RULE-04 | Rules MUST be evaluated on every telemetry data submission for the matching device. |
| RULE-05 | Rules MUST be persisted to disk (`data/rules.json`) and auto-loaded on startup. |
| RULE-06 | Optionally, rules MAY use Lua scripts for advanced evaluation logic. |

### 2.7 MQTT Broker

| ID | Requirement |
|----|-------------|
| MQTT-01 | The system MUST include an integrated MQTT v5 broker on port 1883 (TCP) and 1884 (WebSocket). |
| MQTT-02 | MQTT connections MUST be authenticated using device API keys or device tokens as the password. |
| MQTT-03 | The system MUST support optional TLS for MQTT on port 8883 via `MQTT_TLS_CERT` / `MQTT_TLS_KEY`. If TLS cert/key files are configured but cannot be loaded, startup MUST fail immediately (not fall back to plain-text). |
| MQTT-04 | MQTT ACL MUST restrict clients to their own device topics (`device/{device_id}/#`). |
| MQTT-05 | MQTT ACL lookups MUST be cached (default: 60 seconds) to reduce database load. |
| MQTT-06 | Device telemetry published via MQTT MUST be stored in time-series storage equivalent to the REST API path. |

### 2.8 Real-time Streaming

| ID | Requirement |
|----|-------------|
| STREAM-01 | The system MUST support Server-Sent Events (SSE) for real-time device data and command streaming. |
| STREAM-02 | The system MUST support MJPEG/binary video streaming proxied from device firmware. |
| STREAM-03 | Video streams MUST be bounded to 50 clients per stream and 500 total clients globally. |
| STREAM-04 | Stale streams (no clients + no new frames for 5 minutes) MUST be cleaned up automatically (checked every 2 minutes). |

### 2.9 Document Store (Collections)

| ID | Requirement |
|----|-------------|
| DOC-01 | Each user MUST be able to create, read, update, and delete arbitrary JSON documents organized into named collections. |
| DOC-02 | Documents MUST be isolated per user — one user cannot access or modify another user's documents. |
| DOC-03 | The system MUST prevent API callers from overwriting immutable fields (`id`, `_owner_id`) via update operations. |
| DOC-04 | Admin users MUST be able to list all collections across all users and manage documents in any collection. |

### 2.10 System & Admin

| ID | Requirement |
|----|-------------|
| SYS-01 | The system MUST require one-time initialization before accepting user registrations (`POST /admin/setup`). |
| SYS-02 | User self-registration MUST be independently toggleable (`allow_register` config). |
| SYS-03 | Admin MUST be able to export the full database snapshot. |
| SYS-04 | Admin MUST be able to reset the database (destructive, requires confirmation). |
| SYS-05 | The system MUST expose structured system logs accessible via admin API (with level and search filtering). |
| SYS-06 | The system MUST expose `/health`, `/ready`, and `/live` endpoints for container orchestration. |
| SYS-07 | Data retention days MUST be configurable at runtime without restart. |
| SYS-08 | The system MUST expose current server time via `/sys/time` (used by firmware for NTP-free clock sync). |

### 2.11 Analytics & Quotas

| ID | Requirement |
|----|-------------|
| ANA-01 | The system MUST track cumulative ingest metrics (data points, commands) per user. |
| QUOTA-01 | Admin MUST be able to set per-user device quotas, data quotas, and API rate limits. |

---

## 3. Non-Functional Requirements

### 3.1 Performance

| ID | Requirement |
|----|-------------|
| PERF-01 | The HTTP API MUST sustain ≥ 600 requests/second under 10,000 concurrent users. |
| PERF-02 | Time-series ingestion MUST sustain ≥ 300,000 data point inserts/second on reference hardware. |
| PERF-03 | Device API key lookups MUST complete in ≤ 1 ms (BuntDB in-memory). |
| PERF-04 | Memory overhead per device metadata record MUST not exceed 1 KB. |

### 3.2 Security

| ID | Requirement |
|----|-------------|
| SEC-01 | Passwords MUST be stored using bcrypt with default cost. |
| SEC-02 | JWT tokens MUST be signed with HS256 using a secret of at least 32 characters. If `JWT_SECRET` is absent, a secure random secret is generated per startup (invalidates tokens on restart; operators must set `JWT_SECRET` for production). |
| SEC-03 | All internal/private IP addresses MUST be blocked from accessing public data endpoints. Covered ranges: `10.0.0.0/8`, `172.16.0.0/12`, `192.168.0.0/16`, `127.0.0.0/8`, `::1/128`, `fc00::/7`, `169.254.0.0/16`. |
| SEC-04 | The system MUST enforce a global rate limit on all endpoints (default: 100 req/60s per IP), returning `429 Too Many Requests` when exceeded. |
| SEC-05 | Request bodies MUST be limited in size (default: 5 MB, configurable via `MAX_REQUEST_BODY_BYTES`). |
| SEC-06 | The system MUST apply CORS headers configurable via `CORS_ALLOWED_ORIGINS`. |
| SEC-07 | HSTS MUST be enabled when running behind TLS (configurable via `ENABLE_HSTS`). |
| SEC-08 | All API responses MUST include an `X-Request-ID` header for request correlation. |

### 3.3 Reliability

| ID | Requirement |
|----|-------------|
| REL-01 | Graceful shutdown MUST follow a 4-phase sequence: HTTP drain (10s) → MQTT stop → telemetry flush + rule save → storage close. |
| REL-02 | Telemetry data MUST be buffered in memory (default: 10,000 points) to absorb bursts; `points_dropped` counter MUST be exposed when the buffer overflows. |
| REL-03 | All PostgreSQL queries MUST apply a 10-second timeout to prevent query pile-up. |
| REL-04 | Critical startup failures (TLS misconfiguration, storage init errors) MUST cause immediate process exit with a descriptive error. |

### 3.4 Portability

| ID | Requirement |
|----|-------------|
| PORT-01 | The system MUST run on Linux amd64 and arm64 (Raspberry Pi). |
| PORT-02 | The system MUST support Docker deployment with a single `docker-compose.yml`. |
| PORT-03 | The system MUST default to embedded BuntDB + TSStorage (no external dependencies). Optional PostgreSQL backend is available via `DATABASE_URL`. |

---

## 4. Configuration Reference

All settings may be specified via `datum-server.yaml` or environment variables. Environment variables take precedence.

| Environment Variable | Default | Description |
|---------------------|---------|-------------|
| `SERVER_PORT` | `8000` | HTTP server port |
| `SERVER_URL` | auto-detected | Public server URL (used in provisioning responses) |
| `DATA_DIR` | `./data` | Path for persistent storage (BuntDB + TSStorage) |
| `JWT_SECRET` | random (ephemeral) | HMAC-SHA256 signing key (minimum 32 chars) |
| `JWT_REFRESH_EXPIRY` | `30` | Refresh token lifetime in days |
| `JWT_ACCESS_EXPIRY_MINUTES` | `15` | Access token lifetime in minutes |
| `DATABASE_URL` | — | PostgreSQL DSN (if set, enables PG backend) |
| `DEVICE_MAX_PER_USER` | `100` | Maximum devices per user account |
| `MQTT_TLS_CERT` | — | Path to MQTT TLS certificate |
| `MQTT_TLS_KEY` | — | Path to MQTT TLS private key |
| `MQTT_ALLOW_INSECURE` | `false` | Allow non-TLS MQTT (set `true` for development) |
| `RATE_LIMIT_REQUESTS` | `100` | Requests per rate limit window |
| `RATE_LIMIT_WINDOW_SECONDS` | `60` | Rate limit window size |
| `MAX_REQUEST_BODY_BYTES` | `5242880` | Max body size (5 MB) |
| `RETENTION_MAX_DAYS` | `7` | Time-series data retention in days |
| `PUBLIC_DATA_RETENTION_DAYS` | `1` | Public data endpoint retention |
| `TELEMETRY_BUFFER_SIZE` | `10000` | In-memory telemetry ingestion buffer |
| `QUERY_MAX_LIMIT` | `10000` | Maximum rows per data history query |
| `CORS_ALLOWED_ORIGINS` | `*` | Comma-separated allowed CORS origins |
| `ENABLE_HSTS` | `true` | Add Strict-Transport-Security header |
| `LOG_LEVEL` | `INFO` | Log level: DEBUG, INFO, WARN, ERROR |
| `LOG_OUTPUT` | `console` | Log format: console, json, file |
| `RESEND_API_KEY` | — | Resend.com API key for password reset emails |
| `NTFY_URL` | — | ntfy push notification server URL |
| `GOOGLE_CLIENT_ID` / `GOOGLE_CLIENT_SECRET` | — | Google OAuth credentials |
| `GITHUB_CLIENT_ID` / `GITHUB_CLIENT_SECRET` | — | GitHub OAuth credentials |
| `CONTENT_SECURITY_POLICY` | built-in | Override default CSP header |

---

## 5. API Endpoint Inventory

### 5.1 Authentication (`/auth`)

| Method | Path | Auth | Description |
|--------|------|------|-------------|
| POST | `/auth/register` | None | Register new user |
| POST | `/auth/login` | None | Login, get JWT + refresh token |
| POST | `/auth/refresh` | Refresh token | Issue new access token |
| POST | `/auth/logout` | JWT | Revoke current session |
| GET | `/auth/me` | JWT | Get own profile |
| PUT | `/auth/me` | JWT | Update display name |
| PUT | `/auth/password` | JWT | Change password |
| DELETE | `/auth/user` | JWT | Delete own account |
| GET | `/auth/sessions` | JWT | List active sessions |
| DELETE | `/auth/sessions/:jti` | JWT | Revoke a session |
| GET | `/auth/push-tokens` | JWT | List push notification tokens |
| POST | `/auth/push-token` | JWT | Register push notification token |
| DELETE | `/auth/push-token/:id` | JWT | Remove push notification token |
| POST | `/auth/forgot-password` | None | Request password reset email |
| POST | `/auth/reset-password` | None | Complete password reset with token |
| GET | `/auth/oauth/:provider` | None | Initiate OAuth redirect |
| GET | `/auth/oauth/:provider/callback` | None | OAuth callback |

### 5.2 Devices (`/dev`, `/api/v1/dev`)

| Method | Path | Auth | Description |
|--------|------|------|-------------|
| POST | `/dev` | JWT/API Key | Create device |
| GET | `/dev` | JWT/API Key | List own devices |
| GET | `/dev/:id` | JWT/API Key | Get device details |
| PUT | `/dev/:id` | JWT/API Key | Update device (name, type, status) |
| DELETE | `/dev/:id` | JWT/API Key | Delete device |
| POST | `/dev/:id/data` | Device Key | Submit telemetry |
| POST | `/dev/:id/data/batch` | Device Key | Submit telemetry batch |
| GET | `/dev/:id/data` | JWT/API Key | Get data history |
| GET | `/dev/:id/data/latest` | JWT/API Key | Get latest shadow state |
| GET | `/dev/:id/commands` | Device Key | Poll for pending commands |
| POST | `/dev/:id/commands` | JWT/API Key | Enqueue command |
| POST | `/dev/:id/commands/:cmd_id/ack` | Device Key | Acknowledge command |
| GET | `/dev/:id/commands/count` | Device Key | Get pending command count |
| GET | `/dev/:id/td` | JWT/API Key | Get Thing Description |
| PUT | `/dev/:id/td` | JWT/API Key | Update Thing Description |
| GET | `/dev/:id/config` | Device Key | Get desired configuration |
| PUT | `/dev/:id/config` | JWT/API Key | Set desired configuration |
| GET | `/dev/:id/keys` | JWT/API Key | Get token info |
| POST | `/dev/:id/keys/rotate` | JWT/API Key | Rotate device token |
| DELETE | `/dev/:id/keys` | JWT/API Key | Revoke device keys |
| POST | `/dev/:id/keys/init` | Device Key | Initialize token from master secret |

### 5.3 Public Data (`/pub`)

| Method | Path | Auth | Description |
|--------|------|------|-------------|
| GET | `/pub/:device_id/data` | None (IP filtering) | Public telemetry for active devices |

### 5.4 Document Store (`/db`)

| Method | Path | Auth | Description |
|--------|------|------|-------------|
| POST | `/db/:collection` | JWT | Create document |
| GET | `/db/:collection` | JWT | List documents in collection |
| GET | `/db/:collection/:id` | JWT | Get document |
| PUT | `/db/:collection/:id` | JWT | Update document |
| DELETE | `/db/:collection/:id` | JWT | Delete document |
| GET | `/admin/database/stats` | Admin | Database statistics |

### 5.5 Provisioning (`/prov`)

| Method | Path | Auth | Description |
|--------|------|------|-------------|
| POST | `/prov` | JWT | Create provisioning request |
| GET | `/prov/:id` | JWT | Get provisioning request status |
| POST | `/prov/:id/complete` | Device | Device polls and activates |
| POST | `/prov/:id/cancel` | JWT | Cancel pending request |
| GET | `/prov` | JWT | List user provisioning requests |
| GET | `/prov/check/:uid` | Device | Check if UID is already registered |

### 5.6 Rules (`/rules`)

| Method | Path | Auth | Description |
|--------|------|------|-------------|
| GET | `/rules` | JWT | List user rules |
| POST | `/rules` | JWT | Create rule |
| PUT | `/rules/:id` | JWT | Update rule |
| DELETE | `/rules/:id` | JWT | Delete rule |

### 5.7 Keys (`/keys`)

| Method | Path | Auth | Description |
|--------|------|------|-------------|
| GET | `/keys` | JWT | List user API keys |
| POST | `/keys` | JWT | Create API key |
| DELETE | `/keys/:id` | JWT | Delete API key |

### 5.8 System (`/sys`)

| Method | Path | Auth | Description |
|--------|------|------|-------------|
| GET | `/sys/info` | None | Version and build info |
| GET | `/sys/time` | None | Current server UTC time |
| GET | `/sys/ip` | None | Client's resolved public IP |

### 5.9 Admin (`/admin`)

| Method | Path | Auth | Description |
|--------|------|------|-------------|
| POST | `/admin/setup` | None (once) | Initialize system |
| GET | `/admin/users` | Admin | List all users |
| GET | `/admin/users/:id` | Admin | Get user |
| PUT | `/admin/users/:id` | Admin | Update user role/status |
| DELETE | `/admin/users/:id` | Admin | Delete user |
| GET | `/admin/devices` | Admin | List all devices |
| DELETE | `/admin/devices/:id` | Admin | Force delete device |
| GET | `/admin/system/config` | Admin | Get system config |
| PUT | `/admin/system/retention` | Admin | Update data retention |
| PUT | `/admin/system/registration` | Admin | Toggle user registration |
| PUT | `/admin/system/rate-limit` | Admin | Update rate limit config |
| GET | `/admin/system/logs` | Admin | Get system logs |
| DELETE | `/admin/system/logs` | Admin | Clear system logs |
| GET | `/admin/database/stats` | Admin | Database statistics |
| GET | `/admin/database/export` | Admin | Export full database |
| POST | `/admin/database/reset` | Admin | Reset database (destructive) |

### 5.10 Health & Infrastructure

| Method | Path | Auth | Description |
|--------|------|------|-------------|
| GET | `/health` | None | Health check (MQTT + storage) |
| GET | `/ready` | None | Readiness probe |
| GET | `/live` | None | Liveness probe |

---

## 6. Firmware Compatibility Requirements

| ID | Requirement |
|----|-------------|
| FW-01 | All firmware clients MUST support TLS certificate verification or the CA certificate pinning mechanism provided in `datum_ca.h`. |
| FW-02 | Firmware MUST implement token-based authentication using the `dk_...` token format, derived from a master secret using HMAC-SHA256. |
| FW-03 | Firmware SHOULD implement a failsafe AP mode when WiFi/server connectivity fails (static password minimum 8 characters). |
| FW-04 | OTA update endpoints MUST accept `Authorization: Bearer <device_token>` header, not a query parameter, for security. |
| FW-05 | Firmware MUST handle `401 Unauthorized` responses by triggering token rotation via the `/keys/rotate` endpoint. |
| FW-06 | Firmware MUST report hardware UID (MAC address or chip ID) during provisioning for duplicate detection. |

---

## 7. Client Library Requirements

| ID | Requirement |
|----|-------------|
| LIB-01 | The Arduino C++ library MUST support payload publishing and device token authentication. |
| LIB-02 | The Python library MUST support authentication, device management, and data submission (Python 3.9+). |
| LIB-03 | The Dart/Flutter SDK MUST support all user-facing APIs including provisioning and real-time streaming. |
| LIB-04 | All libraries MUST support TLS (client-side CA pinning optional). |
| LIB-05 | Libraries MUST handle `401 Unauthorized` responses and surface them as typed errors so callers can trigger token refresh. |
