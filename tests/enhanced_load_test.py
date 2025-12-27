"""
Enhanced Datumpy Load Test with Detailed Metrics

Performance testing to determine backend capacity and bottlenecks.

Installation:
    pip install locust psutil

Run Load Test:
    # Quick test (100 devices, 30 seconds)
    locust -f tests/enhanced_load_test.py --host=http://localhost:8007 \
           --users 100 --spawn-rate 10 --run-time 30s --headless

    # Medium test (500 devices, 2 minutes)
    locust -f tests/enhanced_load_test.py --host=http://localhost:8007 \
           --users 500 --spawn-rate 25 --run-time 2m --headless

    # Heavy test (1000 devices, 5 minutes)
    locust -f tests/enhanced_load_test.py --host=http://localhost:8007 \
           --users 1000 --spawn-rate 50 --run-time 5m --headless

    # Interactive mode (with web UI)
    locust -f tests/enhanced_load_test.py --host=http://localhost:8007

    Then open http://localhost:8089 in browser
"""

from locust import HttpUser, task, between, events
import random
import string
import time
import psutil
import json
from datetime import datetime

# Global metrics
request_times = []
error_count = 0
success_count = 0


@events.test_start.add_listener
def on_test_start(environment, **kwargs):
    """Record test start metrics"""
    print("\n" + "="*80)
    print("🚀 LOAD TEST STARTING")
    print("="*80)
    print(f"Start Time: {datetime.now()}")
    print(f"Target: {environment.host}")
    
    # System metrics
    cpu_percent = psutil.cpu_percent(interval=1)
    memory = psutil.virtual_memory()
    
    print(f"\n📊 Initial System Metrics:")
    print(f"  CPU Usage: {cpu_percent}%")
    print(f"  Memory Usage: {memory.percent}%")
    print(f"  Available Memory: {memory.available / (1024**3):.2f} GB")
    print("="*80 + "\n")


@events.test_stop.add_listener
def on_test_stop(environment, **kwargs):
    """Print final metrics and analysis"""
    stats = environment.stats.total
    
    print("\n" + "="*80)
    print("📈 LOAD TEST RESULTS")
    print("="*80)
    print(f"End Time: {datetime.now()}")
    
    print(f"\n📊 Request Statistics:")
    print(f"  Total Requests: {stats.num_requests}")
    print(f"  Successful: {stats.num_requests - stats.num_failures}")
    print(f"  Failed: {stats.num_failures}")
    print(f"  Failure Rate: {(stats.num_failures / stats.num_requests * 100) if stats.num_requests > 0 else 0:.2f}%")
    
    print(f"\n⏱️  Response Time Statistics:")
    print(f"  Average: {stats.avg_response_time:.0f} ms")
    print(f"  Min: {stats.min_response_time:.0f} ms")
    print(f"  Max: {stats.max_response_time:.0f} ms")
    print(f"  Median: {stats.median_response_time:.0f} ms")
    print(f"  95th Percentile: {stats.get_response_time_percentile(0.95):.0f} ms")
    print(f"  99th Percentile: {stats.get_response_time_percentile(0.99):.0f} ms")
    
    print(f"\n🔥 Throughput:")
    print(f"  Requests/sec: {stats.total_rps:.2f}")
    print(f"  Failures/sec: {stats.total_fail_per_sec:.2f}")
    
    # System metrics
    cpu_percent = psutil.cpu_percent(interval=1)
    memory = psutil.virtual_memory()
    
    print(f"\n💻 Final System Metrics:")
    print(f"  CPU Usage: {cpu_percent}%")
    print(f"  Memory Usage: {memory.percent}%")
    print(f"  Available Memory: {memory.available / (1024**3):.2f} GB")
    
    # Analysis
    print(f"\n📋 Performance Analysis:")
    
    avg_response = stats.avg_response_time
    failure_rate = (stats.num_failures / stats.num_requests * 100) if stats.num_requests > 0 else 0
    
    if avg_response < 100 and failure_rate < 1:
        grade = "🟢 EXCELLENT"
        capacity = "Can handle 2000+ concurrent devices"
    elif avg_response < 500 and failure_rate < 5:
        grade = "🟡 GOOD"
        capacity = "Can handle 1000-2000 concurrent devices"
    elif avg_response < 1000 and failure_rate < 10:
        grade = "🟠 ACCEPTABLE"
        capacity = "Can handle 500-1000 concurrent devices"
    else:
        grade = "🔴 NEEDS OPTIMIZATION"
        capacity = "Current capacity < 500 devices"
    
    print(f"  Overall Grade: {grade}")
    print(f"  Estimated Capacity: {capacity}")
    
    if failure_rate > 5:
        print(f"\n⚠️  WARNING: High failure rate detected!")
        print(f"  Recommendations:")
        print(f"    - Check API server logs: docker compose logs api")
        print(f"    - Increase server resources (CPU/Memory)")
        print(f"    - Optimize database queries")
        print(f"    - Implement caching layer")
    
    if avg_response > 500:
        print(f"\n⚠️  WARNING: High response times detected!")
        print(f"  Recommendations:")
        print(f"    - Enable database indexes")
        print(f"    - Optimize time-series data storage")
        print(f"    - Use connection pooling")
        print(f"    - Add Redis cache")
    
    print("="*80 + "\n")


class RealisticIoTDevice(HttpUser):
    """
    Simulates realistic IoT device behavior:
    - Sends data every 30-60 seconds (configurable)
    - Polls for commands occasionally
    - Handles errors gracefully
    """
    wait_time = between(30, 60)  # Realistic interval between data sends
    
    def on_start(self):
        """Initialize device - register and get credentials"""
        # Create unique device identifier
        self.device_id = f"device_{random.randint(100000, 999999)}"
        
        # Register via public endpoint (simulates device auto-provisioning)
        self.api_key = None
        self.authenticated = False
        
        # Try to send initial data to register device
        response = self.client.post(
            f"/public/data/{self.device_id}",
            json={
                "temperature": 20 + random.random() * 10,
                "humidity": 50 + random.random() * 30,
            },
            headers={"Content-Type": "application/json"},
            name="/public/data/[device_id]"
        )
        
        if response.status_code == 200:
            self.authenticated = True
    
    @task(20)
    def send_sensor_data(self):
        """Send sensor data - most common operation"""
        data = {
            "temperature": round(20 + random.random() * 15, 2),
            "humidity": round(40 + random.random() * 40, 2),
            "pressure": round(980 + random.random() * 50, 2),
            "battery": round(70 + random.random() * 30, 2),
        }
        
        self.client.post(
            f"/public/data/{self.device_id}",
            json=data,
            headers={"Content-Type": "application/json"},
            name="/public/data/[device_id] (send)"
        )
    
    @task(5)
    def read_latest_data(self):
        """Device reads its own latest data point"""
        self.client.get(
            f"/public/data/{self.device_id}",
            name="/public/data/[device_id] (read)"
        )
    
    @task(2)
    def poll_commands(self):
        """Device polls for pending commands"""
        self.client.get(
            f"/public/data/{self.device_id}/history?limit=1",
            name="/public/data/[device_id]/history"
        )


class DashboardUser(HttpUser):
    """
    Simulates dashboard/admin users:
    - Views multiple device data
    - Queries analytics
    - Less frequent than device updates
    """
    wait_time = between(5, 15)
    
    def on_start(self):
        """Initialize with some device IDs to query"""
        self.device_ids = [f"device_{random.randint(100000, 999999)}" for _ in range(10)]
    
    @task(10)
    def view_device_data(self):
        """View data from random device"""
        device_id = random.choice(self.device_ids)
        self.client.get(
            f"/public/data/{device_id}",
            name="/public/data/[device_id] (dashboard)"
        )
    
    @task(5)
    def view_device_history(self):
        """View device history"""
        device_id = random.choice(self.device_ids)
        limit = random.choice([10, 50, 100])
        self.client.get(
            f"/public/data/{device_id}/history?limit={limit}",
            name="/public/data/[device_id]/history (dashboard)"
        )
    
    @task(2)
    def check_health(self):
        """Check system health"""
        self.client.get("/", name="/health")


class AdminUser(HttpUser):
    """
    Simulates admin operations:
    - Much less frequent
    - Heavier operations
    """
    wait_time = between(30, 120)
    
    def on_start(self):
        """Admin user doesn't need special setup for public endpoints"""
        self.device_ids = [f"device_{random.randint(100000, 999999)}" for _ in range(5)]
    
    @task(5)
    def view_multiple_devices(self):
        """Admin views multiple devices"""
        for device_id in self.device_ids[:3]:
            self.client.get(
                f"/public/data/{device_id}",
                name="/public/data/[device_id] (admin)"
            )
    
    @task(2)
    def query_large_history(self):
        """Admin queries large history"""
        device_id = random.choice(self.device_ids)
        self.client.get(
            f"/public/data/{device_id}/history?limit=1000",
            name="/public/data/[device_id]/history (admin-large)"
        )
    
    @task(1)
    def system_health_check(self):
        """Admin checks system health"""
        self.client.get("/", name="/health (admin)")


# User distribution (weighted by frequency)
# In production: 80% devices, 15% dashboard users, 5% admins
# For testing: simulate realistic ratios
