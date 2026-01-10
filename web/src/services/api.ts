import axios from 'axios';

// Create base axios instance
export const api = axios.create({
    baseURL: import.meta.env.VITE_API_URL || '/api', // Proxy handles /api locally, direct integration in prod
    headers: {
        'Content-Type': 'application/json',
    },
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
