export interface User {
    id: string;
    email: string;
    role: string;
}

export interface LoginResponse {
    token: string;
    refresh_token?: string;
    user_id: string;
    email: string;
    role: string;
    expires_at: string;
}

export interface AuthState {
    user: User | null;
    token: string | null;
    isAuthenticated: boolean;
    isLoading: boolean;
}

export interface APIKey {
    id: string;
    name: string;
    key: string; // Masked usually, full only on create response
    created_at: string;
}

export interface CreateKeyRequest {
    name: string;
}

export interface CreateKeyResponse {
    id: string;
    name: string;
    key: string;
    created_at: string;
}

export interface GetKeysResponse {
    keys: APIKey[];
}
