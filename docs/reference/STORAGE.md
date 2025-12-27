# BuntDB Storage Explanation

## Where is data stored?

**File Location:**
- **Container**: `/app/data/data.db`
- **Host**: `./data/data.db` (in project root)
- **Type**: Persistent file on disk

## How BuntDB Works

**BuntDB** is an embedded database (like SQLite) that:
1. ✅ **Writes to disk** - All data is persisted
2. ✅ **Survives restarts** - Data remains after container restart
3. ✅ **ACID compliant** - Transactions are durable
4. ✅ **Single file** - Easy to backup/restore

**Not RAM-only!** BuntDB uses memory-mapped files for speed but **always writes to disk**.

## Data Flow

```
User Registration
     ↓
Go API Server
     ↓
storage.CreateUser()
     ↓
BuntDB Transaction
     ↓
Write to /app/data/data.db (DISK)
     ↓
Docker Volume Mount
     ↓
./data/data.db (HOST)
```

## Verification

**Check database file:**
```bash
# On host
ls -lh ./data/data.db

# In container
docker exec datumpy-api-1 ls -lh /app/data/data.db
```

**Database grows with data:**
```bash
# Initial size (empty)
~1KB

# After 1000 users + 10000 data points
~500KB - 1MB
```

## Docker Volume Mount

From `docker-compose.yml`:
```yaml
services:
  api:
    volumes:
      - ./data:/app/data  # Host:Container mapping
```

This ensures:
- Data persists across container restarts
- Easy backup (just copy `./data/` folder)
- Can be inspected from host

## BuntDB vs Other Databases

| Feature | BuntDB | PostgreSQL | SQLite |
|---------|--------|------------|--------|
| **Persistence** | ✅ Disk | ✅ Disk | ✅ Disk |
| **File-based** | ✅ Yes | ❌ No | ✅ Yes |
| **Embedded** | ✅ Yes | ❌ No | ✅ Yes |
| **Transactions** | ✅ ACID | ✅ ACID | ✅ ACID |
| **Concurrency** | Read-heavy | Read+Write | Read-heavy |

## Data Structure in BuntDB

**Key-Value pairs stored on disk:**
```
user:usr_abc123 → {"id":"usr_abc123","email":"test@example.com","password_hash":"$2a$10..."}
user:email:test@example.com → usr_abc123
device:dev_xyz789 → {"id":"dev_xyz789","user_id":"usr_abc123",...}
device:dev_xyz789:data:1766176576000000000 → {"device_id":"dev_xyz789","data":{...}}
```

**Each key-value pair is written to disk immediately** (with transaction guarantees).

## Backup & Restore

**Backup:**
```bash
# Stop services
docker-compose down

# Copy database
cp ./data/data.db ./backups/data-$(date +%Y%m%d).db

# Restart
docker-compose up -d
```

**Restore:**
```bash
docker-compose down
cp ./backups/data-20251219.db ./data/data.db
docker-compose up -d
```

## Performance

BuntDB is **fast** because:
- Memory-mapped I/O (OS caches frequently accessed data)
- B-tree index (O(log n) lookups)
- Single-file design (no network overhead)

But still **durable** because:
- All writes go through `fsync()`
- Transactions are atomic
- Crash recovery built-in

## Summary

✅ **Your data is safe!**
- Stored in `./data/data.db` on host
- Persists across restarts
- ACID transactions
- Easy to backup

❌ **Not RAM-only**
- BuntDB is a persistent database
- All data written to disk
- Survives power loss (with transaction guarantees)
