import { api } from '@/services/api';
import type { LoginResponse, APIKey, CreateKeyResponse, GetKeysResponse } from '@/types/auth';

export interface UserProfile {
    id: string;
    email: string;
    role: string;
    display_name?: string;
    status: string;
    ntfy_topic?: string;
    created_at: string;
    last_login_at?: string;
}

export interface Session {
    jti: string;
    user_id: string;
    created_at: string;
    expires_at: string;
    user_agent: string;
    ip: string;
}

export interface PushToken {
    id: string;
    platform: string;
    token: string;
    created_at: string;
}

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
    },

    forgotPassword: async (email: string): Promise<void> => {
        await api.post('/auth/forgot-password', { email });
    },

    resetPassword: async (token: string, password: string): Promise<void> => {
        await api.post('/auth/reset-password', { token, new_password: password });
    },

    getOAuthProviders: async (): Promise<string[]> => {
        const { data } = await api.get<{ providers: string[] }>('/auth/providers');
        return data.providers || [];
    },

    getProfile: async (): Promise<UserProfile> => {
        const { data } = await api.get<UserProfile>('/auth/me');
        return data;
    },

    updateProfile: async (displayName: string): Promise<UserProfile> => {
        const { data } = await api.put<UserProfile>('/auth/me', { display_name: displayName });
        return data;
    },

    getSessions: async (): Promise<Session[]> => {
        const { data } = await api.get<{ sessions: Session[] }>('/auth/sessions');
        return data.sessions || [];
    },

    revokeSession: async (jti: string): Promise<void> => {
        await api.delete(`/auth/sessions/${jti}`);
    },

    getPushTokens: async (): Promise<PushToken[]> => {
        const { data } = await api.get<{ tokens: PushToken[] }>('/auth/push-tokens');
        return data.tokens || [];
    },

    registerPushToken: async (platform: string, token: string): Promise<{ id: string }> => {
        const { data } = await api.post<{ id: string; message: string }>('/auth/push-token', { platform, token });
        return data;
    },

    deletePushToken: async (id: string): Promise<void> => {
        await api.delete(`/auth/push-token/${id}`);
    },
};
