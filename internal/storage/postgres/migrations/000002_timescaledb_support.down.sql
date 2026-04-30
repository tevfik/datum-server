-- Revert TimescaleDB-specific changes.
-- Note: Converting back from a hypertable requires dropping and recreating the table.
-- This is a destructive operation on the data_points table.

-- Remove policies (safe to call even if they don't exist).
DO $$
BEGIN
    PERFORM remove_retention_policy('data_points', if_exists => TRUE);
    PERFORM remove_compression_policy('data_points', if_exists => TRUE);
EXCEPTION WHEN OTHERS THEN
    RAISE NOTICE 'Policy removal skipped: %', SQLERRM;
END
$$;

-- Note: We do NOT drop the timescaledb extension or revert the hypertable
-- as that would destroy data. The application works fine with or without
-- TimescaleDB on the same table structure.
