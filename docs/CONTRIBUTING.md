# Contributing to Datum IoT Platform

Thank you for your interest in contributing to Datum! This document provides guidelines and instructions for contributing to the project.

## 🤝 Code of Conduct

- Be respectful and inclusive
- Welcome newcomers and help them get started
- Focus on constructive feedback
- Respect differing viewpoints and experiences

## 🐛 Reporting Bugs

Before creating bug reports, please check existing issues. When creating a bug report, include:

**Required Information:**
- Clear, descriptive title
- Steps to reproduce the issue
- Expected behavior vs actual behavior
- Environment details (OS, Go version, Docker version if applicable)
- Relevant logs or error messages
- Screenshots if applicable

**Example:**
```markdown
## Bug: Data ingestion fails with large payloads

**Environment:**
- OS: Ubuntu 22.04
- Go Version: 1.21.5
- Server Version: 1.0.0

**Steps to Reproduce:**
1. Send POST request to `/dev/data` with 10MB payload
2. Server returns 413 error

**Expected:** Data should be accepted up to 16MB
**Actual:** Request rejected at 10MB

**Logs:**
[error] payload size exceeds limit
```

## ✨ Suggesting Features

Feature suggestions are welcome! Please:

1. Check existing feature requests first
2. Provide clear use case and rationale
3. Describe proposed API/interface changes
4. Consider backward compatibility
5. Discuss implementation approach if possible

## 🔧 Development Setup

### Prerequisites

- Go 1.21 or higher
- Git
- Make (optional but recommended)
- Docker and Docker Compose (for integration tests)

### Setup Steps

```bash
# 1. Fork and clone
git clone https://github.com/YOUR_USERNAME/datum-server.git
cd datum-server

# 2. Install dependencies
go mod download

# 3. Run tests
make test

# 4. Run server locally
make run

# 5. Verify installation
curl http://localhost:8000/health
```

### Development Workflow

```bash
# Create feature branch
git checkout -b feature/my-new-feature

# Make changes and test frequently
go test ./...

# Format code
make fmt

# Run linters
make lint

# Build
make build
```

## 📝 Coding Standards

### Go Style Guide

Follow official [Go Code Review Comments](https://github.com/golang/go/wiki/CodeReviewComments) and these project-specific guidelines:

**Naming Conventions:**
- Use descriptive names: `getUserByID` not `getUsr`
- Interfaces: `StorageProvider` not `IStorage`
- Test files: `module_test.go` or `module_feature_test.go`
- No "coverage", "boost", "wins" in test file names

**Code Organization:**
```go
// Package declaration and imports
package storage

import (
    "context"
    "errors"
    
    "datum-go/internal/types"
)

// Constants and variables
const (
    DefaultTimeout = 30 * time.Second
)

// Types
type Storage struct {
    db *buntdb.DB
}

// Constructor
func New(path string) (*Storage, error) {
    // implementation
}

// Methods (receiver, name, parameters, returns)
func (s *Storage) GetDevice(ctx context.Context, id string) (*types.Device, error) {
    // implementation
}
```

**Error Handling:**
```go
// Good: Descriptive errors with context
if err != nil {
    return fmt.Errorf("failed to create device %s: %w", deviceID, err)
}

// Bad: Generic error without context
if err != nil {
    return err
}
```

**Comments:**
```go
// Good: Explains why, not what
// Use exponential backoff to avoid overwhelming the database
// during recovery from connection failures
func (s *Storage) retryOperation() error {

// Bad: States the obvious
// This function retries an operation
func (s *Storage) retryOperation() error {
```

### Package Structure

```
datum-server/
├── cmd/
│   ├── server/          # HTTP server and handlers
│   │   ├── main.go
│   │   ├── admin.go
│   │   ├── handlers_*.go
│   │   └── *_test.go
│   └── datumctl/        # CLI tool
│       └── main.go
├── internal/
│   ├── auth/            # Authentication & authorization
│   ├── storage/         # Data storage layer
│   └── logger/          # Logging utilities
├── docs/                # Documentation
└── tests/               # Integration and load tests
```

## 🧪 Testing Requirements

### Test Coverage

- **Minimum 60% coverage** for new packages
- **Handler tests** for all new HTTP endpoints
- **Integration tests** for critical workflows
- **Edge cases** and error scenarios

### Writing Tests

**Test File Naming:**
```
handlers_data_test.go      ✅ Good: Descriptive, clear scope
handlers_admin_extended_test.go  ✅ Good: Extended tests for admin
critical_boost_test.go     ❌ Bad: Vague, uses "boost"
coverage_test.go           ❌ Bad: Too generic
```

**Test Function Naming:**
```go
// Good: Descriptive test names
func TestGetDataHistoryHandlerWithRFC3339Times(t *testing.T)
func TestCreateUserHandlerDuplicateEmail(t *testing.T)
func TestRateLimiterExceedsLimit(t *testing.T)

// Bad: Vague or generic names
func TestHandler1(t *testing.T)
func TestError(t *testing.T)
func TestSuccess(t *testing.T)
```

**Test Structure:**
```go
func TestCreateDevice(t *testing.T) {
    // Setup
    store, cleanup := setupTestStorage(t)
    defer cleanup()
    
    user := createTestUser(t, store)
    
    // Execute
    device, err := store.CreateDevice(&storage.Device{
        UserID: user.ID,
        Name:   "Test Device",
        Type:   "sensor",
    })
    
    // Assert
    require.NoError(t, err)
    assert.NotEmpty(t, device.ID)
    assert.NotEmpty(t, device.APIKey)
    assert.Equal(t, "Test Device", device.Name)
    
    // Verify persistence
    retrieved, err := store.GetDevice(device.ID)
    require.NoError(t, err)
    assert.Equal(t, device.ID, retrieved.ID)
}
```

**Run Tests:**
```bash
# All tests
go test ./...

# Specific package
go test ./internal/storage

# With coverage
go test -cover ./...

# Verbose output
go test -v ./cmd/server

# Specific test
go test -v ./cmd/server -run TestCreateDevice

# Benchmarks
go test -bench=. ./internal/storage
```

## 🔄 Pull Request Process

### Before Submitting

- [ ] Tests pass: `go test ./...`
- [ ] Code formatted: `make fmt`
- [ ] Linters pass: `make lint`
- [ ] Documentation updated (if applicable)
- [ ] CHANGELOG.md updated (if user-facing change)
- [ ] Commit messages follow conventions

### Commit Message Format

```
type(scope): brief description

Detailed explanation of the change, including:
- Why the change is needed
- What was changed
- Any breaking changes or migration notes

Fixes #123
```

**Types:**
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation changes
- `test`: Adding or updating tests
- `refactor`: Code refactoring
- `perf`: Performance improvements
- `chore`: Maintenance tasks

**Examples:**
```
feat(storage): add batch insert support for time-series data

Implements batch insertion to improve write performance by reducing
individual database transactions. Benchmark shows 3x improvement.

Closes #45

---

fix(auth): prevent race condition in rate limiter

The token bucket implementation had a race condition when multiple
goroutines accessed the same user's rate limit counter. Added mutex
protection to ensure thread-safety.

Fixes #67

---

docs(api): add examples for data aggregation endpoints

Added curl examples and response samples for all aggregation types
(avg, sum, min, max) to improve API documentation clarity.
```

### PR Checklist

When submitting a PR, ensure:

1. **Title**: Clear, descriptive (`feat: add webhook support` not `updates`)
2. **Description**: 
   - What changed and why
   - Link to related issues
   - Screenshots/recordings if UI changes
   - Breaking changes clearly marked
3. **Size**: Keep PRs focused and reasonably sized
4. **Tests**: Include tests for new functionality
5. **Docs**: Update relevant documentation
6. **Review**: Address review comments promptly

### PR Template

```markdown
## Description
Brief description of what this PR does.

## Type of Change
- [ ] Bug fix
- [ ] New feature
- [ ] Breaking change
- [ ] Documentation update

## Related Issues
Fixes #(issue number)

## Testing
- [ ] All tests pass
- [ ] Added new tests
- [ ] Manual testing completed

## Documentation
- [ ] Updated API docs
- [ ] Updated README
- [ ] Updated code comments

## Screenshots (if applicable)
[Add screenshots here]

## Checklist
- [ ] Code follows project style guidelines
- [ ] Self-reviewed my own code
- [ ] Commented complex logic
- [ ] Updated documentation
- [ ] No new warnings generated
- [ ] Added tests that prove fix/feature works
- [ ] New and existing tests pass
```

## 📚 Documentation

### When to Update Documentation

Update documentation when you:
- Add new features or APIs
- Change existing behavior
- Add configuration options
- Modify command-line flags
- Change deployment procedures

### Documentation Structure

- **README.md**: Quick start and overview
- **docs/guides/**: Step-by-step guides
- **docs/tutorials/**: Hands-on tutorials
- **docs/reference/**: Technical reference
- **Code comments**: Explain complex logic

## 🏗️ Architecture Guidelines

### Adding New Handlers

```go
// 1. Define in appropriate file (admin.go, main.go, etc.)
func newFeatureHandler(c *gin.Context) {
    // Implement handler
}

// 2. Add tests in handlers_<module>_test.go
func TestNewFeatureHandler(t *testing.T) {
    // Test implementation
}

// 3. Update OpenAPI spec (openapi.yaml)
// 4. Update docs/reference/API.md
```

### Adding Storage Features

```go
// 1. Add method to storage interface
type Storage interface {
    // Existing methods...
    NewMethod(params) (result, error)
}

// 2. Implement in storage.go
func (s *Storage) NewMethod(params) (result, error) {
    // Implementation
}

// 3. Add tests in storage_test.go
func TestNewMethod(t *testing.T) {
    // Test cases
}
```

## 🚀 Release Process

1. **Version bump**: Update version in relevant files
2. **CHANGELOG**: Document all changes
3. **Testing**: Full test suite + manual verification
4. **Tag**: `git tag -a v1.0.0 -m "Release v1.0.0"`
5. **Push**: `git push origin v1.0.0`
6. **Build**: Create release binaries
7. **Announce**: Update documentation and notify users

## 🎯 Good First Issues

New contributors should look for issues labeled:
- `good first issue` - Easy to start with
- `documentation` - Improve docs
- `testing` - Add test coverage
- `help wanted` - Need assistance

## 💬 Communication

- **GitHub Issues**: Bug reports and feature requests
- **GitHub Discussions**: General questions and ideas
- **Pull Requests**: Code review and discussion

## 📖 Additional Resources

- [Go Documentation](https://go.dev/doc/)
- [Effective Go](https://go.dev/doc/effective_go)
- [Go Code Review Comments](https://github.com/golang/go/wiki/CodeReviewComments)
- [Gin Framework Documentation](https://gin-gonic.com/docs/)

## ❓ Questions?

If you have questions:
1. Check existing documentation
2. Search closed issues
3. Open a discussion on GitHub
4. Ask in pull request if code-related

---

Thank you for contributing to Datum IoT Platform! 🙏
