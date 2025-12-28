# Datum IoT Server

High-performance IoT data collection and management platform built with Go.

## 🚀 Features

- **Time-Series Storage**: Optimized for IoT sensor data with 347K inserts/sec
- **Real-time Streaming**: Server-Sent Events (SSE) for live data
- **Device Management**: Multi-user device provisioning and API key auth
- **CLI Tool**: `datumctl` - powerful command-line interface
- **REST API**: Complete HTTP API for data ingestion and querying
- **Data Retention**: Automatic cleanup of old time-series partitions
- **Performance**: 633 req/s with 10K concurrent users, 5.6% CPU usage

## 📊 Performance

- **HTTP Layer**: 633 requests/sec (10K concurrent users)
- **TSStorage**: 347,182 inserts/sec (direct storage)
- **BuntDB Metadata**: 2.1M API key lookups/sec
- **Memory**: 430 bytes per device metadata
- **CPU**: 5.6% at 10K concurrent connections

See [docs/FINAL_PERFORMANCE_REPORT.md](docs/FINAL_PERFORMANCE_REPORT.md) for detailed benchmarks.

## 🏗️ Architecture

### Dual Storage Design

1. **BuntDB** (Metadata)
   - Users, devices, API keys
   - System configuration
   - Fast key-value lookups

2. **TSStorage** (Time-Series)
   - Sensor data points
   - 1-hour partitions
   - High-throughput writes

### Components

- `cmd/server/` - HTTP server and handlers
- `internal/auth/` - Authentication & rate limiting
- `internal/storage/` - Dual storage layer
- `docs/` - Comprehensive documentation
- `examples/` - Arduino & MicroPython clients

## 🛠️ Quick Start

### Prerequisites

- Go 1.21+
- Docker & Docker Compose (optional)

### Local Development

```bash
# Clone repository
git clone <repository-url>
cd datum-server

# Install dependencies
go mod download

# Run server
make run

# Or with hot reload
make dev
```

### Docker Deployment

```bash
# Production
docker-compose up -d

# Development (with hot reload)
docker-compose -f docker-compose.dev.yml up
```

Server runs on `http://localhost:8080`

## �️ CLI Tool (datumctl)

Powerful command-line interface for managing devices and querying data:

```bash
# Build CLI
make build-cli

# Login
./datumctl login --email admin@example.com

# List devices
./datumctl device list

# Query data
./datumctl data get --device my-sensor --last 1h

# Create device
./datumctl device create --name "Temperature Sensor"
```

See [CLI Tutorial](docs/tutorials/CLI.md) for complete CLI documentation.

## �📡 API Quick Reference

### Device Authentication

```bash
# Data ingestion
curl -X POST http://localhost:8080/api/data \
  -H "Authorization: Bearer <device-api-key>" \
  -H "Content-Type: application/json" \
  -d '{"temperature": 23.5, "humidity": 65}'
```

### User Authentication

```bash
# Login
curl -X POST http://localhost:8080/api/login \
  -H "Content-Type: application/json" \
  -d '{"email": "user@example.com", "password": "secret"}'

# Create device
curl -X POST http://localhost:8080/api/devices \
  -H "Authorization: Bearer <jwt-token>" \
  -H "Content-Type: application/json" \
  -d '{"name": "Sensor-01", "type": "temperature"}'
```

### Real-time Streaming

```bash
# Subscribe to device data stream
curl -N http://localhost:8080/api/stream/<device-id> \
  -H "Authorization: Bearer <jwt-token>"
```

See [API Reference](docs/reference/API.md) for complete API documentation.

## 🧪 Testing

### Run All Tests

```bash
make test
```

### Coverage Report

```bash
# Generate coverage report
make test-coverage

# View HTML report
go tool cover -html=coverage.out
```

### Current Test Coverage

- **Overall**: ~70% coverage across all packages
- **cmd/server**: 68.6% (400+ tests)
- **internal/storage**: 94.0%
- **internal/auth**: 89.1%
- **internal/logger**: 100.0%

### Test Organization

Tests follow Go naming conventions:
- `handlers_data_test.go` - Data history handlers
- `handlers_system_test.go` - System config handlers
- `handlers_admin_extended_test.go` - Admin operations
- `handlers_integration_test.go` - Integration scenarios

See [Testing Guide](docs/guides/TESTING.md) for comprehensive testing documentation.

## 📚 Documentation

### Getting Started
| Document | Description |
|----------|-------------|
| [Quick Start](docs/tutorials/QUICK_START.md) | **Get up and running in 5 minutes** |
| [Contributing](docs/CONTRIBUTING.md) | Contributing guidelines and standards |
| [Changelog](docs/CHANGELOG.md) | Version history and release notes |

### Guides
| Document | Description |
|----------|-------------|
| [Testing Guide](docs/guides/TESTING.md) | **Comprehensive testing documentation** |
| [Deployment Guide](docs/guides/DEPLOYMENT.md) | **Production deployment instructions** |
| [Security Guide](docs/guides/SECURITY.md) | Security features & best practices |
| [Provisioning Guide](docs/guides/PROVISIONING.md) | Device provisioning guide |
| [Retention Guide](docs/guides/RETENTION.md) | Data retention policies |
| [Password Reset](docs/guides/PASSWORD_RESET.md) | Password recovery procedures |
| [Registration Guide](docs/guides/REGISTRATION.md) | User registration setup |

### Tutorials
| Document | Description |
|----------|-------------|
| [Quick Start](docs/tutorials/QUICK_START.md) | **Get your first device running in 10 minutes** |
| [Use Cases](docs/tutorials/USE_CASES.md) | **Real-world application examples** |
| [CLI Tutorial](docs/tutorials/CLI.md) | Command-line tool (datumctl) |
| [Firmware Development](docs/tutorials/FIRMWARE.md) | Arduino/ESP32 examples |
| [SSE Commands](docs/tutorials/SSE_COMMANDS.md) | Server-Sent Events guide |

### Reference
| Document | Description |
|----------|-------------|
| [API Reference](docs/reference/API.md) | Complete REST API reference |
| [Storage Reference](docs/reference/STORAGE.md) | Storage architecture & design |
| [TSStorage Details](docs/reference/TSTORAGE.md) | Time-series storage internals |
| [Rate Limiting](docs/reference/RATE_LIMITING.md) | Rate limiter configuration |

### Performance
| Document | Description |
|----------|-------------|
| [Performance Report](docs/performance/FINAL_PERFORMANCE_REPORT.md) | Load testing results |
| [TSStorage Benchmarks](docs/performance/TSTORAGE_TEST_RESULTS.md) | Time-series storage benchmarks |
| [BuntDB Benchmarks](docs/performance/BUNTDB_TEST_RESULTS.md) | Metadata storage benchmarks |

### Visual Documentation
| Document | Description |
|----------|-------------|
| [Architecture Diagrams](docs/diagrams/ARCHITECTURE.md) | System architecture & data flow diagrams |

## 🔧 Configuration

### Environment Variables

```bash
# Server
PORT=8080
GIN_MODE=release

# Storage
DATA_PATH=./data/tsdata
META_PATH=./data/metadata.db

# Retention
RETENTION_MAX_DAYS=7
RETENTION_CHECK_HOURS=1

# Rate Limiting
RATE_LIMIT_REQUESTS=1000
RATE_LIMIT_WINDOW=60

# Security
JWT_SECRET=<your-secret-key>
ADMIN_PASSWORD=<admin-password>
```


### Command-Line Flags

The server supports command-line flags that take precedence over environment variables:

```bash
# Run with custom port
./datum-server --port 9000

# Custom data directory
./datum-server --data-dir /var/lib/datum

# Configure data retention
./datum-server --retention-days 30 --retention-check-hours 12

# Combine multiple flags
./datum-server --port 9000 --data-dir /var/lib/datum --retention-days 30

# Show version
./datum-server --version

# Show help
./datum-server --help
```

**Available Flags:**
- `--port` - Server port (default: 8000 or PORT env var)
- `--data-dir` - Data directory path (default: ./data or DATA_DIR env var)
- `--retention-days` - Data retention in days (default: 7 or RETENTION_MAX_DAYS env var)
- `--retention-check-hours` - Retention check interval in hours (default: 24 or RETENTION_CHECK_HOURS env var)
- `--version` - Show version information
- `--help` - Show all available options

**Priority Order:** Command-line flags > Environment variables > Default values

## 🔌 Device Integration

### Arduino/ESP32

```cpp
#include <WiFi.h>
#include <HTTPClient.h>

const char* apiKey = "your-device-api-key";
const char* serverUrl = "http://server:8080/api/data";

void sendData(float temp, float humidity) {
  HTTPClient http;
  http.begin(serverUrl);
  http.addHeader("Authorization", String("Bearer ") + apiKey);
  http.addHeader("Content-Type", "application/json");
  
  String payload = "{\"temperature\":" + String(temp) + 
                   ",\"humidity\":" + String(humidity) + "}";
  
  int httpCode = http.POST(payload);
  http.end();
}
```

See [examples/arduino/](examples/arduino/) for complete examples.

### MicroPython

```python
import urequests
import ujson

API_KEY = "your-device-api-key"
SERVER_URL = "http://server:8080/api/data"

def send_data(temperature, humidity):
    headers = {
        "Authorization": f"Bearer {API_KEY}",
        "Content-Type": "application/json"
    }
    
    data = {
        "temperature": temperature,
        "humidity": humidity
    }
    
    response = urequests.post(SERVER_URL, 
                              headers=headers, 
                              data=ujson.dumps(data))
    response.close()
```

See [examples/micropython/](examples/micropython/) for complete examples.

## 🛡️ Security Features

- **JWT Authentication**: Secure user sessions
- **API Key Auth**: Per-device authentication
- **Rate Limiting**: Token bucket algorithm (100K req/min configurable)
- **User Roles**: Admin and regular user roles
- **Device Ownership**: User-based access control
- **HTTPS Support**: TLS/SSL ready

## 🔄 Data Retention

Automatic cleanup of old time-series data:

```bash
# Configure retention period (days)
export RETENTION_MAX_DAYS=30

# Configure cleanup frequency (hours)
export RETENTION_CHECK_HOURS=6
```

Retention worker runs in background and removes partitions older than configured age.

## 📈 Monitoring

### Health Check

```bash
curl http://localhost:8080/health
```

### Metrics Endpoint

```bash
curl http://localhost:8080/metrics
```

Returns:
- Total devices
- Active devices
- Total data points
- Storage size
- Uptime

## 🔨 Development

### Build

```bash
make build
```

### Run with Hot Reload

```bash
make dev
```

### Format Code

```bash
make fmt
```

### Lint

```bash
make lint
```

### Clean Build Artifacts

```bash
make clean
```

## 🚢 Deployment

### Docker Production

```bash
# Build and run
docker-compose up -d

# View logs
docker-compose logs -f datumpy-server

# Stop
docker-compose down
```

### Systemd Service

```bash
# Copy service file
sudo cp scripts/datum-server.service /etc/systemd/system/

# Enable and start
sudo systemctl enable datum-server
sudo systemctl start datum-server

# Check status
sudo systemctl status datum-server
```

### Backup & Restore

```bash
# Backup data
./scripts/backup.sh

# Restore data
./scripts/restore.sh <backup-file>
```

## 🤝 Contributing

We welcome contributions! Please see:
- [Contributing](docs/CONTRIBUTING.md) - Contributing guidelines, code standards, PR process
- [Testing Guide](docs/guides/TESTING.md) - How to write and run tests
- [Deployment Guide](docs/guides/DEPLOYMENT.md) - Deployment best practices

### Quick Contribution Steps

1. Fork the repository
2. Create feature branch (`git checkout -b feature/amazing`)
3. Follow [coding standards](docs/CONTRIBUTING.md#coding-standards)
4. Write tests (minimum 60% coverage for new code)
5. Commit changes (`git commit -m 'feat: add amazing feature'`)
6. Push to branch (`git push origin feature/amazing`)
7. Open Pull Request

See [Contributing](docs/CONTRIBUTING.md) for detailed guidelines.

## 📄 License

MIT License - see [LICENSE](LICENSE) file for details.

## 🐛 Known Issues

1. `GetUserCount()` slice bounds bug in export function (non-critical, test only)
2. TSStorage partition creation timing in some test scenarios

See GitHub Issues for tracking.

## 🗺️ Roadmap

- [ ] gRPC API support
- [ ] Multi-region replication
- [ ] Prometheus metrics exporter
- [ ] GraphQL API
- [ ] WebSocket support
- [ ] Data aggregation functions
- [ ] Alert system

## 💬 Support

- **Documentation**: [docs/](docs/)
- **Issues**: GitHub Issues
- **Discussions**: GitHub Discussions

## 🙏 Acknowledgments

- [Gin Web Framework](https://github.com/gin-gonic/gin)
- [BuntDB](https://github.com/tidwall/buntdb)
- [TSStorage](https://github.com/nakabonne/tstorage)
- [testify](https://github.com/stretchr/testify)

---

**Version**: 1.0.0  
**Status**: Production Ready  
**Performance**: Tested up to 10K concurrent users  
**Last Updated**: December 2025
