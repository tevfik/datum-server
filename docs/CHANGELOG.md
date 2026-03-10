# Changelog

All notable changes to Datum IoT Platform will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.5.1] - 2026-01-20

### Fixed
- **Mobile**: Regenerated Riverpod providers (`.g.dart`) to fix type mismatch in `Auth` provider causing build failures.

## [1.5.0] - 2026-01-20

### Added
- **Application Keys**: Implemented `ak_` prefixed persistent API keys for server-to-server or CLI auth.
- **Datumctl**: Added `datumctl login --api-key` support with validation.
- **Roadmap**: Added future steps documentation for Mobile, Firmware, and CI/CD.
- **Mobile**: Fixed Flutter widget tests and build issues.
- Comprehensive test suite with 400+ tests achieving 68.6% coverage
- Test file reorganization following Go naming conventions
- Documentation improvements (CONTRIBUTING.md, TESTING.md, DEPLOYMENT.md)

### Changed
- **Refactor**: Centralized authentication middleware in `internal/auth` and removed `cmd/server/middleware.go`.
- Renamed test files to follow Go standards:
  - `critical_handlers_boost_test.go` → `handlers_data_test.go`
  - `medium_coverage_boost_test.go` → `handlers_system_test.go`
  - `quick_wins_test.go` → `handlers_admin_extended_test.go`
  - `comprehensive_test.go` → `handlers_integration_test.go`
  - `additional_coverage_test.go` → `handlers_misc_test.go`
  - `admin_critical_test.go` → `admin_extended_test.go`
  - `final_coverage_test.go` → `handlers_list_test.go`
  - `edge_cases_test.go` → `handlers_edge_test.go`

## [1.0.0] - 2025-12-27

### Added
- Initial production release
- High-performance time-series storage (347K inserts/sec)
- Dual storage architecture (BuntDB + TSStorage)
- JWT-based user authentication
- API key-based device authentication
- Rate limiting with token bucket algorithm
- Server-Sent Events (SSE) for real-time data streaming
- RESTful API for data ingestion and querying
- Command-line tool (datumctl) for device management
- Docker and Docker Compose support
- Systemd service configuration
- Automatic data retention and cleanup
- Device provisioning system
- Multi-user device management
- Health and readiness endpoints
- Metrics endpoint for monitoring
- Comprehensive documentation
- Arduino and MicroPython examples
- Load testing with Locust
- Performance benchmarks

### Performance
- HTTP Layer: 633 req/s with 10K concurrent users
- TSStorage: 347,182 inserts/sec (direct storage)
- BuntDB: 2.1M API key lookups/sec
- Memory: 430 bytes per device metadata
- CPU: 5.6% at 10K concurrent connections

### Security
- JWT authentication with secure token signing
- Per-device API key authentication
- Rate limiting (100K requests/minute configurable)
- User roles (admin/user) with access control
- Device ownership enforcement
- Password hashing with bcrypt
- CORS support
- HTTPS/TLS ready

### Storage
- Time-series storage with 1-hour partitions
- Metadata storage with BuntDB
- Configurable data retention (default: 7 days)
- Automatic partition cleanup
- Support for arbitrary JSON data structures
- Efficient querying with time range filters
- Data aggregation support

### API Endpoints
- `POST /sys/setup` - Initialize system
- `POST /auth/register` - User registration
- `POST /auth/login` - User login
- `POST /dev` - Create device
- `GET /dev` - List user devices
- `POST /dev/:id/data` - Ingest device data
- `GET /dev/:id/data/history` - Query historical data
- `GET /dev/:id/data` - Get latest data point
- `GET /dev/:id/stream/ws` - SSE/WS data streaming
- `GET /sys/status` - Health check
- `GET /ready` - Readiness check
- `GET /metrics` - System metrics

### Admin Endpoints
- `GET /admin/users` - List all users
- `POST /admin/users` - Create user
- `PUT /admin/users/:id` - Update user
- `DELETE /admin/users/:id` - Delete user
- `POST /admin/users/:username/reset-password` - Reset password
- `GET /admin/dev` - List all devices (across all users)
- `PUT /admin/dev/:id` - Update device
- `GET /admin/database/stats` - Database statistics
- `GET /admin/database/export` - Export database
- `POST /admin/database/reset` - Reset database
- `PUT /admin/config/retention` - Update retention policy
- `GET /admin/config` - Get system configuration

### CLI Tool (datumctl)
- `datumctl login` - Authenticate user
- `datumctl device list` - List devices
- `datumctl device create` - Create device
- `datumctl device delete` - Delete device
- `datumctl data get` - Query data
- `datumctl data push` - Push data
- `datumctl user list` - List users (admin)
- `datumctl user create` - Create user (admin)
- `datumctl system stats` - System statistics (admin)

### Documentation
- README.md - Project overview and quick start
- API.md - Complete REST API documentation
- CLI.md - Command-line tool guide
- STORAGE.md - Storage architecture details
- SECURITY.md - Security features and best practices
- RETENTION.md - Data retention configuration
- PROVISIONING.md - Device provisioning guide
- FIRMWARE.md - Arduino/ESP32 firmware examples
- SSE_COMMANDS.md - Server-Sent Events documentation
- FINAL_PERFORMANCE_REPORT.md - Performance benchmarks


### Examples
- Arduino/ESP32 device firmware
- MicroPython device firmware
- Python device simulator
- Load testing scripts

### Testing
- 400+ unit and integration tests
- 68.6% code coverage (cmd/server)
- 94.0% storage layer coverage
- 89.1% authentication coverage
- 100% logger coverage
- Comprehensive handler tests
- Rate limiter tests
- SSE streaming tests
- Load tests with Locust
- Performance benchmarks

### Deployment
- Docker images and Docker Compose configuration
- Development environment with hot reload
- Production-ready Docker setup
- Systemd service files
- Kubernetes manifests (examples)
- Backup and restore scripts
- Log rotation configuration

### Configuration
- Environment variable configuration
- Command-line flag support
- .env.example with all options
- Configurable ports and paths
- Adjustable retention policies
- Rate limiting configuration
- Logging level control
- CORS configuration

### Known Issues
- `GetUserCount()` slice bounds in export function (non-critical, test only)
- TSStorage partition timing in some test scenarios

## [0.1.0] - 2025-11-15

### Added
- Initial development version
- Basic HTTP server with Gin framework
- Simple data storage with BuntDB
- Basic authentication
- Device API key support

---

## Version History

### Version Numbering

We use [Semantic Versioning](https://semver.org/):
- MAJOR version for incompatible API changes
- MINOR version for backwards-compatible functionality additions
- PATCH version for backwards-compatible bug fixes

### Release Schedule

- Major releases: As needed for breaking changes
- Minor releases: Monthly feature additions
- Patch releases: As needed for bug fixes

### Upgrade Guide

#### From 0.x to 1.0

**Breaking Changes:**
- API authentication now required for all endpoints
- Data format standardized to RFC3339 timestamps
- Storage structure changed (requires migration)

**Migration Steps:**
1. Backup existing data: `./scripts/backup.sh`
2. Stop old version: `sudo systemctl stop datum-server`
3. Update binary: `sudo cp datum-server /usr/local/bin/`
4. Update configuration: Review `.env` for new options
5. Start new version: `sudo systemctl start datum-server`
6. Verify: `curl http://localhost:8000/sys/status`

### Deprecations

None at this time.

### Future Roadmap

#### v1.1.0 (Planned - Q1 2026)
- [ ] gRPC API support
- [ ] WebSocket support for bidirectional communication
- [ ] Enhanced data aggregation functions
- [ ] Multi-tenancy support
- [ ] GraphQL API

#### v1.2.0 (Planned - Q2 2026)
- [ ] Alert system with configurable thresholds
- [ ] Email notifications
- [ ] Webhook support
- [ ] Data export to various formats (CSV, JSON, Parquet)
- [ ] Advanced analytics and reporting

#### v2.0.0 (Planned - Q3 2026)
- [ ] Multi-region replication
- [ ] Distributed deployment support
- [ ] Time-series data compression
- [ ] Machine learning integration
- [ ] Advanced visualization dashboard

---

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for details on our code of conduct and the process for submitting pull requests.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Support

- **Documentation**: [docs/](docs/)
- **Issues**: [GitHub Issues](https://github.com/your-org/datum-server/issues)
- **Discussions**: [GitHub Discussions](https://github.com/your-org/datum-server/discussions)

---

[Unreleased]: https://github.com/your-org/datum-server/compare/v1.0.0...HEAD
[1.0.0]: https://github.com/your-org/datum-server/releases/tag/v1.0.0
[0.1.0]: https://github.com/your-org/datum-server/releases/tag/v0.1.0
