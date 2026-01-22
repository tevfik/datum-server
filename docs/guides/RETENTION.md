# Data Retention Policy

Automatic cleanup of old time-series data.

## Configuration

Configure via environment variables:

| Variable | Default | Description |
|----------|---------|-------------|
| `RETENTION_MAX_DAYS` | 7 | Days to keep data |
| `RETENTION_CHECK_HOURS` | 1 | (Deprecated) Hours between cleanup runs |

### docker-compose.yml
```yaml
environment:
  - RETENTION_MAX_DAYS=30       # Keep 30 days
  - RETENTION_CHECK_HOURS=6     # Check every 6 hours
```

### Direct Run
```bash
RETENTION_MAX_DAYS=30 RETENTION_CHECK_HOURS=6 ./server
```

## Partition Format

tstorage creates hourly partitions:
```
./data/tsdata/
├── p-1703275200-1703278799/   # Dec 22, 2025 20:00-21:00
├── p-1703278800-1703282399/   # Dec 22, 2025 21:00-22:00
└── p-1703282400-1703285999/   # Dec 22, 2025 22:00-23:00
```

## Customizing Retention

Edit `main.go`:
```go
// 30 days retention, cleanup every 6 hours
config := storage.RetentionConfig{
    MaxAge:       30 * 24 * time.Hour,
    CleanupEvery: 6 * time.Hour,
}
store.StartRetentionWorker(config, "/root/data/tsdata")
```

## Manual Cleanup

```bash
# Delete partitions older than 7 days
find ./data/tsdata -name "p-*" -mtime +7 -exec rm -rf {} \;
```

## Logs

Retention worker logs cleanup activity:
```
🧹 Retention worker started: delete data older than 168h0m0s, check every 1h0m0s
🧹 Retention: deleted 3 old partition(s)
```
