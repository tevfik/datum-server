# Datum IoT Server

![Build Status](https://img.shields.io/github/actions/workflow/status/tevfik/datum-server/ci.yml?branch=main)
![Go Report Card](https://goreportcard.com/badge/github.com/tevfik/datum-server)
![Test Coverage](https://img.shields.io/badge/coverage-95%25-brightgreen.svg)
![License](https://img.shields.io/github/license/tevfik/datum-server)

High-performance IoT data collection and management platform built with Go.

## 🚀 Features

- **Time-Series Storage**: Optimized for IoT sensor data with 347K inserts/sec
- **Real-time Streaming**: Server-Sent Events (SSE) & WebSockets for live video/data
- **Device Management**: Multi-user provisioning, Token-based Auth (`dk_...`), and OTA Updates
- **Mobile App**: Production-ready Flutter app for provisioning, streaming, and control
- **Admin Control**: Role-Based Access Control (RBAC) and Admin Management CLI
- **CLI Tool**: `datumctl` - powerful interactive command-line interface
- **Performance**: 633 req/s with 10K concurrent users, 5.6% CPU usage

## 📊 Performance

- **HTTP Layer**: 633 requests/sec (10K concurrent users)
- **TSStorage**: 347,182 inserts/sec (direct storage)
- **BuntDB Metadata**: 2.1M API key lookups/sec
- **Memory**: 430 bytes per device metadata
- **CPU**: 5.6% at 10K concurrent connections

See [docs/performance/FINAL_PERFORMANCE_REPORT.md](docs/performance/FINAL_PERFORMANCE_REPORT.md) for detailed benchmarks.

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
- `cmd/datumctl/` - CLI tool source code
- `internal/auth/` - Authentication & rate limiting (JWT + API Keys)
- `internal/storage/` - Dual storage layer (BuntDB + TStorage)
- `examples/datum_camera_app/` - Flutter Mobile App
- `examples/esp32-s3-camera/` - ESP32-S3 Firmware (C++)

### Advanced Components (Undocumented)

- **MQTT Broker**: Integrated MQTT v5 broker running on ports 1883 (TCP) and 1884 (WS).
- **Telemetry Processor**: Asynchronous high-throughput data ingestion pipeline (`internal/processing`).
- **PostgreSQL Support**: Optional backend controllable via `DATABASE_URL`.

## 🛠️ Quick Start

### Prerequisites

- Go 1.24+
- Docker & Docker Compose (optional)

### Local Development

```bash
# Clone repository
git clone https://github.com/tevfik/datum-server.git
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

Server runs on `http://localhost:8000`

## �️ CLI Tool (datumctl)

Powerful command-line interface for managing devices and querying data:

```bash
# Build CLI
make build-cli

# Interactive Mode (Recommended)
./datumctl interactive
# Follow the on-screen wizard to login, manage devices, and view system status.

# One-off Commands
./datumctl login --email admin@example.com
./datumctl device list
./datumctl device create --name "Living Room Cam" --type camera
```

See [CLI Tutorial](docs/tutorials/CLI.md) for complete CLI documentation.

## 📺 Stream Viewer

A web-based viewer is included to easily test and view camera streams:

1. Open `stream_viewer.html` in your browser.
2. Log in with your admin credentials.
3. Enter the Target Device ID.
4. View the authenticated MJPEG stream.

This viewer handles JWT token acquisition and passes it to the stream endpoint securely.

## �📡 API Quick Reference

### Device Authentication

```bash
# Data ingestion
curl -X POST http://localhost:8000/dev/YOUR_DEVICE_ID/data \
  -H "Authorization: Bearer <device-api-key>" \
  -H "Content-Type: application/json" \
  -d '{"temperature": 23.5, "humidity": 65}'
```

### User Authentication

```bash
# Login
curl -X POST http://localhost:8000/auth/login \
  -H "Content-Type: application/json" \
  -d '{"email": "user@example.com", "password": "secret"}'

# Create device
curl -X POST http://localhost:8000/dev \
  -H "Authorization: Bearer <jwt-token>" \
  -H "Content-Type: application/json" \
  -d '{"name": "Sensor-01", "type": "temperature"}'
```

### Real-time Streaming

```bash
# Subscribe to device data stream
curl -N http://localhost:8000/dev/<device-id>/stream/mjpeg \
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
| [Database Setup](docs/guides/DATABASE_SETUP.md) | **PostgreSQL Setup & Migration** |

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

### Visual Documentation
| Document | Description |
|----------|-------------|
| [Architecture Diagrams](docs/diagrams/ARCHITECTURE.md) | System architecture & data flow diagrams |

## 🔧 Configuration

### Environment Variables

```bash
# Server
PORT=8000
GIN_MODE=release

# Storage
DATA_PATH=./data/tsdata
META_PATH=./data/metadata.db

# Retention
RETENTION_MAX_DAYS=7
RETENTION_CHECK_HOURS=1 # (Deprecated)

# Rate Limiting
RATE_LIMIT_REQUESTS=1000
RATE_LIMIT_WINDOW=60

# Security
JWT_SECRET=<your-secret-key>
ADMIN_PASSWORD=<admin-password>
```

### Advanced Configuration

```bash
# Storage Backend
# If set, overrides file-based storage and uses PostgreSQL
DATABASE_URL=postgres://user:pass@localhost:5432/datum?sslmode=disable

# Security
# Comma-separated list of allowed origins for WebSocket streams.
# Defaults to "*" (allow all) if empty.
CORS_ALLOWED_ORIGINS=https://myapp.com,https://admin.myapp.com

# MQTT
# MQTT ports are currently fixed at 1883 (TCP) and 1884 (WS)
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
const char* serverUrl = "http://server:8000/dev/YOUR_DEVICE_ID/data";

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

See [examples/arduino/](examples/arduino/) for simple sensor examples.

### 📱 Mobile App (Flutter)

The **Datum Camera App** (`examples/datum_camera_app`) provides a complete mobile experience:
- **Provisioning**: Zero-touch WiFi configuration via BLE / SoftAP.
- **Streaming**: Low-latency MJPEG and WebSocket viewing.
- **Recording**: Save videos and snapshots to local gallery.
- **Control**: Toggle LED, flip/mirror stream, restart device.
- **OTA Updates**: Trigger firmware updates directly from the app.

To build:
```bash
cd examples/datum_camera_app
flutter run
```

### ESP32-S3 Camera (Features)

The `examples/esp32-s3-camera` directory contains a production-ready firmware supporting:

- **Zero-Touch Provisioning**: Connect to `Datum-Camera-XXXX` hotspot to configure WiFi.
- **Local Dashboard**: Modern embedded web interface for monitoring and control.
- **Dual Streaming**: Local MJPEG stream + Cloud WebSocket stream.
- **Supported Boards**: ESP32-S3-CAM, Freenove S3, AI-Thinker.

See [examples/esp32-s3-camera/README.md](examples/esp32-s3-camera/README.md) for flashing instructions.

### MicroPython

```python
import urequests
import ujson

API_KEY = "your-device-api-key"
SERVER_URL = "http://server:8000/dev/YOUR_DEVICE_ID/data"

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
- **API Key Rotation**: Hybrid SAS token system with 90-day expiry, 7-day grace period
- **Rate Limiting**: Token bucket algorithm (100K req/min configurable)
- **User Roles**: Admin and regular user roles
- **Device Ownership**: User-based access control
- **HTTPS Support**: TLS/SSL ready

### API Key Rotation

Devices use rotating tokens for enhanced security:

```bash
# Rotate device key (admin)
datumctl device rotate-key <device_id> --grace-days 7

# View token status
datumctl device token-info <device_id>

# Emergency revocation
datumctl device revoke-key <device_id> --force
```

See [API Key Security](docs/diagrams/API_KEY_SECURITY.md) for detailed documentation.

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
curl http://localhost:8000/health
```

### Metrics Endpoint

```bash
curl http://localhost:8000/metrics
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
docker-compose logs -f datum-server

# Stop
docker-compose down

### Nginx Configuration Impact
If serving behind Nginx, you must allow the backend to disable buffering for MJPEG streams. The server sends `X-Accel-Buffering: no` automatically, but ensure your Nginx config respects this or includes:
```nginx
proxy_buffering off; # Enforce global disable if header is ignored
```
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

##  Support

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
