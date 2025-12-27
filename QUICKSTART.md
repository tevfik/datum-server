# Quick Start Guide

## Start the Server

### Option 1: Using Makefile (Recommended)
```bash
# Build and run the server
make run-server

# Or just build
make build-server
./server
```

### Option 2: Direct Go commands
```bash
# Build
go build -o server ./cmd/server

# Run
./server
```

### Option 3: Docker
```bash
# Start all services (server + analytics + dashboard)
make run

# Or using docker compose directly
docker compose up -d
```

## Build the CLI Tool

```bash
# Using Makefile
make build-cli

# Or directly
go build -o datumctl ./cmd/datumctl
```

## Test Connection

```bash
# Check server status
./datumctl status

# Should show:
# ✅ Server Status
#   URL:      http://localhost:8000
#   Status:   healthy
```

## Server Ports

- **Server**: http://localhost:8000
- **API Docs**: http://localhost:8000/docs
- **Dashboard** (Docker): http://localhost:3000
- **Analytics** (Docker): http://localhost:8001

## First Time Setup

1. Start the server:
   ```bash
   make run-server
   ```

2. Check if initialized:
   ```bash
   ./datumctl status
   ```

3. If not initialized, run setup:
   ```bash
   # Interactive setup (automatically logs you in)
   ./datumctl setup
   
   # Or with flags
   ./datumctl setup --email admin@example.com --platform "My IoT Platform"
   
   # Or using curl
   curl -X POST http://localhost:8000/system/setup \
     -H "Content-Type: application/json" \
     -d '{
       "platform_name": "My IoT Platform",
       "admin_email": "admin@example.com",
       "admin_password": "SecurePassword123",
       "allow_register": false
     }'
   ```

4. Login (if using curl setup):
   ```bash
   ./datumctl login --email admin@example.com
   ```
   
   Note: `datumctl setup` automatically logs you in!

## Data Directory

The server stores data in:
- Local run: `./data/` (in project directory)
- Docker: `/app/data/` (mounted from `./data/`)

Set custom location with environment variable:
```bash
DATA_DIR=/custom/path ./server
```

## Stop the Server

```bash
# If running in foreground: Ctrl+C

# If running in background:
pkill -f "./server"

# Docker:
make stop
# or
docker compose down
```

## Common Commands

```bash
# Build everything
make build-all

# Run tests
make test

# Format code
make fmt

# Check health
make health

# View logs (Docker)
make logs

# Clean everything
make clean
```

## Troubleshooting

### Port already in use
```bash
# Find process using port 8000
lsof -i :8000

# Kill it
kill -9 <PID>
```

### Permission denied for /app/data
```bash
# Create directory with proper permissions
sudo mkdir -p /app/data/tsdata
sudo chown $USER:$USER /app/data -R

# Or use local data directory
DATA_DIR=./data ./server
```

### Server not responding
```bash
# Check if running
ps aux | grep server

# Check logs
tail -f data/server.log  # if logging to file

# Restart
make run-server
```
