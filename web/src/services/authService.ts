import { api } from './api';
import type { LoginResponse, APIKey, CreateKeyResponse, GetKeysResponse } from '@/types/auth';

export const authService = {
    login: async (email: string, password: string): Promise<LoginResponse> => {
        const { data } = await api.post<LoginResponse>('/auth/login', { email, password });
        return data;
    },

    getKeys: async (): Promise<APIKey[]> => {
        const { data } = await api.get<GetKeysResponse>('/auth/keys');
        return data.keys || [];
    },

    createKey: async (name: string): Promise<CreateKeyResponse> => {
        const { data } = await api.post<CreateKeyResponse>('/auth/keys', { name });
        return data;
    },


    deleteKey: async (id: string): Promise<void> => {
        await api.delete(`/auth/keys/${id}`);
    },

    changePassword: async (password: string): Promise<void> => {
        await api.put('/auth/password', { password });
    }
};
