# Mobile Authentication Guide (Refresh Token)

This guide describes how to integrate the Datum IoT Platform's Refresh Token authentication flow into your mobile application (iOS/Android/Flutter/React Native).

## Overview

We use a standard **Access Token + Refresh Token** pair:
- **Access Token**: Short-lived (15 minutes). Used to authenticate API requests via `Authorization: Bearer <token>` header.
- **Refresh Token**: Long-lived (30 days). Used to obtain a new Access Token when the current one expires.

## Authentication Flow

### 1. Login

**Endpoint:** `POST /auth/login`

**Request:**
```json
{
  "email": "user@example.com",
  "password": "yourpassword"
}
```

**Response:**
```json
{
  "token": "eyJhbG...",          // Access Token
  "refresh_token": "eyJhbG...",  // Refresh Token (Save this securely!)
  "user": { ... },
  "expires_at": "2024-01-01T12:15:00Z"
}
```

**Action:**
- Save `token` in memory or secure storage.
- Save `refresh_token` in **Secure Storage** (Keychain on iOS, Keystore/EncryptedSharedPreferences on Android).

### 2. Making Authenticated Requests

Include the Access Token in the header:
`Authorization: Bearer <access_token>`

### 3. Handling Token Expiration (401 Unauthorized)

When an API call returns `401 Unauthorized`, your app should automatically attempt to refresh the token.

**Logic:**
1.  Intercept the `401` error.
2.  Check if you have a stored `refresh_token`.
3.  Call the Refresh Endpoint.

**Endpoint:** `POST /auth/refresh`

**Request:**
```json
{
  "refresh_token": "YOUR_STORED_REFRESH_TOKEN"
}
```

**Response (Success - 200 OK):**
```json
{
  "token": "NEW_ACCESS_TOKEN",
  "refresh_token": "NEW_REFRESH_TOKEN" // OPTIONAL: The server MAY rotate the refresh token
}
```

**Action:**
- Update your stored `token`.
- **Important:** If a `refresh_token` is returned in the response, **replace your old refresh token** with the new one. We implement **Refresh Token Rotation**, so the old one becomes invalid.
- Retry the original failed API request with the new `token`.

**Response (Failure - 401/403):**
- The refresh token is invalid or expired.
- **Action:** Log the user out and redirect to the Login screen. Delete stored tokens.

## Code Example (Pseudo-code)

```javascript
// Axios-like interceptor example

api.interceptors.response.use(
  response => response,
  async error => {
    const originalRequest = error.config;

    if (error.response.status === 401 && !originalRequest._retry) {
      originalRequest._retry = true;
      
      const refreshToken = await SecureStorage.getItem('refresh_token');
      
      if (!refreshToken) {
         // No refresh token, logout
         return Promise.reject(error);
      }

      try {
        const res = await api.post('/auth/refresh', { refresh_token: refreshToken });
        
        if (res.status === 200) {
           const { token, refresh_token: newRefreshToken } = res.data;
           
           await SecureStorage.setItem('token', token);
           if (newRefreshToken) {
             await SecureStorage.setItem('refresh_token', newRefreshToken);
           }
           
           api.defaults.headers.common['Authorization'] = 'Bearer ' + token;
           originalRequest.headers['Authorization'] = 'Bearer ' + token;
           
           return api(originalRequest);
        }
      } catch (refreshErr) {
         // Refresh failed (e.g. token revoked/expired)
         await performLogout();
         return Promise.reject(refreshErr);
      }
    }
    return Promise.reject(error);
  }
);
```

## Security Best Practices

1.  **Secure Storage:** NEVER store the Refresh Token in plain text (e.g., standard SharedPreferences or UserDefaults). Use the platform's secure storage mechanisms.
2.  **Token Rotation:** Always check if a new refresh token is returned and update it.
3.  **Logout:** On explicit logout, ensure you delete both tokens from the device.
4.  **SSL/TLS:** Always use HTTPS.
