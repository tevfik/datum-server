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
}

export interface AdminUserListResponse {
    users: AdminUser[];
}
