# tstorage - Time Series Database

High-performance embedded time-series storage for IoT data.

## Architecture

```
┌─────────────────┐     ┌─────────────────┐
│     BuntDB      │     │    tstorage     │
│   (meta.db)     │     │   (tsdata/)     │
├─────────────────┤     ├─────────────────┤
│ • Users         │     │ • Data points   │
│ • Devices       │     │ • 3M writes/sec │
│ • Commands      │     │ • 305ns latency │
│ • API keys      │     │ • Auto-partition│
└─────────────────┘     └─────────────────┘
```

## Performance

| Metric | Value |
|--------|-------|
| Write latency | 305 ns/op |
| Read (1K points) | 222 ns/op |
| Read (1M points) | 292 ns/op |
| Throughput | ~3.3M writes/sec |

## Data Format

Each numeric field stored as separate metric:
```
{device_id}.{field_name} → value, timestamp
```

Example:
```
dev_123.temperature → 25.5, 1703278545000000000
dev_123.humidity → 60.0, 1703278545000000000
```

## Partitioning

Data auto-partitioned by hour:
```
./data/tsdata/
├── p-1703275200-1703278799/  # Hour 1
│   ├── data
│   └── meta.json
├── p-1703278800-1703282399/  # Hour 2
└── ...
```

## Retention (Manual)

Delete old partitions:
```bash
# Delete partitions older than 7 days
find ./data/tsdata -name "p-*" -mtime +7 -exec rm -rf {} \;
```

## Files

- `./data/meta.db` - BuntDB (users, devices, commands)
- `./data/tsdata/` - tstorage partitions (time-series data)
