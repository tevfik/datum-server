#!/bin/bash
# Integration Test: WiFi Provisioning Workflow
# Tests the complete provisioning flow from mobile app to device activation

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
SERVER_URL="${SERVER_URL:-http://localhost:8000}"
DEVICE_UID="TEST-ESP32-$(date +%s)"
DEVICE_NAME="Test Device Integration"
WIFI_SSID="TestNetwork"
WIFI_PASS="SecurePassword123"

# Test counters
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

# Cleanup function
cleanup() {
    echo -e "\n${YELLOW}Cleaning up test data...${NC}"
    if [ -n "$TOKEN" ] && [ -n "$REQUEST_ID" ]; then
        curl -s -X DELETE \
            -H "Authorization: Bearer $TOKEN" \
            "$SERVER_URL/devices/provisioning/$REQUEST_ID" > /dev/null 2>&1 || true
    fi
}

trap cleanup EXIT

# Helper functions
print_test() {
    echo -e "\n${BLUE}[TEST $1]${NC} $2"
    TESTS_RUN=$((TESTS_RUN + 1))
}

print_pass() {
    echo -e "${GREEN}✓ PASS${NC}: $1"
    TESTS_PASSED=$((TESTS_PASSED + 1))
}

print_fail() {
    echo -e "${RED}✗ FAIL${NC}: $1"
    TESTS_FAILED=$((TESTS_FAILED + 1))
}

print_info() {
    echo -e "${YELLOW}ℹ${NC} $1"
}

# Check if server is running
print_info "Checking server status at $SERVER_URL..."
if ! curl -s "$SERVER_URL/status" > /dev/null; then
    echo -e "${RED}Error: Server is not running at $SERVER_URL${NC}"
    echo "Please start the server first: cd backend/go && go run ./cmd/server"
    exit 1
fi
print_pass "Server is running"

echo -e "\n${BLUE}╔════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║  WiFi Provisioning Integration Test                   ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════════════════╝${NC}"

# ============================================================================
# PHASE 1: User Registration & Authentication
# ============================================================================
echo -e "\n${YELLOW}═══ PHASE 1: User Authentication ═══${NC}"

print_test "1.1" "Register test user"
TIMESTAMP=$(date +%s)
TEST_USER="testuser_$TIMESTAMP"
TEST_EMAIL="test_$TIMESTAMP@example.com"
TEST_PASS="TestPass123!"

REGISTER_RESPONSE=$(curl -s -X POST \
    -H "Content-Type: application/json" \
    -d "{
        \"username\": \"$TEST_USER\",
        \"email\": \"$TEST_EMAIL\",
        \"password\": \"$TEST_PASS\"
    }" \
    "$SERVER_URL/auth/register")

if echo "$REGISTER_RESPONSE" | grep -q "user_id"; then
    USER_ID=$(echo "$REGISTER_RESPONSE" | grep -o '"user_id":"[^"]*' | cut -d'"' -f4)
    print_pass "User registered successfully (ID: $USER_ID)"
else
    print_fail "Failed to register user"
    echo "Response: $REGISTER_RESPONSE"
    exit 1
fi

print_test "1.2" "Login and obtain JWT token"
LOGIN_RESPONSE=$(curl -s -X POST \
    -H "Content-Type: application/json" \
    -d "{
        \"username\": \"$TEST_USER\",
        \"password\": \"$TEST_PASS\"
    }" \
    "$SERVER_URL/auth/login")

if echo "$LOGIN_RESPONSE" | grep -q "token"; then
    TOKEN=$(echo "$LOGIN_RESPONSE" | grep -o '"token":"[^"]*' | cut -d'"' -f4)
    print_pass "Login successful, JWT token obtained"
else
    print_fail "Failed to login"
    echo "Response: $LOGIN_RESPONSE"
    exit 1
fi

# ============================================================================
# PHASE 2: Mobile App - Device Registration
# ============================================================================
echo -e "\n${YELLOW}═══ PHASE 2: Mobile App - Device Registration ═══${NC}"

print_test "2.1" "Check device UID availability"
CHECK_RESPONSE=$(curl -s -X POST \
    -H "Content-Type: application/json" \
    -H "Authorization: Bearer $TOKEN" \
    -d "{\"device_uid\": \"$DEVICE_UID\"}" \
    "$SERVER_URL/devices/check")

if echo "$CHECK_RESPONSE" | grep -q '"registered":false'; then
    print_pass "Device UID is available"
else
    print_fail "Device UID check failed or device already exists"
    echo "Response: $CHECK_RESPONSE"
fi

print_test "2.2" "Register device for provisioning (mobile app)"
REGISTER_DEV_RESPONSE=$(curl -s -X POST \
    -H "Content-Type: application/json" \
    -H "Authorization: Bearer $TOKEN" \
    -d "{
        \"device_uid\": \"$DEVICE_UID\",
        \"device_name\": \"$DEVICE_NAME\",
        \"device_type\": \"sensor\",
        \"wifi_ssid\": \"$WIFI_SSID\",
        \"wifi_pass\": \"$WIFI_PASS\"
    }" \
    "$SERVER_URL/devices/register")

if echo "$REGISTER_DEV_RESPONSE" | grep -q "request_id"; then
    REQUEST_ID=$(echo "$REGISTER_DEV_RESPONSE" | grep -o '"request_id":"[^"]*' | cut -d'"' -f4)
    DEVICE_ID=$(echo "$REGISTER_DEV_RESPONSE" | grep -o '"device_id":"[^"]*' | cut -d'"' -f4)
    API_KEY=$(echo "$REGISTER_DEV_RESPONSE" | grep -o '"api_key":"[^"]*' | cut -d'"' -f4)
    WIFI_SSID_RESP=$(echo "$REGISTER_DEV_RESPONSE" | grep -o '"wifi_ssid":"[^"]*' | cut -d'"' -f4)
    WIFI_PASS_RESP=$(echo "$REGISTER_DEV_RESPONSE" | grep -o '"wifi_pass":"[^"]*' | cut -d'"' -f4)
    
    print_pass "Device registered for provisioning"
    print_info "Request ID: $REQUEST_ID"
    print_info "Device ID: $DEVICE_ID"
    print_info "WiFi SSID: $WIFI_SSID_RESP"
    
    # Verify WiFi credentials match
    if [ "$WIFI_SSID_RESP" = "$WIFI_SSID" ] && [ "$WIFI_PASS_RESP" = "$WIFI_PASS" ]; then
        print_pass "WiFi credentials correctly included in response"
    else
        print_fail "WiFi credentials mismatch"
    fi
else
    print_fail "Failed to register device"
    echo "Response: $REGISTER_DEV_RESPONSE"
    exit 1
fi

print_test "2.3" "List user's provisioning requests"
LIST_RESPONSE=$(curl -s -X GET \
    -H "Authorization: Bearer $TOKEN" \
    "$SERVER_URL/devices/provisioning")

if echo "$LIST_RESPONSE" | grep -q "$REQUEST_ID"; then
    print_pass "Provisioning request appears in user's list"
    REQUEST_COUNT=$(echo "$LIST_RESPONSE" | grep -o '"request_id"' | wc -l)
    print_info "Total provisioning requests: $REQUEST_COUNT"
else
    print_fail "Provisioning request not found in list"
    echo "Response: $LIST_RESPONSE"
fi

print_test "2.4" "Get specific provisioning request status"
STATUS_RESPONSE=$(curl -s -X GET \
    -H "Authorization: Bearer $TOKEN" \
    "$SERVER_URL/devices/provisioning/$REQUEST_ID")

if echo "$STATUS_RESPONSE" | grep -q '"status":"pending"'; then
    print_pass "Provisioning request status is 'pending'"
else
    print_fail "Failed to get provisioning status or wrong status"
    echo "Response: $STATUS_RESPONSE"
fi

print_test "2.5" "Verify duplicate registration prevention"
DUPLICATE_RESPONSE=$(curl -s -X POST \
    -H "Content-Type: application/json" \
    -H "Authorization: Bearer $TOKEN" \
    -d "{
        \"device_uid\": \"$DEVICE_UID\",
        \"device_name\": \"Duplicate Device\",
        \"device_type\": \"sensor\"
    }" \
    "$SERVER_URL/devices/register")

if echo "$DUPLICATE_RESPONSE" | grep -q "already has pending"; then
    print_pass "Duplicate registration correctly prevented"
else
    print_fail "Duplicate registration not properly blocked"
    echo "Response: $DUPLICATE_RESPONSE"
fi

# ============================================================================
# PHASE 3: Device Side - Check & Activate
# ============================================================================
echo -e "\n${YELLOW}═══ PHASE 3: Device Side - Discovery & Activation ═══${NC}"

print_test "3.1" "Device checks for provisioning request (polling)"
CHECK_DEV_RESPONSE=$(curl -s -X GET \
    "$SERVER_URL/provisioning/check/$DEVICE_UID")

if echo "$CHECK_DEV_RESPONSE" | grep -q '"status":"pending"'; then
    print_pass "Device found pending provisioning request"
    ACTIVATE_URL=$(echo "$CHECK_DEV_RESPONSE" | grep -o '"activate_url":"[^"]*' | cut -d'"' -f4)
    print_info "Activate URL: $ACTIVATE_URL"
else
    print_fail "Device check failed"
    echo "Response: $CHECK_DEV_RESPONSE"
fi

print_test "3.2" "Device activates using provisioning request"
ACTIVATE_RESPONSE=$(curl -s -X POST \
    -H "Content-Type: application/json" \
    -d "{
        \"device_uid\": \"$DEVICE_UID\",
        \"firmware_version\": \"1.0.0\",
        \"model\": \"ESP32-DevKit\"
    }" \
    "$SERVER_URL/provisioning/activate/$REQUEST_ID")

if echo "$ACTIVATE_RESPONSE" | grep -q '"device_id"'; then
    ACTIVATED_DEVICE_ID=$(echo "$ACTIVATE_RESPONSE" | grep -o '"device_id":"[^"]*' | cut -d'"' -f4)
    ACTIVATED_API_KEY=$(echo "$ACTIVATE_RESPONSE" | grep -o '"api_key":"[^"]*' | cut -d'"' -f4)
    ACTIVATED_WIFI_SSID=$(echo "$ACTIVATE_RESPONSE" | grep -o '"wifi_ssid":"[^"]*' | cut -d'"' -f4)
    ACTIVATED_WIFI_PASS=$(echo "$ACTIVATE_RESPONSE" | grep -o '"wifi_pass":"[^"]*' | cut -d'"' -f4)
    
    print_pass "Device activated successfully"
    print_info "Device ID: $ACTIVATED_DEVICE_ID"
    print_info "WiFi SSID: $ACTIVATED_WIFI_SSID"
    
    # Verify credentials match
    if [ "$ACTIVATED_DEVICE_ID" = "$DEVICE_ID" ] && [ "$ACTIVATED_API_KEY" = "$API_KEY" ]; then
        print_pass "Device credentials match provisioning request"
    else
        print_fail "Device credentials mismatch"
        echo "Expected Device ID: $DEVICE_ID, Got: $ACTIVATED_DEVICE_ID"
    fi
    
    # Verify WiFi credentials
    if [ "$ACTIVATED_WIFI_SSID" = "$WIFI_SSID" ] && [ "$ACTIVATED_WIFI_PASS" = "$WIFI_PASS" ]; then
        print_pass "WiFi credentials correctly provided to device"
    else
        print_fail "WiFi credentials not provided correctly"
    fi
else
    print_fail "Device activation failed"
    echo "Response: $ACTIVATE_RESPONSE"
    exit 1
fi

print_test "3.3" "Verify device cannot activate twice"
REACTIVATE_RESPONSE=$(curl -s -X POST \
    -H "Content-Type: application/json" \
    -d "{
        \"device_uid\": \"$DEVICE_UID\",
        \"firmware_version\": \"1.0.0\"
    }" \
    "$SERVER_URL/provisioning/activate/$REQUEST_ID")

if echo "$REACTIVATE_RESPONSE" | grep -q "already registered\|not pending\|completed"; then
    print_pass "Duplicate activation correctly prevented"
else
    print_fail "Duplicate activation not properly blocked"
    echo "Response: $REACTIVATE_RESPONSE"
fi

# ============================================================================
# PHASE 4: Post-Activation Verification
# ============================================================================
echo -e "\n${YELLOW}═══ PHASE 4: Post-Activation Verification ═══${NC}"

print_test "4.1" "Verify device appears in user's device list"
DEVICES_RESPONSE=$(curl -s -X GET \
    -H "Authorization: Bearer $TOKEN" \
    "$SERVER_URL/devices")

if echo "$DEVICES_RESPONSE" | grep -q "$ACTIVATED_DEVICE_ID"; then
    print_pass "Device appears in user's device list"
else
    print_fail "Device not found in user's device list"
    echo "Response: $DEVICES_RESPONSE"
fi

print_test "4.2" "Verify device can authenticate with API key"
DATA_RESPONSE=$(curl -s -X POST \
    -H "Content-Type: application/json" \
    -H "X-API-Key: $ACTIVATED_API_KEY" \
    -d "{
        \"device_id\": \"$ACTIVATED_DEVICE_ID\",
        \"data\": {\"temperature\": 25.5, \"humidity\": 60}
    }" \
    "$SERVER_URL/data")

if echo "$DATA_RESPONSE" | grep -q "success\|stored\|ok"; then
    print_pass "Device successfully authenticated and sent data"
else
    print_fail "Device authentication with API key failed"
    echo "Response: $DATA_RESPONSE"
fi

print_test "4.3" "Verify provisioning request status changed to 'completed'"
FINAL_STATUS=$(curl -s -X GET \
    -H "Authorization: Bearer $TOKEN" \
    "$SERVER_URL/devices/provisioning/$REQUEST_ID")

if echo "$FINAL_STATUS" | grep -q '"status":"completed"'; then
    print_pass "Provisioning request status updated to 'completed'"
else
    print_fail "Provisioning request status not updated correctly"
    echo "Response: $FINAL_STATUS"
fi

print_test "4.4" "Verify new device cannot be registered with same UID"
NEW_REG_RESPONSE=$(curl -s -X POST \
    -H "Content-Type: application/json" \
    -H "Authorization: Bearer $TOKEN" \
    -d "{
        \"device_uid\": \"$DEVICE_UID\",
        \"device_name\": \"Another Device\",
        \"device_type\": \"sensor\"
    }" \
    "$SERVER_URL/devices/register")

if echo "$NEW_REG_RESPONSE" | grep -q "already registered"; then
    print_pass "Duplicate UID registration correctly prevented after activation"
else
    print_fail "Duplicate UID registration not blocked"
    echo "Response: $NEW_REG_RESPONSE"
fi

# ============================================================================
# PHASE 5: Edge Cases & Error Handling
# ============================================================================
echo -e "\n${YELLOW}═══ PHASE 5: Edge Cases & Error Handling ═══${NC}"

print_test "5.1" "Test activation with non-existent request ID"
NONEXIST_RESPONSE=$(curl -s -X POST \
    -H "Content-Type: application/json" \
    -d "{\"device_uid\": \"NONEXISTENT\"}" \
    "$SERVER_URL/provisioning/activate/prov_nonexistent")

if echo "$NONEXIST_RESPONSE" | grep -q "not found"; then
    print_pass "Non-existent request ID correctly rejected"
else
    print_fail "Non-existent request ID handling failed"
    echo "Response: $NONEXIST_RESPONSE"
fi

print_test "5.2" "Test device check with non-registered UID"
NONREG_CHECK=$(curl -s -X GET \
    "$SERVER_URL/provisioning/check/NONREGISTERED-UID")

if echo "$NONREG_CHECK" | grep -q "not found\|unconfigured"; then
    print_pass "Non-registered UID check handled correctly"
else
    print_fail "Non-registered UID check failed"
    echo "Response: $NONREG_CHECK"
fi

print_test "5.3" "Test unauthorized access to other user's request"
# Create second user
SECOND_USER="testuser2_$TIMESTAMP"
SECOND_EMAIL="test2_$TIMESTAMP@example.com"
SECOND_REGISTER=$(curl -s -X POST \
    -H "Content-Type: application/json" \
    -d "{
        \"username\": \"$SECOND_USER\",
        \"email\": \"$SECOND_EMAIL\",
        \"password\": \"$TEST_PASS\"
    }" \
    "$SERVER_URL/auth/register")

SECOND_LOGIN=$(curl -s -X POST \
    -H "Content-Type: application/json" \
    -d "{
        \"username\": \"$SECOND_USER\",
        \"password\": \"$TEST_PASS\"
    }" \
    "$SERVER_URL/auth/login")

SECOND_TOKEN=$(echo "$SECOND_LOGIN" | grep -o '"token":"[^"]*' | cut -d'"' -f4)

UNAUTH_ACCESS=$(curl -s -X GET \
    -H "Authorization: Bearer $SECOND_TOKEN" \
    "$SERVER_URL/devices/provisioning/$REQUEST_ID")

if echo "$UNAUTH_ACCESS" | grep -q "not authorized\|forbidden"; then
    print_pass "Cross-user access correctly forbidden"
else
    print_fail "Cross-user access not properly blocked"
    echo "Response: $UNAUTH_ACCESS"
fi

print_test "5.4" "Test cancel provisioning request"
# Create new provisioning request to cancel
NEW_UID="CANCEL-TEST-$(date +%s)"
NEW_REG=$(curl -s -X POST \
    -H "Content-Type: application/json" \
    -H "Authorization: Bearer $TOKEN" \
    -d "{
        \"device_uid\": \"$NEW_UID\",
        \"device_name\": \"Cancel Test Device\"
    }" \
    "$SERVER_URL/devices/register")

NEW_REQUEST_ID=$(echo "$NEW_REG" | grep -o '"request_id":"[^"]*' | cut -d'"' -f4)

CANCEL_RESPONSE=$(curl -s -X DELETE \
    -H "Authorization: Bearer $TOKEN" \
    "$SERVER_URL/devices/provisioning/$NEW_REQUEST_ID")

if echo "$CANCEL_RESPONSE" | grep -q "cancelled"; then
    print_pass "Provisioning request cancelled successfully"
else
    print_fail "Failed to cancel provisioning request"
    echo "Response: $CANCEL_RESPONSE"
fi

# ============================================================================
# Test Summary
# ============================================================================
echo -e "\n${BLUE}╔════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║  Test Summary                                          ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════════════════╝${NC}"

echo -e "\nTests Run:    $TESTS_RUN"
echo -e "${GREEN}Tests Passed: $TESTS_PASSED${NC}"
echo -e "${RED}Tests Failed: $TESTS_FAILED${NC}"

if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "\n${GREEN}✓ All tests passed!${NC}"
    exit 0
else
    echo -e "\n${RED}✗ Some tests failed${NC}"
    exit 1
fi
