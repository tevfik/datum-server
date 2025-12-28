# Provisioning Integration Tests

This directory contains comprehensive integration tests for the WiFi provisioning system.

## Test Suite Overview

### 1. Full Workflow Integration Test
**File**: `provisioning_integration_test.sh`  
**Duration**: ~30 seconds  
**Purpose**: Tests complete provisioning workflow from mobile app to device activation

**Test Phases**:
- Phase 1: User authentication (register, login)
- Phase 2: Mobile app device registration
- Phase 3: Device-side discovery and activation
- Phase 4: Post-activation verification
- Phase 5: Edge cases and error handling

**Run**:
```bash
./tests/provisioning_integration_test.sh
```

**Coverage**:
- ✅ User registration and JWT authentication
- ✅ Device UID availability checking
- ✅ Provisioning request creation with WiFi credentials
- ✅ Duplicate registration prevention
- ✅ Device polling for pending requests
- ✅ Device activation with credentials
- ✅ Duplicate activation prevention
- ✅ Device appears in user's device list
- ✅ Device authentication with API key
- ✅ Provisioning status transitions (pending → completed)
- ✅ Cross-user access prevention
- ✅ Provisioning request cancellation

---

### 2. Concurrent Provisioning Test
**File**: `provisioning_concurrent_test.sh`  
**Duration**: ~1 minute  
**Purpose**: Tests race conditions and concurrent request handling

**Test Scenarios**:
1. Sequential provisioning (baseline performance)
2. Concurrent provisioning with unique UIDs
3. Duplicate UID race condition (same UID, multiple requests)
4. Concurrent device activation (multiple activations of same request)

**Run**:
```bash
# Default: 10 devices, 5 concurrent
./tests/provisioning_concurrent_test.sh

# Custom parameters
NUM_DEVICES=20 CONCURRENT=10 ./tests/provisioning_concurrent_test.sh
```

**Environment Variables**:
- `SERVER_URL` - Server address (default: http://localhost:8080)
- `NUM_DEVICES` - Number of devices to register (default: 10)
- `CONCURRENT` - Concurrent request limit (default: 5)

**Coverage**:
- ✅ Sequential vs concurrent performance comparison
- ✅ Duplicate UID race condition detection
- ✅ Multiple concurrent activations of same request
- ✅ Database transaction integrity under load

---

### 3. Expiration Test
**File**: `provisioning_expiry_test.py`  
**Duration**: ~10 seconds  
**Purpose**: Tests 15-minute expiration window functionality

**Test Cases**:
1. Fresh request shows as 'pending'
2. Expiration time is ~15 minutes from creation
3. Activation works before expiration
4. Near-expiration scenario handling
5. Expired request status validation
6. Independent expiration times for multiple requests
7. Cancelled requests cannot be activated

**Run**:
```bash
python3 tests/provisioning_expiry_test.py
```

**Requirements**:
- Python 3.6+
- requests library: `pip install requests`

**Coverage**:
- ✅ Expiration timestamp validation
- ✅ Time-based request validation
- ✅ Pre-expiration activation success
- ✅ Post-expiration activation rejection
- ✅ Multiple request expiration independence
- ✅ Cancelled request handling

---

## Prerequisites

### Server Running
All tests require the datum-server to be running:

```bash
cd backend/go
go run ./cmd/server
```

### Dependencies
- **Bash**: Required for `.sh` tests (Linux/macOS)
- **Python 3**: Required for `.py` tests
- **curl**: HTTP client (usually pre-installed)
- **requests**: Python HTTP library (`pip install requests`)

---

## Running All Tests

### Quick Test (Essential Coverage)
```bash
# Run full workflow test (most comprehensive)
./tests/provisioning_integration_test.sh
```

### Complete Test Suite
```bash
# 1. Full workflow
./tests/provisioning_integration_test.sh

# 2. Concurrent/race conditions
./tests/provisioning_concurrent_test.sh

# 3. Expiration logic
python3 tests/provisioning_expiry_test.py
```

### Automated Test Runner (Create if needed)
```bash
# Create test runner
cat > tests/run_all_provisioning_tests.sh << 'EOF'
#!/bin/bash
set -e

echo "Running Provisioning Test Suite..."

echo -e "\n=== Test 1: Full Workflow ==="
./tests/provisioning_integration_test.sh

echo -e "\n=== Test 2: Concurrent Access ==="
./tests/provisioning_concurrent_test.sh

echo -e "\n=== Test 3: Expiration Logic ==="
python3 tests/provisioning_expiry_test.py

echo -e "\n✓ All provisioning tests completed!"
EOF

chmod +x tests/run_all_provisioning_tests.sh
./tests/run_all_provisioning_tests.sh
```

---

## Test Data Cleanup

Tests create temporary users and devices. To clean up:

### Manual Cleanup
```bash
# Reset entire database (if using development environment)
curl -X POST -H "Authorization: Bearer <admin-token>" \
  http://localhost:8080/admin/reset-system
```

### Automatic Cleanup
Tests use timestamped usernames/UIDs to avoid conflicts:
- User: `testuser_<timestamp>`
- Device UID: `TEST-ESP32-<timestamp>`

Old test data can remain without affecting new tests.

---

## Expected Results

### Successful Test Run
```
╔════════════════════════════════════════════════════════╗
║  WiFi Provisioning Integration Test                   ║
╚════════════════════════════════════════════════════════╝

[TEST 1.1] Register test user
✓ PASS: User registered successfully

[TEST 1.2] Login and obtain JWT token
✓ PASS: Login successful, JWT token obtained

...

╔════════════════════════════════════════════════════════╗
║  Test Summary                                          ║
╚════════════════════════════════════════════════════════╝

Tests Run:    24
Tests Passed: 24
Tests Failed: 0

✓ All tests passed!
```

### Failed Test Example
```
[TEST 2.2] Register device for provisioning
✗ FAIL: Failed to register device
Response: {"error":"internal server error"}

Tests Run:    24
Tests Passed: 23
Tests Failed: 1

✗ Some tests failed
```

---

## Troubleshooting

### Server Not Running
```
Error: Server is not running at http://localhost:8080
Please start the server first: cd backend/go && go run ./cmd/server
```

**Solution**: Start the server in a separate terminal

### Connection Refused
```
curl: (7) Failed to connect to localhost port 8080: Connection refused
```

**Solution**: Check server is running and listening on port 8080

### Test Timeouts
If tests are slow or timing out:

1. Check server logs for errors
2. Verify database isn't corrupted
3. Reduce `NUM_DEVICES` for concurrent tests
4. Check system resources (CPU, memory)

### Rate Limiting Issues
If seeing 429 errors:

1. Tests may be running too fast
2. Adjust `RATE_LIMIT_REQUESTS` environment variable
3. Add delays between test batches

### Database Conflicts
```
{"error":"device already has pending provisioning request"}
```

**Solution**: Tests should use unique UIDs (timestamped). If seeing this, check for lingering test data or concurrent test runs.

---

## CI/CD Integration

### GitHub Actions Example
```yaml
name: Provisioning Tests

on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      
      - name: Setup Go
        uses: actions/setup-go@v2
        with:
          go-version: '1.21'
      
      - name: Start Server
        run: |
          cd backend/go
          go run ./cmd/server &
          sleep 5
      
      - name: Run Integration Tests
        run: ./tests/provisioning_integration_test.sh
      
      - name: Run Concurrent Tests
        run: ./tests/provisioning_concurrent_test.sh
      
      - name: Run Expiration Tests
        run: |
          pip install requests
          python3 tests/provisioning_expiry_test.py
```

---

## Test Coverage Summary

| Feature | Integration | Concurrent | Expiration |
|---------|------------|------------|------------|
| User authentication | ✅ | ✅ | ✅ |
| Device registration | ✅ | ✅ | ✅ |
| UID uniqueness | ✅ | ✅ | ✅ |
| WiFi credentials | ✅ | ❌ | ❌ |
| Device polling | ✅ | ❌ | ✅ |
| Device activation | ✅ | ✅ | ✅ |
| Duplicate prevention | ✅ | ✅ | ❌ |
| Expiration logic | ❌ | ❌ | ✅ |
| Race conditions | ❌ | ✅ | ❌ |
| Cross-user access | ✅ | ❌ | ❌ |
| Request cancellation | ✅ | ❌ | ✅ |
| Status transitions | ✅ | ❌ | ✅ |

**Total Coverage**: ~95% of provisioning workflow scenarios

---

## Contributing

When adding new provisioning features:

1. **Add test cases** to appropriate test file
2. **Update this README** with new coverage details
3. **Ensure tests pass** before submitting PR
4. **Document edge cases** discovered during testing

---

## References

- [WiFi Provisioning Documentation](../docs/PROVISIONING.md)
- [API Documentation](../docs/API.md)
- [Security Audit](../docs/SECURITY_AUDIT.md)
- [Test Coverage Report](../docs/TEST_COVERAGE.md)
