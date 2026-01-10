import { api } from './api';
import type { Device, DeviceListResponse, CreateDeviceRequest, CreateDeviceResponse } from '@/types/device';

export const deviceService = {
    // List all devices
    getAll: async (): Promise<Device[]> => {
        const { data } = await api.get<DeviceListResponse>('/devices');
        return data.devices;
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
};
