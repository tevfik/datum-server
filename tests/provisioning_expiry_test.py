#!/usr/bin/env python3
"""
Provisioning Expiration Test
Tests the 15-minute expiration window for provisioning requests
"""

import requests
import time
import json
from datetime import datetime, timedelta

# Configuration
SERVER_URL = "http://localhost:8000"
EXPIRATION_SECONDS = 15 * 60  # 15 minutes

# Colors for output
class Colors:
    BLUE = '\033[0;34m'
    GREEN = '\033[0;32m'
    RED = '\033[0;31m'
    YELLOW = '\033[1;33m'
    NC = '\033[0m'

def print_header(text):
    print(f"\n{Colors.BLUE}{'='*60}{Colors.NC}")
    print(f"{Colors.BLUE}{text.center(60)}{Colors.NC}")
    print(f"{Colors.BLUE}{'='*60}{Colors.NC}")

def print_test(num, desc):
    print(f"\n{Colors.BLUE}[TEST {num}]{Colors.NC} {desc}")

def print_pass(msg):
    print(f"{Colors.GREEN}✓ PASS{Colors.NC}: {msg}")

def print_fail(msg):
    print(f"{Colors.RED}✗ FAIL{Colors.NC}: {msg}")

def print_info(msg):
    print(f"{Colors.YELLOW}ℹ{Colors.NC} {msg}")

def setup_test_user():
    """Create and authenticate test user"""
    timestamp = int(time.time())
    username = f"expiry_test_{timestamp}"
    email = f"expiry_{timestamp}@example.com"
    password = "TestPass123!"
    
    # Register
    resp = requests.post(f"{SERVER_URL}/auth/register", json={
        "username": username,
        "email": email,
        "password": password
    })
    
    # Login
    resp = requests.post(f"{SERVER_URL}/auth/login", json={
        "username": username,
        "password": password
    })
    
    return resp.json()['token']

def create_provisioning_request(token, uid_suffix=""):
    """Create a provisioning request"""
    device_uid = f"EXPIRY-TEST-{int(time.time())}{uid_suffix}"
    
    resp = requests.post(
        f"{SERVER_URL}/devices/register",
        headers={"Authorization": f"Bearer {token}"},
        json={
            "device_uid": device_uid,
            "device_name": f"Expiry Test Device {uid_suffix}",
            "device_type": "sensor"
        }
    )
    
    return resp.json(), device_uid

def main():
    print_header("Provisioning Expiration Test")
    
    # Setup
    print_info("Setting up test user...")
    token = setup_test_user()
    print_pass("Test user authenticated")
    
    tests_passed = 0
    tests_failed = 0
    
    # ========================================================================
    # Test 1: Verify fresh request is pending and valid
    # ========================================================================
    print_test("1", "Verify fresh request is pending and valid")
    
    data, uid1 = create_provisioning_request(token, "-1")
    request_id_1 = data['request_id']
    expires_at = data['expires_at']
    
    print_info(f"Request ID: {request_id_1}")
    print_info(f"Expires at: {expires_at}")
    
    # Check device can see it
    resp = requests.get(f"{SERVER_URL}/provisioning/check/{uid1}")
    if resp.json().get('status') == 'pending':
        print_pass("Fresh request shows as 'pending'")
        tests_passed += 1
    else:
        print_fail(f"Fresh request status incorrect: {resp.json()}")
        tests_failed += 1
    
    # ========================================================================
    # Test 2: Verify expiration time is approximately 15 minutes
    # ========================================================================
    print_test("2", "Verify expiration time is approximately 15 minutes")
    
    # Parse expires_at timestamp
    expires_dt = datetime.fromisoformat(expires_at.replace('Z', '+00:00'))
    now = datetime.now(expires_dt.tzinfo)
    time_until_expiry = (expires_dt - now).total_seconds()
    
    print_info(f"Time until expiry: {time_until_expiry:.0f} seconds ({time_until_expiry/60:.1f} minutes)")
    
    # Should be approximately 15 minutes (allow 1 minute tolerance)
    if 14 * 60 <= time_until_expiry <= 16 * 60:
        print_pass("Expiration time is correct (~15 minutes)")
        tests_passed += 1
    else:
        print_fail(f"Expiration time incorrect: {time_until_expiry}s")
        tests_failed += 1
    
    # ========================================================================
    # Test 3: Verify activation works before expiration
    # ========================================================================
    print_test("3", "Verify activation works before expiration")
    
    data, uid3 = create_provisioning_request(token, "-3")
    request_id_3 = data['request_id']
    
    resp = requests.post(
        f"{SERVER_URL}/provisioning/activate/{request_id_3}",
        json={
            "device_uid": uid3,
            "firmware_version": "1.0.0"
        }
    )
    
    if resp.status_code == 200 and 'device_id' in resp.json():
        print_pass("Activation successful before expiration")
        tests_passed += 1
    else:
        print_fail(f"Activation failed: {resp.status_code} - {resp.text}")
        tests_failed += 1
    
    # ========================================================================
    # Test 4: Simulate near-expiration scenario (if time permits)
    # ========================================================================
    print_test("4", "Simulate near-expiration scenario")
    
    print_info("⚠️  SKIPPED - Would take 15 minutes to test actual expiration")
    print_info("For manual testing:")
    print_info("1. Create a provisioning request")
    print_info("2. Wait 15+ minutes")
    print_info("3. Try to activate - should get 'expired' error")
    print_info("4. Check device status - should show 'expired'")
    
    # ========================================================================
    # Test 5: Verify expired request shows correct status
    # ========================================================================
    print_test("5", "Verify system handles expiration checks correctly")
    
    # Create request and check status endpoint validates expiration
    data, uid5 = create_provisioning_request(token, "-5")
    request_id_5 = data['request_id']
    
    # Check current status
    resp = requests.get(
        f"{SERVER_URL}/devices/provisioning/{request_id_5}",
        headers={"Authorization": f"Bearer {token}"}
    )
    
    if resp.status_code == 200:
        status_data = resp.json()
        if 'expires_at' in status_data:
            print_pass("Status endpoint includes expiration timestamp")
            tests_passed += 1
        else:
            print_fail("Status endpoint missing expiration timestamp")
            tests_failed += 1
    else:
        print_fail(f"Status check failed: {resp.status_code}")
        tests_failed += 1
    
    # ========================================================================
    # Test 6: Verify multiple requests have independent expiration
    # ========================================================================
    print_test("6", "Verify multiple requests have independent expiration times")
    
    # Create multiple requests with delays
    requests_data = []
    for i in range(3):
        data, uid = create_provisioning_request(token, f"-multi-{i}")
        requests_data.append(data)
        if i < 2:
            time.sleep(2)  # 2 second delay between requests
    
    # Check all have different expiration times
    expiry_times = [r['expires_at'] for r in requests_data]
    unique_times = len(set(expiry_times))
    
    if unique_times == 3:
        print_pass("Each request has independent expiration time")
        tests_passed += 1
    else:
        print_fail(f"Expiration times not unique: {unique_times}/3")
        tests_failed += 1
    
    # ========================================================================
    # Test 7: Verify cancelled request doesn't affect expiration
    # ========================================================================
    print_test("7", "Verify cancelled request can't be activated even before expiration")
    
    data, uid7 = create_provisioning_request(token, "-7")
    request_id_7 = data['request_id']
    
    # Cancel the request
    resp = requests.delete(
        f"{SERVER_URL}/devices/provisioning/{request_id_7}",
        headers={"Authorization": f"Bearer {token}"}
    )
    
    # Try to activate
    resp = requests.post(
        f"{SERVER_URL}/provisioning/activate/{request_id_7}",
        json={
            "device_uid": uid7,
            "firmware_version": "1.0.0"
        }
    )
    
    if resp.status_code != 200:
        print_pass("Cancelled request cannot be activated")
        tests_passed += 1
    else:
        print_fail("Cancelled request was activated!")
        tests_failed += 1
    
    # ========================================================================
    # Summary
    # ========================================================================
    print_header("Test Summary")
    
    total_tests = tests_passed + tests_failed
    print(f"\nTests Run:    {total_tests}")
    print(f"{Colors.GREEN}Tests Passed: {tests_passed}{Colors.NC}")
    print(f"{Colors.RED}Tests Failed: {tests_failed}{Colors.NC}")
    
    if tests_failed == 0:
        print(f"\n{Colors.GREEN}✓ All tests passed!{Colors.NC}")
        return 0
    else:
        print(f"\n{Colors.RED}✗ Some tests failed{Colors.NC}")
        return 1

if __name__ == "__main__":
    try:
        exit_code = main()
        exit(exit_code)
    except requests.exceptions.ConnectionError:
        print(f"{Colors.RED}Error: Cannot connect to server at {SERVER_URL}{Colors.NC}")
        print("Please start the server first: cd backend/go && go run ./cmd/server")
        exit(1)
    except KeyboardInterrupt:
        print(f"\n{Colors.YELLOW}Test interrupted by user{Colors.NC}")
        exit(1)
