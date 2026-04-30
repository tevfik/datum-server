import React, { createContext, useContext, useEffect, useState } from 'react';
import type { User, AuthState } from '@/types/auth';

interface AuthContextType extends AuthState {
    login: (token: string, refreshToken: string | undefined, user: User, expiresAt: string, remember?: boolean) => void;
    logout: () => void;
}

const AuthContext = createContext<AuthContextType | undefined>(undefined);

export function AuthProvider({ children }: { children: React.ReactNode }) {
    const [state, setState] = useState<AuthState>({
        user: null,
        token: null,
        isAuthenticated: false,
        isLoading: true,
    });

    useEffect(() => {
        // Check for stored token on mount (Local OR Session)
        const getToken = (key: string) => localStorage.getItem(key) || sessionStorage.getItem(key);

        const token = getToken('datum_token');
        const userStr = getToken('datum_user');
        const expiry = getToken('datum_token_expiry');

        if (token && userStr && expiry) {
            const refreshToken = getToken('datum_refresh_token');
            const isExpired = new Date(expiry) <= new Date();

            if (!isExpired || (isExpired && refreshToken)) {
                try {
                    const user = JSON.parse(userStr);
                    setState({
                        user,
                        token,
                        isAuthenticated: true,
                        isLoading: false,
                    });
                } catch (e) {
                    logout();
                }
            } else {
                logout();
            }
        } else {
            setState((prev) => ({ ...prev, isLoading: false }));
        }
    }, []);

    const login = (token: string, refreshToken: string | undefined, user: User, expiresAt: string, remember: boolean = true) => {
        const storage = remember ? localStorage : sessionStorage;

        // Clear other storage to avoid duplicates
        const other = remember ? sessionStorage : localStorage;
        other.removeItem('datum_token');
        other.removeItem('datum_refresh_token');
        other.removeItem('datum_user');
        other.removeItem('datum_token_expiry');

        storage.setItem('datum_token', token);
        if (refreshToken) {
            storage.setItem('datum_refresh_token', refreshToken);
        }
        storage.setItem('datum_user', JSON.stringify(user));
        storage.setItem('datum_token_expiry', expiresAt);

        setState({
            user,
            token,
            isAuthenticated: true,
            isLoading: false,
        });
    };

    const logout = () => {
        // Best-effort: tell the server to revoke the JWT (jti) so a stolen
        // token can't be replayed. We don't await — the local cleanup below
        // must always run, even on offline / 401 / network failure.
        void (async () => {
            try {
                const { api } = await import('@/services/api');
                await api.post('/auth/logout');
            } catch {
                // Network/server failure is fine; server-side jti revocation
                // is a defense-in-depth measure on top of local clearing.
            }
        })();

        ['datum_token', 'datum_refresh_token', 'datum_user', 'datum_token_expiry'].forEach(key => {
            localStorage.removeItem(key);
            sessionStorage.removeItem(key);
        });

        setState({
            user: null,
            token: null,
            isAuthenticated: false,
            isLoading: false,
        });
    };

    return (
        <AuthContext.Provider value={{ ...state, login, logout }}>
            {children}
        </AuthContext.Provider>
    );
}

export function useAuth() {
    const context = useContext(AuthContext);
    if (context === undefined) {
        throw new Error('useAuth must be used within an AuthProvider');
    }
    return context;
}
