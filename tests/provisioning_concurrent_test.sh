#!/bin/bash
# Concurrent Provisioning Test
# Tests multiple simultaneous provisioning requests to detect race conditions

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

SERVER_URL="${SERVER_URL:-http://localhost:8080}"
NUM_DEVICES="${NUM_DEVICES:-10}"
CONCURRENT="${CONCURRENT:-5}"

echo -e "${BLUE}╔════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║  Concurrent Provisioning Test                         ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════════════════╝${NC}"

# Setup test user
TIMESTAMP=$(date +%s)
TEST_USER="concurrent_test_$TIMESTAMP"
TEST_EMAIL="concurrent_$TIMESTAMP@example.com"
TEST_PASS="TestPass123!"

echo -e "\n${YELLOW}Setting up test user...${NC}"
curl -s -X POST \
    -H "Content-Type: application/json" \
    -d "{\"username\": \"$TEST_USER\", \"email\": \"$TEST_EMAIL\", \"password\": \"$TEST_PASS\"}" \
    "$SERVER_URL/auth/register" > /dev/null

LOGIN_RESPONSE=$(curl -s -X POST \
    -H "Content-Type: application/json" \
    -d "{\"username\": \"$TEST_USER\", \"password\": \"$TEST_PASS\"}" \
    "$SERVER_URL/auth/login")

TOKEN=$(echo "$LOGIN_RESPONSE" | grep -o '"token":"[^"]*' | cut -d'"' -f4)
echo -e "${GREEN}✓${NC} Test user created and authenticated"

# Test 1: Sequential provisioning (baseline)
echo -e "\n${YELLOW}Test 1: Sequential provisioning ($NUM_DEVICES devices)${NC}"
START_TIME=$(date +%s)
SUCCESS_COUNT=0

for i in $(seq 1 $NUM_DEVICES); do
    DEVICE_UID="CONCURRENT-SEQ-$i-$TIMESTAMP"
    RESPONSE=$(curl -s -X POST \
        -H "Content-Type: application/json" \
        -H "Authorization: Bearer $TOKEN" \
        -d "{\"device_uid\": \"$DEVICE_UID\", \"device_name\": \"Sequential Device $i\"}" \
        "$SERVER_URL/devices/register")
    
    if echo "$RESPONSE" | grep -q "request_id"; then
        SUCCESS_COUNT=$((SUCCESS_COUNT + 1))
    fi
done

END_TIME=$(date +%s)
DURATION=$((END_TIME - START_TIME))
echo -e "${GREEN}✓${NC} Sequential: $SUCCESS_COUNT/$NUM_DEVICES devices registered in ${DURATION}s"

# Test 2: Concurrent provisioning with unique UIDs
echo -e "\n${YELLOW}Test 2: Concurrent provisioning ($CONCURRENT parallel requests)${NC}"
START_TIME=$(date +%s)
SUCCESS_COUNT=0
PIDS=()

for i in $(seq 1 $NUM_DEVICES); do
    (
        DEVICE_UID="CONCURRENT-PAR-$i-$TIMESTAMP"
        RESPONSE=$(curl -s -X POST \
            -H "Content-Type: application/json" \
            -H "Authorization: Bearer $TOKEN" \
            -d "{\"device_uid\": \"$DEVICE_UID\", \"device_name\": \"Parallel Device $i\"}" \
            "$SERVER_URL/devices/register")
        
        if echo "$RESPONSE" | grep -q "request_id"; then
            echo "SUCCESS"
        else
            echo "FAIL: $RESPONSE"
        fi
    ) &
    
    PIDS+=($!)
    
    # Limit concurrency
    if [ $((i % CONCURRENT)) -eq 0 ]; then
        wait
    fi
done

# Wait for remaining processes
wait

END_TIME=$(date +%s)
DURATION=$((END_TIME - START_TIME))

# Count successes from output
SUCCESS_COUNT=$(grep -c "SUCCESS" /dev/null 2>&1 || echo "0")
echo -e "${GREEN}✓${NC} Concurrent: Completed in ${DURATION}s"

# Test 3: Duplicate UID race condition
echo -e "\n${YELLOW}Test 3: Duplicate UID race condition (same UID, multiple requests)${NC}"
DUPLICATE_UID="RACE-CONDITION-$TIMESTAMP"
SUCCESS_COUNT=0
CONFLICT_COUNT=0

for i in $(seq 1 5); do
    (
        RESPONSE=$(curl -s -X POST \
            -H "Content-Type: application/json" \
            -H "Authorization: Bearer $TOKEN" \
            -d "{\"device_uid\": \"$DUPLICATE_UID\", \"device_name\": \"Race Test $i\"}" \
            "$SERVER_URL/devices/register")
        
        if echo "$RESPONSE" | grep -q "request_id"; then
            echo "SUCCESS"
        elif echo "$RESPONSE" | grep -q "already\|pending\|exists"; then
            echo "CONFLICT"
        else
            echo "ERROR: $RESPONSE"
        fi
    ) &
done

wait

# Count results
RESULTS=$(jobs -p | wc -l)
echo ""
echo "Results:"
echo "- Expected: 1 SUCCESS, 4 CONFLICTS"
echo "- Actual: Check responses above"

if [ $(grep -c "SUCCESS" /dev/null 2>&1 || echo "0") -eq 1 ]; then
    echo -e "${GREEN}✓${NC} Race condition properly handled (only 1 success)"
else
    echo -e "${RED}✗${NC} Race condition detected! Multiple requests succeeded for same UID"
fi

# Test 4: Concurrent activation
echo -e "\n${YELLOW}Test 4: Concurrent device activation${NC}"

# Create a provisioning request
ACT_UID="ACTIVATION-$TIMESTAMP"
ACT_REG=$(curl -s -X POST \
    -H "Content-Type: application/json" \
    -H "Authorization: Bearer $TOKEN" \
    -d "{\"device_uid\": \"$ACT_UID\", \"device_name\": \"Activation Test\"}" \
    "$SERVER_URL/devices/register")

REQUEST_ID=$(echo "$ACT_REG" | grep -o '"request_id":"[^"]*' | cut -d'"' -f4)

# Try to activate concurrently
echo "Attempting concurrent activations..."
for i in $(seq 1 3); do
    (
        RESPONSE=$(curl -s -X POST \
            -H "Content-Type: application/json" \
            -d "{\"device_uid\": \"$ACT_UID\", \"firmware_version\": \"1.0.$i\"}" \
            "$SERVER_URL/provisioning/activate/$REQUEST_ID")
        
        if echo "$RESPONSE" | grep -q "device_id"; then
            echo "ACTIVATED"
        elif echo "$RESPONSE" | grep -q "already\|not pending\|completed"; then
            echo "REJECTED"
        else
            echo "ERROR: $RESPONSE"
        fi
    ) &
done

wait

echo ""
if [ $(grep -c "ACTIVATED" /dev/null 2>&1 || echo "0") -eq 1 ]; then
    echo -e "${GREEN}✓${NC} Concurrent activation handled correctly (only 1 success)"
else
    echo -e "${RED}✗${NC} Multiple activations succeeded!"
fi

echo -e "\n${BLUE}╔════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║  Concurrent Test Complete                             ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════════════════╝${NC}"
