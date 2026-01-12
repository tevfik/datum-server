import { api } from '@/services/api';
import type { AdminUser, AdminUserListResponse, SystemStats, BrokerStats, MqttClient, PublishRequest, SystemConfig, LogsResponse, UpdateRetentionRequest, UpdateRegistrationRequest } from '@/types/admin';

export const adminService = {
    getUsers: async (): Promise<AdminUser[]> => {
        const { data } = await api.get<AdminUserListResponse>('/admin/users');
        return data.users || [];
    },

    deleteUser: async (id: string): Promise<void> => {
        await api.delete(`/admin/users/${id}`);
    },

    getSystemStats: async (): Promise<SystemStats> => {
        const { data } = await api.get<SystemStats>('/admin/database/stats');
        return data;
    },

    // MQTT
    getMqttStats: async (): Promise<BrokerStats> => {
        const { data } = await api.get<BrokerStats>('/admin/mqtt/stats');
        return data;
    },

    getMqttClients: async (): Promise<MqttClient[]> => {
        const { data } = await api.get<{ clients: MqttClient[] }>('/admin/mqtt/clients');
        return data.clients || [];
    },

    publishMqttMessage: async (req: PublishRequest): Promise<void> => {
        await api.post('/admin/mqtt/publish', req);
    },

    // System Config & Logs
    getSystemConfig: async (): Promise<SystemConfig> => {
        const { data } = await api.get<SystemConfig>('/admin/config');
        return data;
    },

    updateRetention: async (req: UpdateRetentionRequest): Promise<void> => {
        await api.put('/admin/config/retention', req);
    },

    updateRegistration: async (req: UpdateRegistrationRequest): Promise<void> => {
        await api.put('/admin/config/registration', req);
    },

    // eslint-disable-next-line @typescript-eslint/no-unused-vars
    getLogs: async (_lines: number = 100): Promise<LogsResponse> => {
        // admin.go uses 'limit' or just returns max 500, but let's just GET
        // Actually admin.go doesn't support limit param in readLastLines call directly in handler (hardcoded 500)
        // But handler accepts query? No, handler calls readLastLines(path, 500).
        // It accepts `type`, `level`, `search`.
        const { data } = await api.get<LogsResponse>(`/admin/logs`);
        return data;
    },

    clearLogs: async (): Promise<void> => {
        await api.delete('/admin/logs');
    },

    uploadFirmware: async (file: File): Promise<{ url: string; filename: string }> => {
        const formData = new FormData();
        formData.append('firmware', file);
        const { data } = await api.post('/admin/firmware', formData, {
            headers: { 'Content-Type': 'multipart/form-data' },
        });
        return data;
    },

    resetSystem: async (): Promise<void> => {
        await api.delete('/admin/database/reset', { data: { confirm: 'RESET' } });
    },

    getSystemInfo: async (): Promise<{ version: string; build_date: string; go_version: string; os: string; arch: string }> => {
        const { data } = await api.get('/sys/info');
        return data;
    }
};
