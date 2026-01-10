export interface Device {
    id: string;
    name: string;
    type: string;
    device_uid: string;
    public_ip: string;
    last_seen: string;
    created_at: string;
    status: 'online' | 'offline';
}

export interface DeviceListResponse {
    devices: Device[];
}

export interface CreateDeviceRequest {
    name: string;
    type: string;
}

export interface CreateDeviceResponse {
    device_id: string;
    api_key: string;
    message: string;
}
