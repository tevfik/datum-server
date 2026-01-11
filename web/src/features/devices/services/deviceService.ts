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
    // List all devices
    getAll: async (): Promise<Device[]> => {
        const { data } = await api.get<DeviceListResponse>('/devices');
        return data.devices;
    },

    // Get a single device
    getById: async (id: string): Promise<Device> => {
        const { data } = await api.get<Device>(`/devices/${id}`);
        return data;
    },

    // Create a new device
    create: async (device: CreateDeviceRequest): Promise<CreateDeviceResponse> => {
        const { data } = await api.post<CreateDeviceResponse>('/devices', device);
        return data;
    },

    // Delete a device
    delete: async (id: string): Promise<void> => {
        await api.delete(`/devices/${id}`);
    },

    // Get pending commands for a device
    getCommands: async (deviceId: string): Promise<Command[]> => {
        const { data } = await api.get<{ commands: Command[] }>(`/devices/${deviceId}/commands`);
        return data.commands || [];
    },

    // Send a command to a device
    sendCommand: async (deviceId: string, command: SendCommandRequest): Promise<SendCommandResponse> => {
        const { data } = await api.post<SendCommandResponse>(`/devices/${deviceId}/commands`, command);
        return data;
    },

    // Get telemetry history
    getHistory: async (deviceId: string): Promise<TelemetryPoint[]> => {
        const { data } = await api.get<TelemetryHistoryResponse>(`/data/${deviceId}/history`);
        return data.data || [];
    }
};
