export interface User {
    id: string;
    email: string;
    role: string;
}

export interface LoginResponse {
    token: string;
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
