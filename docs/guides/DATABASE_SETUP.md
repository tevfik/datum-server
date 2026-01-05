# Database Setup & Migration Guide

Datum Server supports two storage backends:
1.  **Embedded BuntDB/tstorage** (Default) - Zero-configuration, file-based. Best for development and small deployments.
2.  **PostgreSQL** (Recommended for Production) - Scalable, reliable, supports TimescaleDB.

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

## 2. Production (PostgreSQL)

To use PostgreSQL, simply set the `DATABASE_URL` environment variable. The server will automatically switch backends and initialize the schema.

### Requirements
- PostgreSQL 13+ (TimescaleDB extension recommended for high-volume telemetry)

### Configuration
Set the connection string in your environment or `.env` file:
```bash
export DATABASE_URL="postgres://user:password@localhost:5432/datum?sslmode=disable"
```

### Running
```bash
go run ./cmd/server
```
Logs will indicate: `type=postgres`.

---

## 3. Migrating Data (BuntDB -> Postgres)

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
