import { api } from './api';
import type { AdminUser, AdminUserListResponse, SystemStats } from '@/types/admin';

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
    }
};
