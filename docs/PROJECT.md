# Datum Server - Standalone Backend

This is the standalone Go backend for the Datum IoT platform, extracted from the main monorepo for easier deployment and maintenance.

## What's Included

### Core Components
- **Go Backend** (`cmd/server/`, `internal/`)
  - HTTP REST API with Gin framework
  - Real-time SSE streaming, embedded MQTT broker (mochi), WebSocket binary streams
  - Pluggable storage: BuntDB+tstorage (default, embedded mode) or PostgreSQL+TimescaleDB (production)
  - Hybrid auth: JWT + refresh sessions, persistent user API keys (`ak_`), device keys (`sk_`/`dk_`)
  - Rate limiting, audit logging, rules engine, webhook dispatch, multi-channel notifications
  
### Comprehensive Testing
- **40+ Go unit test files** spanning auth, storage, handlers, MQTT, rules, webhook, processing
- **Storage tests**: tstorage, BuntDB, PostgreSQL provider, retention, system config
- **Auth tests**: JWT, API keys, hybrid middleware, WiFi-encryption, rate limiting
- **HTTP tests**: Per-package handler tests under `internal/api/*`
- **Integration scripts** (`tests/`): provisioning, load, SSE, MQTT device simulator
- **Benchmarks**: Storage performance, auth validation

### Performance
- **HTTP**: 633 req/s with 10K concurrent users
- **Storage**: 347,182 inserts/sec (TSStorage)
- **Auth**: 2,104,200 API key validations/sec

### Documentation (12 files)
- API.md - Complete REST API reference
- STORAGE.md - Storage architecture
- SECURITY.md - Security features
- RETENTION.md - Data retention policies
- PROVISIONING.md - Device provisioning
- FIRMWARE.md - Arduino/ESP32 examples
- Performance reports and benchmark results

### Device Examples
- **Arduino/ESP32** firmware examples
- **MicroPython** client examples
- Production-ready code for IoT devices

### Deployment Scripts
- Docker Compose setup
- Backup/restore scripts
- Setup automation

## Quick Start

```bash
# 1. Build the server
make build-server

# 2. Run locally
make run-server

# 3. Or use Docker
docker compose up -d

# 4. Run tests
make test

# 5. Run benchmarks
make bench

# 6. Run load tests
pip3 install -r tests/requirements.txt
make test-load
```

## Key Commands

```bash
# Development
make help              # Show all commands
make build-server      # Build Go binary
make run-server        # Run server locally
make test              # Run all tests
make bench             # Run Go benchmarks
make test-load         # Run HTTP load tests
make fmt               # Format code
make lint              # Lint code

# Docker
make build             # Build Docker images
make run               # Start all services
make stop              # Stop services
make logs              # View logs

# Database
make db-backup         # Backup database
make db-restore        # Restore from backup
```

## Directory Structure

```
datum-server/
├── cmd/
│   ├── server/          # HTTP server entry point, openapi.yaml
│   └── datumctl/        # Admin/operator CLI (kubectl-style)
├── internal/
│   ├── api/             # HTTP handlers grouped by resource (auth, devices, data, ...)
│   ├── auth/            # JWT, API keys, middleware, WiFi-encryption
│   ├── storage/         # Provider interface + BuntDB+tstorage default backend
│   │   └── postgres/    # PostgreSQL + TimescaleDB backend
│   ├── mqtt/            # Embedded MQTT broker (mochi)
│   ├── processing/      # Async telemetry batcher
│   ├── notify/          # Notification dispatcher (in-app, ntfy, etc.)
│   ├── webhook/         # Outbound webhook dispatcher
│   ├── rules/           # User-defined rule engine
│   ├── ratelimit/       # Per-IP token bucket
│   ├── metrics/         # In-memory metrics + Prometheus export
│   ├── audit/           # Audit log emitter
│   ├── config/          # Centralised configuration loader
│   ├── email/           # Resend / SMTP-style email sender
│   └── logger/          # zerolog setup, broadcaster, file reader
├── web/                 # React + Vite admin UI
├── docs/                # Project, guides, references, diagrams
├── tests/               # Integration scripts (shell + python)
├── scripts/             # Backup, restore, migration, load test
├── docker/              # Dockerfile and compose files
└── Makefile             # Build, test, deploy targets
```

## Migration from Monorepo

This project was cleanly extracted from the `datum-py` monorepo:

**What was kept:**
- All Go backend code (cmd/, internal/)
- The full automated test suite
- All backend documentation
- Device examples (arduino, micropython) under `libraries/DatumIoT/`
- Deployment scripts and Docker files
- Load testing infrastructure

**What was removed:**
- Python dashboard (separate project)
- Python analytics API (separate project)
- Frontend-specific documentation

## Development Notes

### Testing Strategy
1. **Unit tests** (`make test`): Fast, comprehensive coverage
2. **Benchmarks** (`make bench`): Performance validation
3. **Load tests** (`make test-load`): Real-world capacity testing

### Storage Architecture
Two first-class backends share the same `storage.Provider` interface:

- **BuntDB + tstorage (default)** — Single binary, no external dependencies.
  BuntDB stores metadata (users, devices, sessions, rules, configuration);
  tstorage stores time-series telemetry. Ideal for edge devices and dev/staging.
- **PostgreSQL + TimescaleDB** — Production backend. Metadata in regular
  tables, telemetry in a TimescaleDB hypertable. Recommended for multi-instance
  deployments. Selected via configuration (see `docs/guides/DATABASE_SETUP.md`).

All higher-level features (rules engine, webhooks, MQTT, retention) work
identically regardless of which backend is active.

### Authentication
- **JWT tokens**: For user authentication
- **API keys**: For device authentication
- **Rate limiting**: Per-IP and per-device protection

## Performance Targets

- **HTTP throughput**: 600+ req/s sustained
- **Storage writes**: 300K+ inserts/sec
- **API key validation**: 2M+ ops/sec
- **Concurrent connections**: 10K+ users

## License

MIT License - See [LICENSE](LICENSE)

## Links

- **Main Project**: https://github.com/tevfik/datum-py
- **Documentation**: See [docs/](docs/) directory
- **Examples**: See [examples/](examples/) directory
