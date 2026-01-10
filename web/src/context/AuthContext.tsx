import React, { createContext, useContext, useEffect, useState } from 'react';
import type { User, AuthState } from '@/types/auth';

interface AuthContextType extends AuthState {
    login: (token: string, user: User, expiresAt: string) => void;
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

        if (token && userStr && expiry) {
            if (new Date(expiry) > new Date()) {
                try {
                    const user = JSON.parse(userStr);
                    setState({
                        user,
                        token,
                        isAuthenticated: true,
                        isLoading: false,
                    });
                } catch (e) {
                    // Invalid stored data
                    logout();
                }
            } else {
                // Token expired
                logout();
            }
        } else {
            setState((prev) => ({ ...prev, isLoading: false }));
        }
    }, []);

    const login = (token: string, user: User, expiresAt: string) => {
        localStorage.setItem('datum_token', token);
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
