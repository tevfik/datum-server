# CI/CD & DevOps Roadmap

## 1. GitHub Actions Pipeline
Automate testing and building on every Push/PR.

### Workflow Steps
1.  **Checkout**: Get code.
2.  **Linting**: Run `golangci-lint` to check style/errors.
3.  **Unit Tests**: Run `go test ./...` with coverage report.
4.  **Integration Tests**:
    - Spin up Postgres/Redis services via Docker Compose.
    - Run API integration tests.
5.  **Build**:
    - Compile `server` and `datumctl` for Linux/AMD64, Linux/ARM64 (RPi), Windows, macOS.

## 2. Docker Automation
1.  **Multi-Arch Build**: Use Docker Buildx to push Multi-Architecture images (AMD64/ARM64).
2.  **Tagging**:
    - `latest` -> Latest commit on main.
    - `vX.Y.Z` -> On Git Tag.

## 3. Infrastructure as Code (IaC)
1.  **Terraform/Ansible**: Scripts to deploy `datum-server` on VPS (DigitalOcean/AWS).
2.  **Monitoring Stack**: Pre-configured Prometheus + Grafana dashboard for Server Metrics.
