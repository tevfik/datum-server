# Security Policy

## Reporting a Vulnerability

If you discover a security vulnerability in Datum, please report it responsibly:

**DO NOT** open a public GitHub issue for security vulnerabilities.

Instead, please email security concerns to: security@datum.io (or your organization's security contact)

Include the following information:
- Description of the vulnerability
- Steps to reproduce
- Potential impact
- Suggested fix (if any)

We will respond within 48 hours and work with you to understand and resolve the issue.

## Supported Versions

| Version | Supported          |
| ------- | ------------------ |
| 1.5.x   | :white_check_mark: |
| 1.0.x   | :white_check_mark: |
| < 1.0   | :x:                |

## Security Best Practices

### Production Deployment

1. **Change Default Secrets**
   ```bash
   # Copy docker/.env.example to .env and update all secrets
   cp docker/.env.example .env
   # Generate strong JWT secret (min 32 characters)
   openssl rand -base64 32
   ```

2. **Use HTTPS/TLS**
   - Deploy behind a reverse proxy (nginx, Traefik, Caddy)
   - Enable HTTPS with valid SSL certificates
   - Enable HSTS by setting `ENABLE_HSTS=true` in your environment

3. **Database Security**
   - Keep data directory outside web root
   - Regular backups (use `make db-backup`)
   - Restrict file permissions: `chmod 700 data/`

4. **Rate Limiting**
   - Adjust rate limits based on your needs
   - Monitor for abuse patterns
   - Consider additional WAF protection

5. **Network Security**
   - Use firewall rules to restrict access
   - Keep services on internal network
   - Use VPN for remote access

### Authentication

- Default JWT expiration: 24 hours
- Passwords hashed with bcrypt
- Minimum password length: 8 characters
- **Device Authentication**:
  - **[Planned]** Hybrid SAS Token Rotation (90-day expiry)
  - 7-day grace period
  - Emergency revocation support

### Known Limitations

- CORS is set to allow all origins by default (change in production)
- No built-in IP whitelisting (use firewall rules)
- Admin endpoints require manual security setup

### Security Headers

The following security headers are automatically set:
- `X-Frame-Options: DENY`
- `X-Content-Type-Options: nosniff`
- `X-XSS-Protection: 1; mode=block`
- `Referrer-Policy: strict-origin-when-cross-origin`
- `Content-Security-Policy: default-src 'self'`

### Dependencies

Keep dependencies up to date:
```bash
# Go dependencies
go get -u && go mod tidy
```

### Audit Log

- **Provisioning**: Comprehensive audit logs for device registration/activation.
- **System**: Structured JSON logs via Zerolog.
- **Planned**: Admin action auditing (future release).

### Data Privacy

- Time-series data retention is configurable
- User data is isolated per account
- No telemetry or external data sharing

## Security Checklist for Production

- [ ] Changed all default secrets in `.env`
- [ ] Enabled HTTPS/TLS
- [ ] Configured proper CORS origins
- [ ] Set up regular backups
- [ ] Restricted data directory permissions
- [ ] Configured rate limits appropriately
- [ ] Updated all dependencies
- [ ] Set up monitoring and alerting
- [ ] Reviewed and restricted admin access
- [ ] Configured firewall rules
- [ ] Disabled debug mode

## Contact

For security concerns: See [SECURITY.yml](../../.github/SECURITY.md) or use GitHub Security Advisories

For general issues: GitHub Issues
