# Logging Infrastructure

## Overview

Datum Server uses **zerolog** for structured, high-performance logging. Logs are written to **STDOUT** only - there is no built-in file logging.

## Configuration

### Environment Variables

| Variable | Values | Default | Description |
|----------|--------|---------|-------------|
| `LOG_LEVEL` | `DEBUG`, `INFO`, `WARN`, `ERROR`, `FATAL` | `INFO` | Minimum log level |
| `LOG_FORMAT` | `pretty`, `json` | `pretty` | Output format |

### Examples

```bash
# Development: human-readable colored output
LOG_LEVEL=DEBUG LOG_FORMAT=pretty ./server

# Production: JSON format for log aggregation
LOG_LEVEL=INFO LOG_FORMAT=json ./server

# Docker Compose
environment:
  - LOG_LEVEL=INFO
  - LOG_FORMAT=json
```

## Log Formats

### Pretty Format (Development)
```
2025-12-27T23:53:41+03:00 INF Server started port=8000
2025-12-27T23:53:41+03:00 DBG Database connected path=/app/data/meta.db
```

### JSON Format (Production)
```json
{"level":"info","time":"2025-12-27T23:53:41+03:00","caller":"server/main.go:123","message":"Server started","port":8000}
{"level":"debug","time":"2025-12-27T23:53:41+03:00","caller":"storage/storage.go:45","message":"Database connected","path":"/app/data/meta.db"}
```

## Log Levels

- **DEBUG**: Detailed diagnostic information (SQL queries, request details)
- **INFO**: General informational messages (server start, requests)
- **WARN**: Warning conditions (rate limit exceeded, deprecated features)
- **ERROR**: Error conditions (failed operations, but server continues)
- **FATAL**: Critical errors (server cannot continue, will exit)

## File Logging (Production Setup)

For production deployments, redirect logs to files using standard Unix tools:

### Direct Redirection
```bash
# Append to file
./server >> /var/log/datum/server.log 2>&1

# Rotate logs with logrotate
./server 2>&1 | tee -a /var/log/datum/server.log
```

### Docker Compose with Logging Driver
```yaml
services:
  api:
    image: datum-server
    logging:
      driver: "json-file"
      options:
        max-size: "10m"
        max-file: "3"
```

### Systemd Service
```ini
[Service]
ExecStart=/usr/local/bin/datum-server
StandardOutput=append:/var/log/datum/server.log
StandardError=append:/var/log/datum/error.log
```

### Log Aggregation (Recommended)

For production, use a log aggregation solution:

**1. Filebeat + ELK Stack**
```yaml
# docker-compose.yml
services:
  api:
    logging:
      driver: "json-file"
  
  filebeat:
    image: docker.elastic.co/beats/filebeat:7.15.0
    volumes:
      - /var/lib/docker/containers:/var/lib/docker/containers:ro
      - ./filebeat.yml:/usr/share/filebeat/filebeat.yml
```

**2. Fluentd**
```yaml
services:
  api:
    logging:
      driver: fluentd
      options:
        fluentd-address: localhost:24224
        tag: datum.server
```

**3. CloudWatch (AWS)**
```yaml
services:
  api:
    logging:
      driver: awslogs
      options:
        awslogs-group: /datum/server
        awslogs-stream: production
```

## Log Rotation

### Using Logrotate
```bash
# /etc/logrotate.d/datum
/var/log/datum/server.log {
    daily
    rotate 7
    compress
    delaycompress
    missingok
    notifempty
    create 0640 datum datum
    sharedscripts
    postrotate
        systemctl reload datum-server
    endscript
}
```

## Viewing Logs

### Docker Compose
```bash
# Follow all logs
docker compose logs -f

# Follow specific service
docker compose logs -f api

# Last 100 lines
docker compose logs --tail=100 api

# Since timestamp
docker compose logs --since 2025-12-27T10:00:00
```

### Systemd
```bash
# Follow logs
journalctl -u datum-server -f

# Last 100 lines
journalctl -u datum-server -n 100

# Today's logs
journalctl -u datum-server --since today
```

### Grep JSON Logs
```bash
# Find errors
cat server.log | jq 'select(.level=="error")'

# Count by level
cat server.log | jq -r .level | sort | uniq -c

# Find specific device
cat server.log | jq 'select(.device_id=="dev_123")'
```

## Structured Logging Examples

The logger automatically includes caller information and timestamps:

```go
// In your code
logger.Info().
    Str("device_id", deviceID).
    Int("data_points", count).
    Msg("Data ingested")

logger.Error().
    Err(err).
    Str("user_id", userID).
    Msg("Failed to create device")

logger.Debug().
    Interface("request", req).
    Msg("Received request")
```

## Performance

- **Pretty Format**: ~1.5 µs per log line (human-readable)
- **JSON Format**: ~0.8 µs per log line (optimized for parsing)
- **Zero Allocation**: Minimal GC pressure
- **Async**: Non-blocking I/O

## Best Practices

1. **Development**: Use `LOG_FORMAT=pretty` for readability
2. **Production**: Use `LOG_FORMAT=json` with log aggregation
3. **CI/CD**: Use `LOG_LEVEL=ERROR` to reduce noise
4. **Testing**: Use `LOG_LEVEL=FATAL` to suppress output
5. **Debugging**: Use `LOG_LEVEL=DEBUG` temporarily

## Troubleshooting

### No Logs Appearing
```bash
# Check log level
echo $LOG_LEVEL

# Verify stderr/stdout
./server 2>&1 | less

# Check Docker logs
docker compose logs api
```

### Too Many Logs
```bash
# Increase log level
export LOG_LEVEL=WARN

# Filter in production
LOG_LEVEL=ERROR ./server 2>&1 | grep -v "health check"
```

### JSON Parsing Errors
```bash
# Validate JSON format
./server 2>&1 | jq empty

# Pretty print
./server 2>&1 | jq -C | less -R
```

## Security Considerations

- **Never log sensitive data**: API keys, passwords, tokens
- **Redact PII**: User emails, device identifiers (in production)
- **Secure log files**: Set proper permissions (0640)
- **Encrypt in transit**: Use TLS for remote log shipping
- **Retention policy**: Rotate and delete old logs (GDPR compliance)

## References

- [zerolog documentation](https://github.com/rs/zerolog)
- [Docker logging drivers](https://docs.docker.com/config/containers/logging/)
- [systemd journal](https://www.freedesktop.org/software/systemd/man/systemd-journald.service.html)
