import React, { createContext, useContext, useEffect, useState } from 'react';
import type { User, AuthState } from '@/types/auth';

interface AuthContextType extends AuthState {
    login: (token: string, refreshToken: string | undefined, user: User, expiresAt: string) => void;
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
        // Check for stored token on mount
        const token = localStorage.getItem('datum_token');
        const userStr = localStorage.getItem('datum_user');
        const expiry = localStorage.getItem('datum_token_expiry');
        // We generally don't check refresh token existence here, 
        // as long as access token appears valid or we use it.
        // If access token is expired, the interceptor will try refresh.
        // Actually, if access token is expired here, we might want to try refresh immediately?
        // For now, let's stick to existing logic: if expired, logout.
        // Use interceptor for API calls.

        if (token && userStr && expiry) {
            // Check implicit expiry (optional, since server validates)
            // Allow expired token if we have refresh token?
            // No, for UI state we might want to show "Authenticated" but 
            // api calls will fail and refresh. If we set isAuthenticated=false,
            // the UI might redirect to login immediately.
            // So, we should be lenient here if we have a refresh token?
            const refreshToken = localStorage.getItem('datum_refresh_token');
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

    const login = (token: string, refreshToken: string | undefined, user: User, expiresAt: string) => {
        localStorage.setItem('datum_token', token);
        if (refreshToken) {
            localStorage.setItem('datum_refresh_token', refreshToken);
        }
        localStorage.setItem('datum_user', JSON.stringify(user));
        localStorage.setItem('datum_token_expiry', expiresAt);

        setState({
            user,
            token,
            isAuthenticated: true,
            isLoading: false,
        });
    };

    const logout = () => {
        localStorage.removeItem('datum_token');
        localStorage.removeItem('datum_refresh_token');
        localStorage.removeItem('datum_user');
        localStorage.removeItem('datum_token_expiry');

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
