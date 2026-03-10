#!/bin/bash

# Basic Integration Test
# Tests server startup and basic health check

set -e

echo "🧪 Starting Integration Tests..."

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

pass_test() {
    echo -e "${GREEN}✓${NC} $1"
}

fail_test() {
    echo -e "${RED}✗${NC} $1"
    exit 1
}

info() {
    echo -e "${YELLOW}→${NC} $1"
}

# Configuration
SERVER_BIN=${SERVER_BINARY:-"./build/binaries/server"}
CLI_BIN=${CLI_BINARY:-"./build/binaries/datumctl"}
SERVER_PORT=8000
SERVER_URL="http://127.0.0.1:${SERVER_PORT}"

# Unset proxies to prevent interference
unset HTTP_PROXY HTTPS_PROXY http_proxy https_proxy all_proxy NO_PROXY no_proxy


# Ensure absolute paths
SERVER_BIN=$(readlink -f "$SERVER_BIN" || echo "$SERVER_BIN")
CLI_BIN=$(readlink -f "$CLI_BIN" || echo "$CLI_BIN")

# Check if binaries exist
info "Checking binaries..."
if [ ! -f "$SERVER_BIN" ]; then
    fail_test "Server binary not found at $SERVER_BIN"
fi

if [ ! -f "$CLI_BIN" ]; then
    fail_test "CLI binary not found at $CLI_BIN"
fi
pass_test "Binaries found"

# Test datumctl version
info "Testing datumctl..."
$CLI_BIN version > /dev/null 2>&1 || fail_test "datumctl version failed"
pass_test "datumctl works"

# Setup workspace
WORK_DIR=$(mktemp -d)
info "Created temporary workspace at $WORK_DIR"
cd "$WORK_DIR" || fail_test "Failed to enter workspace"

# Cleanup trap
cleanup() {
    exit_code=$?
    info "Cleaning up..."
    if [ $exit_code -ne 0 ]; then
        echo "================ SERVER LOG (STDOUT) ================"
        cat server.log || echo "No server log found"
        echo "====================================================="
        echo "================ SERVER LOG (FILE) ================"
        cat data/server.log || echo "No data/server.log found"
        echo "==================================================="
    fi

    if [ -n "$SERVER_PID" ]; then
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
    rm -rf "$WORK_DIR"
    
    if [ $exit_code -eq 0 ]; then
        echo -e "${GREEN}✓ All integration tests passed!${NC}"
    else
        echo -e "${RED}✗ Tests failed!${NC}"
    fi
    exit $exit_code
}
trap cleanup EXIT

# Check if port is in use
if curl -sf "${SERVER_URL}/health" > /dev/null 2>&1; then
    fail_test "Port $SERVER_PORT is already in use. Please stop the existing server."
fi

# Start server
info "Starting server..."
# Copy binary to workspace to ensure CWD relative paths work (like data/) or just run from there
mkdir -p data
# We run the binary from the workspace so it creates data/ in the workspace
"$SERVER_BIN" > server.log 2>&1 &
SERVER_PID=$!

# Wait for server to start
info "Waiting for server to start (PID: $SERVER_PID)..."
for i in {1..30}; do
    if ! kill -0 "$SERVER_PID" 2>/dev/null; then
        echo "Server log output:"
        cat server.log
        fail_test "Server process died unexpectedly"
    fi
    
    if curl -sf "${SERVER_URL}/health" > /dev/null 2>&1; then
        pass_test "Server is healthy"
        break
    fi
    
    if [ $i -eq 30 ]; then
        echo "Server log output:"
        cat server.log
        fail_test "Server failed to start within timeout"
    fi
    sleep 0.5
done

# Run basic CLI operations against the server

# Extract token/login
# For CLI tool, it saves token to ~/.datumctl.yaml by default. 
# We should override config location to stay isolated.
export DATUMCTL_CONFIG="${WORK_DIR}/datumctl.yaml"
touch "$DATUMCTL_CONFIG"

# 1. Setup
info "Running first-time setup..."
$CLI_BIN --server "$SERVER_URL" setup \
    --platform "Integration Test" \
    --email "admin@example.com" \
    --password "admin123" \
    --retention 7 \
    --yes --json --verbose
    
if grep -q "token" "$DATUMCTL_CONFIG"; then
    pass_test "Setup completed"
else
    echo "DEBUG: Config content:"
    cat "$DATUMCTL_CONFIG"
    fail_test "Setup failed - token not found in config"
fi

info "Logging in..."
$CLI_BIN --server "$SERVER_URL" login \
    --email "admin@example.com" \
    --password "admin123" > /dev/null

# 2. Status
info "Checking status..."
STATUS=$($CLI_BIN --server "$SERVER_URL" status)
if echo "$STATUS" | grep -q "healthy"; then
    pass_test "Status check passed"
else
    echo "$STATUS"
    fail_test "Status check failed"
fi

# 3. Create Device
info "Creating device..."
DEVICE_OUT=$($CLI_BIN --server "$SERVER_URL" device create --name "TestDevice" --type "integration" --json)
# Extract device_id (support both id and device_id for compatibility)
DEVICE_ID=$(echo "$DEVICE_OUT" | grep -o '"device_id": *"[^"]*"' | cut -d'"' -f4)
if [ -z "$DEVICE_ID" ]; then
    DEVICE_ID=$(echo "$DEVICE_OUT" | grep -o '"id": *"[^"]*"' | cut -d'"' -f4)
fi

if [ -n "$DEVICE_ID" ]; then
    pass_test "Device created: $DEVICE_ID"
else
    echo "$DEVICE_OUT"
    fail_test "Device creation failed"
fi

# 4. List Devices
info "Listing devices..."
LIST_OUT=$($CLI_BIN --server "$SERVER_URL" device list --json)
if echo "$LIST_OUT" | grep -q "$DEVICE_ID"; then
    pass_test "Device listed successfully"
else
    echo "$LIST_OUT"
    fail_test "Device not found in list"
fi

info "Integration tests completed successfully"
