export interface Device {
    id: string;
    name: string;
    type: string;
    device_uid: string;
    public_ip: string;
    last_seen: string;
    created_at: string;
    status: 'online' | 'offline';  // Connection status
    admin_status?: 'active' | 'suspended' | 'banned';  // Administrative status
    shadow_state?: Record<string, any>;
    thing_description?: Record<string, any>;
}

export interface DeviceListResponse {
    devices: Device[];
}

export interface CreateDeviceRequest {
    name: string;
    type: string;
    device_uid?: string;
}

export interface CreateDeviceResponse {
    device_id: string;
    api_key: string;
    message: string;
}

export interface Command {
    command_id: string;
    action: string;
    params: Record<string, any>;
    status?: string;
    created_at: string;
}

export interface SendCommandRequest {
    action: string;
    params?: Record<string, any>;
    expires_in?: number;
}

export interface SendCommandResponse {
    command_id: string;
    status: string;
    message: string;
    expires_at: string;
}

export interface TelemetryPoint {
    timestamp: number;
    data: Record<string, number | string | boolean>;
}

export interface TelemetryHistoryResponse {
    data: TelemetryPoint[];
}

export interface DeviceStats {
    total_devices: number;
    online_devices: number;
    offline_devices: number;
}
