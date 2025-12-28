# Datum Server Docker Image

The official Docker image for Datum Server, a high-performance IoT data collection and management platform built with Go.

## What is Datum Server?

Datum Server is a lightweight, high-throughput backend for IoT applications. It features:
- **Dual Storage Architecture**: BuntDB for fast metadata lookups and tstorage for high-speed time-series data ingestion.
- **Real-time Streaming**: Server-Sent Events (SSE) for live command and data updates.
- **Device Management**: Secure provisioning, authentication, and management of IoT devices.
- **REST API**: Comprehensive API for data access and system management.

## How to use this image

### Start a simple instance

```bash
docker run -d \
  --name datum-server \
  -p 8000:8000 \
  -e JWT_SECRET=my-super-secret-key \
  datum-server:latest
```

### Using Docker Compose

The easiest way to run Datum Server is with Docker Compose.

```yaml
version: '3.8'
services:
  datum-server:
    image: datum-server:latest
    ports:
      - "8000:8000"
    environment:
      - JWT_SECRET=change-me-in-production
      - LOG_LEVEL=INFO
      - LOG_FORMAT=json
    volumes:
      - datum-data:/root/data

volumes:
  datum-data:
```

## Configuration

The image is configured using environment variables.

### Core Settings

| Variable | Default | Description |
|:--- |:--- |:--- |
| `PORT` | `8000` | The port the server listens on. |
| `JWT_SECRET` | **Required** | Secret key for signing JWT tokens. Must be at least 32 characters. |
| `CORS_ORIGINS` | `*` | Comma-separated list of allowed CORS origins. |

### Logging

| Variable | Default | Description |
|:--- |:--- |:--- |
| `LOG_LEVEL` | `INFO` | Logging verbosity. Options: `DEBUG`, `INFO`, `WARN`, `ERROR`, `FATAL`. |
| `LOG_FORMAT` | `pretty` | Log output format. Options: `pretty` (colored text), `json` (structured). |

### Data Retention

| Variable | Default | Description |
|:--- |:--- |:--- |
| `RETENTION_MAX_DAYS` | `7` | Number of days to keep time-series data. |
| `RETENTION_CHECK_HOURS` | `1` | Interval (in hours) to check for and delete old data. |

### Rate Limiting

| Variable | Default | Description |
|:--- |:--- |:--- |
| `RATE_LIMIT_REQUESTS` | `100` | Maximum requests allowed per window. |
| `RATE_LIMIT_WINDOW_SECONDS` | `60` | Duration of the rate limit window in seconds. |

## Volumes

| Path | Description |
|:--- |:--- |
| `/root/data` | Stores both the metadata database (`meta.db`) and time-series data (`tsdata/`). **Mount this volume to persist data.** |

## Exposed Ports

| Port | Description |
|:--- |:--- |
| `8000` | Main HTTP API port. |

## Health Check

The image includes `wget` for health checks.
- **Endpoint**: `http://localhost:8000/health`
- **Command**: `wget -qO- http://localhost:8000/health`

## Building Locally

To build the image from source:

```bash
# From the project root
docker build -f docker/Dockerfile -t datum-server:local .
```
