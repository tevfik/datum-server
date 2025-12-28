# Datum Server - CI/CD Configuration

This directory contains Gitea Actions workflows for automated testing and deployment.

## Workflows

### test.yml - Main Test Pipeline

Triggered on:
- Push to `main`, `master`, or `develop` branches
- Pull requests to `main` or `master`

**Pipeline stages:**

1. **Lint** - Code quality checks
   - golangci-lint (comprehensive Go linting)
   - gofmt formatting verification
   
2. **Test** - Unit tests with coverage
   - Race condition detection
   - Coverage report generation
   - 50% coverage threshold enforcement
   
3. **Build** - Multi-platform binary compilation
   - Linux AMD64/ARM64
   - macOS AMD64/ARM64 (Darwin)
   - Produces both `datum-server` and `datumctl`
   
4. **Integration** - End-to-end testing
   - Full server startup/shutdown
   - Command execution tests
   - SSE streaming tests
   
5. **Docker** - Container build verification
   - Dockerfile build test
   - Health check validation
   - datumctl availability check
   
6. **Security** - Vulnerability scanning
   - Gosec static analysis
   - govulncheck dependency scanning
   
7. **Summary** - Aggregate results
   - Overall pass/fail status
   - Individual stage results

## Usage

### Running Locally

Simulate CI pipeline locally:

```bash
# Lint
golangci-lint run --timeout=5m ./...
gofmt -s -l .

# Test with coverage
go test -v -race -coverprofile=coverage.out ./...
go tool cover -func=coverage.out

# Build all platforms
GOOS=linux GOARCH=amd64 go build -o build/datum-server-linux-amd64 ./cmd/server
GOOS=linux GOARCH=arm64 go build -o build/datum-server-linux-arm64 ./cmd/server

# Integration tests
cd tests && ./integration_test.sh

# Docker build
cd docker && docker build -f Dockerfile -t datum-server:test ..
```

### Viewing Results

In Gitea:
1. Navigate to repository → Actions
2. Click on workflow run
3. View individual job logs
4. Download artifacts (coverage reports, binaries)

### Artifacts

**Available after successful run:**

- `coverage-report` - HTML coverage visualization
- `binaries-{os}-{arch}` - Compiled binaries for each platform
- `security-report` - Gosec JSON output

## Configuration

### Coverage Threshold

Adjust in [test.yml](test.yml):

```yaml
THRESHOLD=50  # Minimum coverage percentage
```

### Build Platforms

Add/remove platforms in matrix:

```yaml
strategy:
  matrix:
    target:
      - { os: linux, arch: amd64 }
      - { os: windows, arch: amd64 }  # Add Windows
```

### Integration Test Timeout

Tests run with default timeouts. For longer tests:

```yaml
- name: Run integration tests
  timeout-minutes: 10  # Default: 360 (6 hours)
```

## Troubleshooting

### Pipeline Fails on Lint

**Issue:** golangci-lint errors

```bash
# Run locally to see errors
golangci-lint run ./...

# Auto-fix some issues
golangci-lint run --fix ./...
```

### Coverage Below Threshold

**Issue:** Total coverage < 50%

```bash
# Check uncovered code
go test -coverprofile=coverage.out ./...
go tool cover -html=coverage.out

# Add tests for uncovered functions
```

### Integration Tests Fail

**Issue:** Server doesn't start or tests timeout

```bash
# Run integration tests locally
cd tests
./integration_test.sh

# Check server logs
tail -f /tmp/datum-server.log

# Debug mode
export DEBUG=1
./integration_test.sh
```

### Docker Build Fails

**Issue:** Dockerfile errors or missing dependencies

```bash
# Build locally with verbose output
cd docker
docker build --no-cache -f Dockerfile -t datum-server:debug ..

# Check build logs
docker build --progress=plain -f Dockerfile ..
```

### Security Scan Fails

**Issue:** Vulnerabilities detected

```bash
# Run locally
go install github.com/securego/gosec/v2/cmd/gosec@latest
gosec ./...

# Check for dependency vulnerabilities
go install golang.org/x/vuln/cmd/govulncheck@latest
govulncheck ./...

# Update dependencies
go get -u ./...
go mod tidy
```

## Best Practices

### Before Pushing

1. **Run tests locally:**
   ```bash
   go test ./...
   ```

2. **Check formatting:**
   ```bash
   gofmt -s -w .
   ```

3. **Lint code:**
   ```bash
   golangci-lint run ./...
   ```

4. **Build binaries:**
   ```bash
   go build ./cmd/server
   go build ./cmd/datumctl
   ```

### Writing Tests

1. **Add table-driven tests:**
   ```go
   func TestFeature(t *testing.T) {
       tests := []struct {
           name string
           input string
           want string
       }{
           {"case1", "input1", "output1"},
       }
       for _, tt := range tests {
           t.Run(tt.name, func(t *testing.T) {
               // test logic
           })
       }
   }
   ```

2. **Test coverage for new code:**
   ```bash
   go test -cover ./internal/mypackage
   ```

3. **Race detection:**
   ```bash
   go test -race ./...
   ```

### Security

1. **Keep dependencies updated:**
   ```bash
   go get -u ./...
   go mod tidy
   ```

2. **Review security reports:**
   - Download artifacts after each run
   - Check `gosec-report.json`
   - Address high-severity issues

3. **Enable Dependabot (if available):**
   - Automatic dependency updates
   - Security vulnerability alerts

## CI Performance

### Optimization Tips

1. **Cache Go modules:**
   ```yaml
   - name: Cache Go modules
     uses: actions/cache@v3
     with:
       path: ~/go/pkg/mod
       key: ${{ runner.os }}-go-${{ hashFiles('**/go.sum') }}
   ```

2. **Parallel test execution:**
   ```bash
   go test -parallel 4 ./...
   ```

3. **Reduce artifact size:**
   ```yaml
   - name: Build datum-server
     run: |
       go build -ldflags="-s -w" ./cmd/server  # Strip debug info
   ```

## Deployment Pipeline

For production deployment, create [deploy.yml](deploy.yml):

```yaml
name: Deploy to Production

on:
  push:
    tags:
      - 'v*'

jobs:
  deploy:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      
      - name: Deploy via SSH
        run: |
          ssh user@server 'cd /opt/datum-server && \
            git pull && \
            cd docker && \
            docker-compose pull && \
            docker-compose up -d'
```

## Monitoring

### Pipeline Health

Monitor these metrics:
- ✅ Success rate (target: >95%)
- ⏱️ Duration (target: <10 minutes)
- 📊 Coverage trend (maintain >50%)
- 🔒 Security issues (target: 0 high/critical)

### Notifications

Configure Gitea webhook for:
- Slack/Discord notifications
- Email alerts on failures
- Status badges in README

## Resources

- [Gitea Actions Documentation](https://docs.gitea.io/en-us/actions/)
- [golangci-lint Linters](https://golangci-lint.run/usage/linters/)
- [Go Testing Best Practices](https://go.dev/doc/tutorial/add-a-test)
- [Docker Build Best Practices](https://docs.docker.com/build/building/best-practices/)
