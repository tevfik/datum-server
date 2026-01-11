-- Users Table
CREATE TABLE IF NOT EXISTS users (
    id TEXT PRIMARY KEY,
    email TEXT UNIQUE NOT NULL,
    password_hash TEXT NOT NULL,
    role TEXT NOT NULL,
    status TEXT NOT NULL,
    created_at TIMESTAMPTZ NOT NULL,
    updated_at TIMESTAMPTZ,
    last_login_at TIMESTAMPTZ,
    refresh_token TEXT
);

-- Devices Table
CREATE TABLE IF NOT EXISTS devices (
    id TEXT PRIMARY KEY,
    user_id TEXT REFERENCES users(id) ON DELETE CASCADE,
    name TEXT NOT NULL,
    type TEXT NOT NULL,
    device_uid TEXT,
    api_key TEXT UNIQUE NOT NULL,
    status TEXT NOT NULL,
    last_seen TIMESTAMPTZ NOT NULL,
    created_at TIMESTAMPTZ NOT NULL,
    updated_at TIMESTAMPTZ,
    firmware_version TEXT,
    
    -- Token Auth Fields
    master_secret TEXT,
    current_token TEXT,
    previous_token TEXT,
    token_issued_at TIMESTAMPTZ,
    token_expires_at TIMESTAMPTZ,
    grace_period_end TIMESTAMPTZ,
    key_revoked_at TIMESTAMPTZ,
    
    -- Device Shadow (Latest State)
    shadow_state JSONB
);

CREATE INDEX IF NOT EXISTS idx_devices_user_id ON devices(user_id);
CREATE INDEX IF NOT EXISTS idx_devices_device_uid ON devices(device_uid);
CREATE INDEX IF NOT EXISTS idx_devices_current_token ON devices(current_token);

-- Data Points Table (Time Series)
-- In a real TimescaleDB setup, we would convert this to a hypertable.
CREATE TABLE IF NOT EXISTS data_points (
    time TIMESTAMPTZ NOT NULL,
    device_id TEXT NOT NULL REFERENCES devices(id) ON DELETE CASCADE,
    data JSONB NOT NULL
);

-- Composite index for fast range queries per device
CREATE INDEX IF NOT EXISTS idx_data_points_device_time ON data_points (device_id, time DESC);

-- Commands Table
CREATE TABLE IF NOT EXISTS commands (
    id TEXT PRIMARY KEY,
    device_id TEXT REFERENCES devices(id) ON DELETE CASCADE,
    action TEXT NOT NULL,
    params JSONB,
    status TEXT NOT NULL,
    created_at TIMESTAMPTZ NOT NULL,
    expires_at TIMESTAMPTZ,
    acked_at TIMESTAMPTZ,
    result JSONB
);

CREATE INDEX IF NOT EXISTS idx_commands_device_pending ON commands(device_id) WHERE status = 'pending';

-- Provisioning Requests Table
CREATE TABLE IF NOT EXISTS provisioning_requests (
    id TEXT PRIMARY KEY,
    device_uid TEXT NOT NULL,
    user_id TEXT REFERENCES users(id) ON DELETE CASCADE,
    device_name TEXT NOT NULL,
    device_type TEXT NOT NULL,
    status TEXT NOT NULL,
    device_id TEXT,
    api_key TEXT,
    server_url TEXT,
    wifi_ssid TEXT,
    wifi_pass TEXT,
    expires_at TIMESTAMPTZ NOT NULL,
    created_at TIMESTAMPTZ NOT NULL,
    completed_at TIMESTAMPTZ
);

CREATE INDEX IF NOT EXISTS idx_provisioning_uid ON provisioning_requests(device_uid);
CREATE INDEX IF NOT EXISTS idx_provisioning_user ON provisioning_requests(user_id);

-- Password Reset Tokens
CREATE TABLE IF NOT EXISTS password_reset_tokens (
    token TEXT PRIMARY KEY,
    user_id TEXT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    expires_at TIMESTAMPTZ NOT NULL,
    created_at TIMESTAMPTZ DEFAULT NOW()
);

-- User API Keys
CREATE TABLE IF NOT EXISTS user_api_keys (
    id TEXT PRIMARY KEY,
    user_id TEXT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    name TEXT NOT NULL,
    key_value TEXT UNIQUE NOT NULL, -- "key" is reserved word often
    created_at TIMESTAMPTZ DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_user_api_keys_user_id ON user_api_keys(user_id);
CREATE INDEX IF NOT EXISTS idx_user_api_keys_value ON user_api_keys(key_value);

-- System Settings (Key-Value Store for Config)
CREATE TABLE IF NOT EXISTS system_settings (
    key TEXT PRIMARY KEY,
    value JSONB NOT NULL,
    updated_at TIMESTAMPTZ DEFAULT NOW()
);
