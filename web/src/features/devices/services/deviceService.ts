import { api } from '@/services/api';
import type {
    Device,
    DeviceListResponse,
    CreateDeviceRequest,
    CreateDeviceResponse,
    Command,
    SendCommandRequest,
    SendCommandResponse,
    TelemetryPoint,
    TelemetryHistoryResponse
} from '@/types/device';

export const deviceService = {
    // List all devices (user-scoped)
    getAll: async (): Promise<Device[]> => {
        const { data } = await api.get<DeviceListResponse>('/dev');
        return data.devices;
    },

    // List all devices (admin - all users)
    getAllAdmin: async (): Promise<Device[]> => {
        const { data } = await api.get<{ devices: Device[] }>('/admin/dev');
        return data.devices;
    },

    // Get a single device
    getById: async (id: string): Promise<Device> => {
        const { data } = await api.get<Device>(`/dev/${id}`);
        return data;
    },

    // Create a new device
    create: async (device: CreateDeviceRequest): Promise<CreateDeviceResponse> => {
        const { data } = await api.post<CreateDeviceResponse>('/dev', device);
        return data;
    },

    // Delete a device
    delete: async (id: string): Promise<void> => {
        await api.delete(`/dev/${id}`);
    },

    // Update device status (admin only)
    updateStatus: async (id: string, status: 'active' | 'suspended' | 'banned'): Promise<void> => {
        await api.put(`/admin/dev/${id}`, { status });
    },

    // Get pending commands for a device
    getCommands: async (deviceId: string): Promise<Command[]> => {
        const { data } = await api.get<{ commands: Command[] }>(`/dev/${deviceId}/cmd`);
        return data.commands || [];
    },

    // Send a command to a device
    sendCommand: async (deviceId: string, command: SendCommandRequest): Promise<SendCommandResponse> => {
        const { data } = await api.post<SendCommandResponse>(`/dev/${deviceId}/cmd`, command);
        return data;
    },

    // Get telemetry history
    getHistory: async (deviceId: string): Promise<TelemetryPoint[]> => {
        const { data } = await api.get<TelemetryHistoryResponse>(`/dev/${deviceId}/rec`);
        return data.data || [];
    }
};
