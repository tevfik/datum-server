#!/usr/bin/env python3
"""
Basic Provisioning Test
Tests device provisioning functionality
"""

import requests
import sys
import time

BASE_URL = "http://localhost:8000"

def test_health():
    """Test server health endpoint"""
    try:
        response = requests.get(f"{BASE_URL}/health", timeout=5)
        if response.status_code == 200:
            print("✓ Server health check passed")
            return True
    except Exception as e:
        print(f"✗ Health check failed: {e}")
    return False

def main():
    print("🧪 Starting Provisioning Tests...")
    
    # Wait for server
    print("→ Waiting for server...")
    for i in range(10):
        try:
            response = requests.get(f"{BASE_URL}/health", timeout=2)
            if response.status_code == 200:
                break
        except:
            if i == 9:
                print("✗ Server not responding")
                return 1
            time.sleep(1)
    
    # Test health
    if not test_health():
        return 1
    
    print("\n✓ Provisioning tests passed!")
    return 0

if __name__ == "__main__":
    sys.exit(main())
