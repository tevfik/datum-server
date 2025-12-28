# Datum Server - Docker Deployment

This directory contains Docker and Docker Compose configurations for deploying Datum IoT Platform.

## Quick Start

### 1. Environment Setup

```bash
cd docker
cp .env.example .env
# Edit .env with your configuration
```

**Critical settings to change:**
```bash
# Generate secure JWT secret
openssl rand -base64 32

# Update in .env
JWT_SECRET=<generated-secret>
DOMAIN=datum.yourcompany.com
ACME_EMAIL=admin@yourcompany.com
```

### 2. Production Deployment (with Traefik)

```bash
docker-compose up -d
```

This starts:
- ✅ **Traefik** - Reverse proxy with automatic SSL (Let's Encrypt)
- ✅ **datum-server** - Main API server with WebSocket support
- ✅ **Automatic HTTPS** - SSL certificates provisioned automatically

**Endpoints:**
- `https://datum.yourcompany.com` - API server (automatic SSL)
- `http://localhost:8080` - Traefik dashboard (insecure, for monitoring)

### 3. Development Deployment

```bash
docker-compose -f docker-compose.yml -f docker-compose.dev.yml up
```

Development features:
- Live code reloading
- Debug logging enabled
- Source code mounted as volumes
- Gin debug mode

## Architecture

```
                                   ┌──────────────────┐
                                   │   Internet       │
                                   └────────┬─────────┘
                                            │
                                   ┌────────▼─────────┐
                                   │   Traefik        │
                         ┌─────────┤   (Port 80/443)  │
                         │         │   + Let's Encrypt│
                         │         └──────────────────┘
                         │
                         │
            ┌────────────▼────────────┐
            │   datum-server          │
            │   (Port 8000)           │
            │   + datumctl CLI        │
            │   + WebSocket support   │
            └─────────────────────────┘
                         │
                         │
            ┌────────────▼────────────┐
            │   Persistent Storage    │
            │   - datum-data volume   │
            │   - letsencrypt volume  │
            └─────────────────────────┘
```

## Configuration

### Environment Variables

See [.env.example](.env.example) for all available options.

**Required for Production:**
```bash
JWT_SECRET=<secure-random-string>     # Token signing key
DOMAIN=datum.example.com              # Your domain
ACME_EMAIL=admin@example.com          # SSL certificate email
```

**Optional:**
```bash
PORT=8000                             # Server port (default: 8000)
LOG_LEVEL=INFO                        # DEBUG|INFO|WARN|ERROR
CORS_ORIGINS=*                        # Allowed CORS origins
RETENTION_MAX_DAYS=7                  # Data retention period
RATE_LIMIT_REQUESTS=100               # Rate limit per window
```

### Traefik Configuration

Traefik is pre-configured with:
- ✅ **Automatic SSL** - Let's Encrypt certificates
- ✅ **HTTP → HTTPS redirect** - Automatic
- ✅ **WebSocket support** - For camera streaming
- ✅ **Dashboard** - http://localhost:8080

**Custom Traefik labels** (in docker-compose.yml):
```yaml
labels:
  - "traefik.http.routers.datum-server-secure.rule=Host(`${DOMAIN}`)"
  - "traefik.http.routers.datum-server-secure.tls.certresolver=letsencrypt"
  - "traefik.http.middlewares.datum-ws.headers.customrequestheaders.Connection=Upgrade"
  - "traefik.http.middlewares.datum-ws.headers.customrequestheaders.Upgrade=websocket"
```

### WebSocket Configuration

WebSocket endpoints work automatically through Traefik thanks to upgrade headers:

```bash
# ESP32 camera streaming
wss://datum.example.com/devices/esp32-cam-01/stream/ws

# SSE commands
https://datum.example.com/device/device-id/sse-commands
```

## Volumes

**Data storage is managed via host directories** (not Docker named volumes) for easier backup and management:

```yaml
DATA_DIR=../data              # Application data (BuntDB, tstorage)
LETSENCRYPT_DIR=./letsencrypt # SSL certificates
```

**Advantages:**
- ✅ Direct access to data files
- ✅ Simple backup with standard tools (rsync, tar)
- ✅ Easy to locate and manage
- ✅ Works with existing backup solutions

**Default locations:**
- Development: `datum-server/data/` (relative to project root)
- Production: Configure absolute paths in `.env`:
  ```bash
  DATA_DIR=/var/lib/datum/data
  LETSENCRYPT_DIR=/var/lib/datum/letsencrypt
  ```

**Backup data:**
```bash
# Simple tar backup
tar czf datum-backup-$(date +%Y%m%d).tar.gz data/

# Or use rsync
rsync -av data/ /backup/datum/
```

**Restore data:**
```bash
# From tar archive
tar xzf datum-backup-20251228.tar.gz

# Or use rsync
rsync -av /backup/datum/ data/
```

**Migrate from Docker volumes** (if you previously used named volumes):
```bash
# Find old volume
docker volume ls | grep datum

# Copy data from old volume to host directory
docker run --rm -v datum-server_datum-data:/source -v $(pwd)/data:/dest \
  alpine sh -c "cp -av /source/* /dest/"

# Update docker-compose.yml to use host directory (already configured)
docker-compose up -d
```

## Management

### Start services
```bash
docker-compose up -d
```

### Stop services
```bash
docker-compose down
```

### View logs
```bash
# All services
docker-compose logs -f

# Specific service
docker-compose logs -f datum-server
docker-compose logs -f traefik
```

### Restart service
```bash
docker-compose restart datum-server
```

### Execute datumctl commands
```bash
# Inside container
docker-compose exec datum-server datumctl --help

# Examples
docker-compose exec datum-server datumctl user create --username admin
docker-compose exec datum-server datumctl device list
docker-compose exec datum-server datumctl stats show
```

### Access server shell
```bash
docker-compose exec datum-server sh
```

## Monitoring

### Health Check
```bash
curl https://datum.example.com/health
```

Expected response:
```json
{
  "status": "healthy",
  "timestamp": "2025-12-28T10:30:45Z"
}
```

### Traefik Dashboard
Open http://localhost:8080 to view:
- Active routers and services
- SSL certificate status
- Request metrics
- Health checks

### Container Status
```bash
docker-compose ps
```

## Troubleshooting

### SSL Certificate Issues

**Problem:** SSL certificate not provisioned
```bash
# Check Traefik logs
docker-compose logs traefik | grep acme

# Verify DNS points to server
dig datum.example.com

# Ensure ports 80 and 443 are accessible
sudo netstat -tlnp | grep -E ':(80|443)'
```

**Solution:**
1. Verify `DOMAIN` matches DNS A record
2. Ensure ports 80/443 are open in firewall
3. Check `ACME_EMAIL` is valid
4. Wait 1-2 minutes for certificate provisioning

### WebSocket Connection Failed

**Problem:** WebSocket upgrade fails
```bash
# Test WebSocket endpoint
curl -i -N \
  -H "Connection: Upgrade" \
  -H "Upgrade: websocket" \
  https://datum.example.com/devices/device-id/stream/ws
```

**Expected response:** `101 Switching Protocols`

**Solution:**
1. Verify `datum-ws` middleware is applied (check docker-compose labels)
2. Check Traefik logs for upgrade errors
3. Ensure client sends proper WebSocket headers

### Container Won't Start

**Problem:** Container exits immediately
```bash
# Check logs
docker-compose logs datum-server

# Common issues:
# - Missing JWT_SECRET (server won't start)
# - Port already in use
# - Invalid configuration
```

**Solution:**
```bash
# Generate JWT secret if missing
echo "JWT_SECRET=$(openssl rand -base64 32)" >> .env

# Check port conflicts
sudo lsof -i :8000
```

### High Memory Usage

**Problem:** Container using too much memory

```bash
# Check resource usage
docker stats datum-server

# Set memory limits
docker-compose.yml:
services:
  datum-server:
    deploy:
      resources:
        limits:
          memory: 512M
```

### Data Not Persisting

**Problem:** Data lost after restart

```bash
# Verify volumes are mounted
docker volume ls | grep datum

# Inspect volume
docker volume inspect datum-server_datum-data

# Check container mounts
docker inspect datum-server | grep -A 10 Mounts
```

## Security Checklist

Before production deployment:

- [ ] Generate strong `JWT_SECRET` (32+ characters)
- [ ] Set restrictive `CORS_ORIGINS` (not `*`)
- [ ] Enable HTTPS with valid SSL certificate
- [ ] Change Traefik dashboard to secure mode or disable
- [ ] Configure rate limiting appropriately
- [ ] Enable JSON logging (`LOG_FORMAT=json`)
- [ ] Set `LOG_LEVEL=WARN` or `ERROR`
- [ ] Regular backups of `datum-data` volume
- [ ] Keep Docker images updated
- [ ] Monitor Traefik access logs

## Performance Tuning

### For High-Traffic Deployments

```yaml
# docker-compose.yml
services:
  datum-server:
    environment:
      - RATE_LIMIT_REQUESTS=10000
      - RATE_LIMIT_WINDOW_SECONDS=60
    deploy:
      resources:
        limits:
          cpus: '2'
          memory: 2G
        reservations:
          cpus: '1'
          memory: 512M
```

### For Camera Streaming

```yaml
# Increase client buffer for WebSocket streams
services:
  traefik:
    command:
      - "--providers.docker.defaultRule=Host(`{{ normalize .Name }}`)"
      - "--entrypoints.websecure.transport.respondingTimeouts.writeTimeout=300s"
```

## Multi-Server Deployment

For production redundancy:

```yaml
# Add replicas
services:
  datum-server:
    deploy:
      replicas: 3
      update_config:
        parallelism: 1
        delay: 10s
```

**Note:** Requires external load balancer or Docker Swarm mode.

## CI/CD Integration

Automated deployment with Gitea Actions:

```yaml
# .gitea/workflows/deploy.yml
- name: Deploy to production
  run: |
    cd docker
    docker-compose pull
    docker-compose up -d
```

See [../.gitea/workflows/test.yml](../.gitea/workflows/test.yml) for test pipeline.

## Additional Resources

- [Main README](../README.md)
- [API Documentation](../docs/API.md)
- [ESP32 Camera Streaming](../examples/esp32-s3-camera/README.md)
- [Traefik Documentation](https://doc.traefik.io/traefik/)

## Support

For issues or questions:
1. Check logs: `docker-compose logs`
2. Review [Troubleshooting](#troubleshooting)
3. Consult documentation in `docs/`
4. Open issue on Gitea
