#!/bin/bash

# Datumpy Load Test Runner
# Prepares environment and runs Locust load test

set -e

API_URL="${API_URL:-http://localhost:8007}"
ANALYTICS_URL="${ANALYTICS_URL:-http://localhost:8001}"

echo "🚀 Datumpy Load Test Runner"
echo "==========================="
echo ""

# Check if services are running
echo "1. Checking services..."
if ! curl -s "$API_URL/" > /dev/null 2>&1; then
    echo "   ❌ API not reachable at $API_URL"
    echo "   Run: docker-compose up -d"
    exit 1
fi
echo "   ✅ API running at $API_URL"

if curl -s "$ANALYTICS_URL/health" > /dev/null 2>&1; then
    echo "   ✅ Analytics running at $ANALYTICS_URL"
else
    echo "   ⚠️  Analytics not running (optional)"
fi

# Check locust
echo ""
echo "2. Checking Locust..."
if ! command -v locust &> /dev/null; then
    echo "   Installing locust..."
    pip install locust -q
fi
echo "   ✅ Locust installed"

# Quick warmup test
echo ""
echo "3. Running warmup (5 seconds)..."
for i in {1..5}; do
    curl -s -X POST "$API_URL/public/data/warmup-test" \
        -H "Content-Type: application/json" \
        -d "{\"temperature\": $((20 + i)), \"humidity\": 50}" > /dev/null
done
echo "   ✅ Warmup complete"

# Start Locust
echo ""
echo "4. Starting Locust..."
echo ""
echo "   Options:"
echo "   =========================================="
echo "   Press ENTER for Web UI mode (recommended)"
echo "   Or type 'headless' for CLI mode"
echo "   =========================================="
echo ""
read -p "   Mode: " MODE

if [ "$MODE" == "headless" ]; then
    echo ""
    echo "   Running headless test (30 seconds, 10 users)..."
    locust -f tests/load_test.py \
        --host=$API_URL \
        --headless \
        --users 10 \
        --spawn-rate 2 \
        --run-time 30s \
        --only-summary
else
    echo ""
    echo "   Starting Locust Web UI..."
    echo "   Open: http://localhost:8089"
    echo ""
    echo "   Suggested settings:"
    echo "   - Users: 50"
    echo "   - Spawn rate: 5"
    echo "   - Host: $API_URL"
    echo ""
    locust -f tests/load_test.py --host=$API_URL
fi
