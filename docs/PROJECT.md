# Datum Server - Standalone Backend

This is the standalone Go backend for the Datum IoT platform, extracted from the main monorepo for easier deployment and maintenance.

## What's Included

### Core Components
- **Go Backend** (`cmd/server/`, `internal/`)
  - HTTP REST API with Gin framework
  - Real-time SSE streaming
  - Dual storage: BuntDB (metadata) + TSStorage (time-series)
  - JWT authentication + API key auth
  - Rate limiting and admin middleware
  
### Comprehensive Testing
- **55+ unit tests** (3,282 lines of test code)
- **Storage tests**: TSStorage, BuntDB, retention, system config
- **Auth tests**: JWT, API keys, rate limiting
- **HTTP tests**: All handlers and endpoints
- **Benchmarks**: Storage performance, auth validation
- **Load tests**: Locust-based HTTP load testing

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
│   └── server/           # HTTP server, handlers, main.go
├── internal/
│   ├── auth/            # Authentication, middleware, rate limiting
│   └── storage/         # TSStorage + BuntDB implementations
├── docs/                # 12 documentation files
├── examples/
│   ├── arduino/         # ESP32 firmware examples
│   └── micropython/     # MicroPython client examples
├── tests/               # Load testing scripts (Locust)
├── scripts/             # Backup, restore, setup scripts
├── data/                # Database files (generated)
├── docker-compose.yml   # Docker deployment
├── Makefile             # Build and test commands
└── README.md            # Main documentation
```

## Migration from Monorepo

This project was cleanly extracted from the `datum-py` monorepo:

**What was kept:**
- All Go backend code (cmd/, internal/)
- All tests (55+ tests, full coverage)
- All backend documentation (12 files)
- Device examples (arduino, micropython)
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
- **BuntDB**: Fast in-memory metadata storage with persistence
- **TSStorage**: Custom time-series storage optimized for IoT
- **Retention**: Automatic cleanup of old data

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
