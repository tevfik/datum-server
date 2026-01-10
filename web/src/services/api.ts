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
api.interceptors.response.use(
    (response) => response,
    (error) => {
        // Handle global errors here (e.g. 401 Unauthorized)
        if (error.response?.status === 401) {
            // Redirect to login or clear auth state
            console.warn('Unauthorized access');
        }
        return Promise.reject(error);
    }
);
