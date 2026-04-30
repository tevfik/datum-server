import axios from 'axios';

// Create base axios instance
export const api = axios.create({
    // In production (PROD), the React app is served by the Go server, so APIs are at root (/)
    // In development (DEV), we use the Vite proxy at /api -> localhost:8000
    baseURL: import.meta.env.VITE_API_URL || (import.meta.env.DEV ? '/api' : '/'),
    headers: {
        'Content-Type': 'application/json',
    },
});

// Helper to get token from available storage
const getStoredToken = (key: string): string | null => {
    return localStorage.getItem(key) || sessionStorage.getItem(key);
};

// Helper to set token to the correct storage (based on where refresh token exists)
const setStoredToken = (key: string, value: string) => {
    if (localStorage.getItem('datum_refresh_token')) {
        localStorage.setItem(key, value);
    } else {
        sessionStorage.setItem(key, value);
    }
};

// Helper to clear tokens
const clearTokens = () => {
    ['datum_token', 'datum_refresh_token', 'datum_user'].forEach(key => {
        localStorage.removeItem(key);
        sessionStorage.removeItem(key);
    });
};

// Request interceptor to add token
api.interceptors.request.use((config) => {
    const token = getStoredToken('datum_token');
    if (token) {
        config.headers.Authorization = `Bearer ${token}`;
    }
    return config;
});

// Response interceptor for error handling
let isRefreshing = false;
let failedQueue: any[] = [];

const processQueue = (error: any, token: string | null = null) => {
    failedQueue.forEach(prom => {
        if (error) {
            prom.reject(error);
        } else {
            prom.resolve(token);
        }
    });

    failedQueue = [];
};

api.interceptors.response.use(
    (response) => response,
    async (error) => {
        const originalRequest = error.config;

        // Handle 401 Unauthorized
        if (error.response?.status === 401 && !originalRequest._retry) {
            // Check if this was already a refresh request to avoid infinite loops
            if (originalRequest.url?.includes('/auth/refresh')) {
                clearTokens();
                window.location.href = '/login';
                return Promise.reject(error);
            }

            if (isRefreshing) {
                return new Promise(function (resolve, reject) {
                    failedQueue.push({ resolve, reject });
                }).then(token => {
                    originalRequest.headers['Authorization'] = 'Bearer ' + token;
                    return api(originalRequest);
                }).catch(err => {
                    return Promise.reject(err);
                });
            }

            originalRequest._retry = true;
            isRefreshing = true;

            const refreshToken = getStoredToken('datum_refresh_token');

            if (refreshToken) {
                try {
                    // Call refresh endpoint directly using a clean axios instance
                    const baseURL = api.defaults.baseURL;
                    const response = await axios.post(`${baseURL}/auth/refresh`, {
                        refresh_token: refreshToken
                    });

                    const { token, refresh_token: newRefreshToken } = response.data;

                    setStoredToken('datum_token', token);
                    if (newRefreshToken) {
                        setStoredToken('datum_refresh_token', newRefreshToken);
                    }

                    // Update headers and retry
                    api.defaults.headers.common['Authorization'] = `Bearer ${token}`;

                    processQueue(null, token);

                    originalRequest.headers['Authorization'] = `Bearer ${token}`;
                    return api(originalRequest);
                } catch (refreshError) {
                    processQueue(refreshError, null);
                    // Refresh failed - logout
                    console.error("Session expired", refreshError);
                    clearTokens();
                    window.location.href = '/login';
                    return Promise.reject(refreshError);
                } finally {
                    isRefreshing = false;
                }
            } else {
                // No refresh token - logout
                clearTokens();
                // Only redirect if not already on login
                if (!window.location.pathname.includes('/login')) {
                    window.location.href = '/login';
                }
            }
        }
        return Promise.reject(error);
    }
);
