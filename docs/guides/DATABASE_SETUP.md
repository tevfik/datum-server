# Database Setup & Migration Guide

Datum Server supports two storage backends:
1.  **Embedded BuntDB/tstorage** (Default) - Zero-configuration, file-based. Best for development and small deployments.
2.  **PostgreSQL/TimescaleDB** (Recommended for Production) - Scalable, reliable, optimized for time-series data.

## 1. Development (BuntDB)

This is the default mode. No configuration is required.
Data is stored locally in the `./data` directory.

### Running
```bash
go run ./cmd/server
```
or 
```bash
./datum-server
```
Logs will indicate: `backend=BuntDB`.

---

## 2. Production (PostgreSQL/TimescaleDB)

To use PostgreSQL, simply set the `DATABASE_URL` environment variable. The server will automatically switch backends and initialize the schema.

### Requirements
- PostgreSQL 13+ (TimescaleDB 2.x recommended for high-volume telemetry)

### Configuration
Set the connection string in your environment or `.env` file:
```bash
export DATABASE_URL="postgres://user:password@localhost:5432/datum?sslmode=disable"
```

### Running
```bash
go run ./cmd/server
```
Logs will indicate: `backend=PostgreSQL`.

---

## 3. Connection Pool Settings

The server configures the database connection pool via environment variables. Defaults are tuned for typical workloads.

| Variable | Description | Default |
|---|---|---|
| `POSTGRES_MAX_OPEN_CONNS` | Maximum open connections to the database | 25 |
| `POSTGRES_MAX_IDLE_CONNS` | Maximum idle connections in the pool | 25 |
| `POSTGRES_CONN_MAX_LIFETIME_MINUTES` | Maximum lifetime of a connection (minutes) | 5 |

**Example:**
```bash
export POSTGRES_MAX_OPEN_CONNS=50
export POSTGRES_MAX_IDLE_CONNS=25
export POSTGRES_CONN_MAX_LIFETIME_MINUTES=10
```

---

## 4. SSL Configuration

PostgreSQL connection URLs support SSL modes via the `sslmode` query parameter.

| Mode | Description |
|---|---|
| `disable` | No SSL. Only for local/trusted networks. |
| `require` | Require SSL, but don't verify the certificate. |
| `verify-ca` | Require SSL, verify CN matches. Certificate file may be needed. |
| `verify-full` | Strictest. Verify CA and CN. |

**Example (SSL required):**
```bash
export DATABASE_URL="postgres://user:pass@host:5432/datum?sslmode=require"
```

---

## 5. TimescaleDB Setup (Optional, Recommended)

TimescaleDB is a PostgreSQL extension optimized for time-series data. It provides automatic partitioning (hypertables) and compression for high-volume telemetry.

### Step 1: Install TimescaleDB Extension
On your PostgreSQL server:
```sql
CREATE EXTENSION IF NOT EXISTS timescaledb CASCADE;
```

### Step 2: Convert `data_points` to Hypertable
After the server creates the `data_points` table, run:
```sql
SELECT create_hypertable('data_points', 'time', if_not_exists => TRUE, migrate_data => TRUE);
```
> **Note:** Setting a `chunk_time_interval` is optional. Default is 7 days.

### Step 3: Enable Compression (Optional)
For long-term storage efficiency:
```sql
ALTER TABLE data_points SET (
  timescaledb.compress,
  timescaledb.compress_segmentby = 'device_id'
);
SELECT add_compression_policy('data_points', INTERVAL '7 days');
```

---

## 6. Migrating Data (BuntDB -> Postgres)

If you started with BuntDB and want to move to PostgreSQL, use the provided migration tool.

### Steps

1.  **Stop the Server**: Ensure `datum-server` is not running to prevent data corruption.
2.  **Start PostgreSQL**: Ensure your target database is running and accessible.
3.  **Run Migration**:
    Run the migration script pointing to your data directory and target database.

    ```bash
    # Run from project root
    go run scripts/migrate_db/main.go \
      --src ./data \
      --dest "postgres://user:password@localhost:5432/datum?sslmode=disable" \
      --days 90
    ```

    **Flags:**
    - `--src`: Path to source data directory containing `meta.db` (default: `./data`).
    - `--dest`: **(Required)** Target PostgreSQL connection URL.
    - `--days`: Number of days of telemetry history to migrate (default: 90).

4.  **Verify**:
    Start the server with `DATABASE_URL` set and check if users/devices are present.

### What is Migrated?
- ✅ **System Config** (Platform settings, Retention)
- ✅ **Users** (Roles, Passwords, Status)
- ✅ **API Keys** (User API Keys `ak_...`)
- ✅ **Devices** (Tokens, Secrets, Shadow State)
- ✅ **Provisioning Requests** (Pending requests only)
- ✅ **Telemetry** (Recent history based on `--days`)

### Troubleshooting
- **Duplicates**: The tool tries to avoid duplicates. If a record exists, it updates mutable fields (like roles/status) but skips insertion.
- **Connection Error**: Ensure the PostgreSQL URL is correct and the database exists (`createdb datum`).

