-- Enable TimescaleDB extension if available.
-- This is idempotent; if TimescaleDB is not installed it will be a no-op 
-- because the DO block catches the exception.
DO $$
BEGIN
    CREATE EXTENSION IF NOT EXISTS timescaledb;
    RAISE NOTICE 'TimescaleDB extension enabled';
EXCEPTION WHEN OTHERS THEN
    RAISE NOTICE 'TimescaleDB not available, using standard PostgreSQL';
END
$$;

-- Convert data_points to a hypertable if TimescaleDB is available.
-- This is idempotent; if the table is already a hypertable or if
-- TimescaleDB is not installed, the block catches the exception.
DO $$
BEGIN
    PERFORM create_hypertable('data_points', 'time',
        if_not_exists => TRUE,
        migrate_data => TRUE
    );
    RAISE NOTICE 'data_points converted to hypertable';
EXCEPTION WHEN OTHERS THEN
    RAISE NOTICE 'Hypertable conversion skipped: %', SQLERRM;
END
$$;

-- Add compression policy if TimescaleDB is available (compress after 7 days).
DO $$
BEGIN
    ALTER TABLE data_points SET (
        timescaledb.compress,
        timescaledb.compress_segmentby = 'device_id'
    );
    PERFORM add_compression_policy('data_points', INTERVAL '7 days',
        if_not_exists => TRUE);
    RAISE NOTICE 'Compression policy added';
EXCEPTION WHEN OTHERS THEN
    RAISE NOTICE 'Compression policy skipped: %', SQLERRM;
END
$$;

-- Add retention policy if TimescaleDB is available.
-- The default retention is controlled by the app's config; this adds
-- a DB-level safety net of 365 days.
DO $$
BEGIN
    PERFORM add_retention_policy('data_points', INTERVAL '365 days',
        if_not_exists => TRUE);
    RAISE NOTICE 'Retention policy added';
EXCEPTION WHEN OTHERS THEN
    RAISE NOTICE 'Retention policy skipped: %', SQLERRM;
END
$$;
