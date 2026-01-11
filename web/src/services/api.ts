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

// Request interceptor to add token
api.interceptors.request.use((config) => {
    const token = localStorage.getItem('datum_token');
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
                localStorage.removeItem('datum_token');
                localStorage.removeItem('datum_refresh_token');
                localStorage.removeItem('datum_user');
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

            const refreshToken = localStorage.getItem('datum_refresh_token');

            if (refreshToken) {
                try {
                    // Call refresh endpoint directly using a clean axios instance
                    const baseURL = api.defaults.baseURL;
                    const response = await axios.post(`${baseURL}/auth/refresh`, {
                        refresh_token: refreshToken
                    });

                    const { token, refresh_token: newRefreshToken } = response.data;

                    localStorage.setItem('datum_token', token);
                    if (newRefreshToken) {
                        localStorage.setItem('datum_refresh_token', newRefreshToken);
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
                    localStorage.removeItem('datum_token');
                    localStorage.removeItem('datum_refresh_token');
                    localStorage.removeItem('datum_user');
                    window.location.href = '/login';
                    return Promise.reject(refreshError);
                } finally {
                    isRefreshing = false;
                }
            } else {
                // No refresh token - logout
                localStorage.removeItem('datum_token');
                // Only redirect if not already on login
                if (!window.location.pathname.includes('/login')) {
                    window.location.href = '/login';
                }
            }
        }
        return Promise.reject(error);
    }
);
