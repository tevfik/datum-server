-- Add display_name to users
ALTER TABLE users ADD COLUMN IF NOT EXISTS display_name TEXT;

-- Sessions table for multi-device refresh token tracking
CREATE TABLE IF NOT EXISTS sessions (
    jti        TEXT PRIMARY KEY,
    user_id    TEXT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    created_at TIMESTAMPTZ NOT NULL,
    expires_at TIMESTAMPTZ NOT NULL,
    user_agent TEXT,
    ip         TEXT
);
CREATE INDEX IF NOT EXISTS idx_sessions_user_id ON sessions(user_id);
CREATE INDEX IF NOT EXISTS idx_sessions_expires_at ON sessions(expires_at);

-- Push tokens for mobile push notifications
CREATE TABLE IF NOT EXISTS push_tokens (
    id         TEXT PRIMARY KEY,
    user_id    TEXT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    platform   TEXT NOT NULL, -- 'fcm' | 'apns'
    token      TEXT NOT NULL,
    created_at TIMESTAMPTZ NOT NULL,
    UNIQUE (user_id, token)
);
CREATE INDEX IF NOT EXISTS idx_push_tokens_user_id ON push_tokens(user_id);
