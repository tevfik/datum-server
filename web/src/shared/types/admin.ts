export interface AdminUser {
    id: string;
    email: string;
    role: string;
    status: string;
    created_at: string;
    last_login_at?: string;
    device_count: number;
}

export interface SystemStats {
    db_size_bytes: number;
    total_users: number;
    active_devices: number;
    total_devices: number;
    platform_name: string;
    data_retention_days: number;
    public_data_retention_days?: number;
    allow_register?: boolean;
    // New fields
    server_time?: string;
    server_uptime_seconds?: number;
    env_vars?: Record<string, string>;
}

export interface AdminUserListResponse {
    users: AdminUser[];
}

// MQTT Types
export interface BrokerStats {
    bytes_recv: number;
    bytes_sent: number;
    clients_connected: number;
    subscriptions: number;
    inflight: number;
}

export interface MqttClient {
    id: string;
    ip: string;
    connected: boolean;
}

export interface MqttClientListResponse {
    clients: MqttClient[];
}

export interface PublishRequest {
    topic: string;
    message: string;
    retain: boolean;
}

// System Config Types
export interface SystemConfig {
    initialized: boolean;
    setup_at?: string;
    platform_name: string;
    allow_register: boolean;
    data_retention: number;
    public_data_retention: number;
}

export interface LogEntry {
    raw: string;
}

export interface LogsResponse {
    logs: string[];
    total: number;
    message?: string;
}

export interface UpdateRetentionRequest {
    days: number;
    public_days: number;
    check_interval_hours: number;
}

export interface UpdateRegistrationRequest {
    allow_register: boolean;
}
