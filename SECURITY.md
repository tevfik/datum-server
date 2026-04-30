# Security Policy

## Reporting a Vulnerability
If you discover a security vulnerability in `datum-server`, please report it **privately**:

1. **Do NOT open a public GitHub issue** for security findings.
2. Email the maintainer directly at the address listed on the GitHub profile of [@tevfik](https://github.com/tevfik).
3. Include a detailed description of the vulnerability, steps to reproduce, and potential impact.

We will acknowledge receipt within **48 hours** and aim to issue a fix within **7 days** for critical issues.

## Security Considerations
`datum-server` is designed as a robust backend for IoT and AI-based conformity assessment applications. Key security notes:

- **Authentication**: All API endpoints (except device boot activation) require a valid JWT or API Key (`ak_...` / `sk_...`). 
- **MQTT Broker**: The built-in MQTT broker enforces strict ACLs. Devices can only read/write to their own topics (`dev/{device_id}/*`). By default, unencrypted ports (1883, 1884) are disabled unless explicitly opted-in via the `MQTT_ALLOW_INSECURE=true` environment variable.
- **Storage Isolation**: Object storage operations are transparently isolated per-user (`user_id-bucket`) to prevent Broken Object Level Authorization (BOLA).
- **MCP Server**: The Model Context Protocol server integrates with AI agents. It automatically sanitizes raw internal backend errors (e.g., database connection errors) before returning them to the agent to prevent information disclosure.

## Threat Model
| Threat | Mitigation |
|--------|------------|
| **Device Takeover (BOLA)** | The provisioning flow enforces strict ownership checks; a device cannot be re-registered by an attacker without ownership validation. |
| **Object Storage BOLA** | The backend API dynamically forces a tenant-specific prefix on all storage operations. |
| **MQTT Identity Spoofing** | ClientIDs are strictly verified against the underlying owner of the provided API key during the MQTT connection handshake. |
| **JWT Algorithm Confusion** | The authentication middleware explicitly rejects any token that does not use the strong `SigningMethodHMAC` (HS256) algorithm. |
| **DoS via In-Memory DB** | Heavy aggregation queries (like listing all collections) use lightweight tracking indexes rather than scanning all memory keys. |

## Supported Versions
| Version | Supported |
|---------|-----------|
| Latest `main` | Yes — security fixes applied immediately |
| Latest tagged release | Yes — backported patch fixes |
| < 1.0 | Best effort |
