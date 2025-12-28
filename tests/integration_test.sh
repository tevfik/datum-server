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

# Check if binaries exist
info "Checking binaries..."
if [ ! -f "./datum-server" ]; then
    fail_test "datum-server binary not found"
fi

if [ ! -f "./datumctl" ]; then
    fail_test "datumctl binary not found"
fi
pass_test "Binaries found"

# Test datumctl version
info "Testing datumctl..."
./datumctl version > /dev/null 2>&1 || fail_test "datumctl version failed"
pass_test "datumctl works"

# Create data directory for server
info "Creating data directory..."
mkdir -p ./data
pass_test "Data directory created"

# Start server in background
info "Starting server..."
./datum-server > server.log 2>&1 &
SERVER_PID=$!

# Wait for server to start
sleep 3

# Check if server is still running
if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo "Server logs:"
    cat server.log
    fail_test "Server failed to start"
fi
pass_test "Server started (PID: $SERVER_PID)"

# Test health endpoint
info "Testing health endpoint..."
for i in {1..10}; do
    if curl -sf http://localhost:8000/health > /dev/null 2>&1; then
        pass_test "Health endpoint responding"
        break
    fi
    if [ $i -eq 10 ]; then
        kill $SERVER_PID 2>/dev/null || true
        cat server.log
        fail_test "Health endpoint not responding after 10 attempts"
    fi
    sleep 1
done

# Cleanup
info "Cleaning up..."
kill $SERVER_PID 2>/dev/null || true
wait $SERVER_PID 2>/dev/null || true
rm -f server.log
rm -rf ./data

echo ""
echo -e "${GREEN}✓ All integration tests passed!${NC}"
