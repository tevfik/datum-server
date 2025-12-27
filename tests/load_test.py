"""
Datumpy Load Test

Performance testing using Locust.

Run:
    pip install locust
    locust -f tests/load_test.py --host=http://localhost:8007

Then open http://localhost:8089 in browser.
"""

from locust import HttpUser, task, between
import random
import string

class PublicDataUser(HttpUser):
    """Simulates public devices sending data without auth"""
    wait_time = between(0.1, 0.5)
    
    def on_start(self):
        self.device_id = "loadtest_" + "".join(random.choices(string.ascii_lowercase, k=8))
    
    @task(10)
    def send_data(self):
        """Send sensor data to public endpoint"""
        self.client.post(
            f"/public/data/{self.device_id}",
            json={
                "temperature": 20 + random.random() * 10,
                "humidity": 40 + random.random() * 30,
            },
            headers={"Content-Type": "application/json"}
        )
    
    @task(3)
    def read_latest(self):
        """Read latest data point"""
        self.client.get(f"/public/data/{self.device_id}")
    
    @task(1)
    def read_history(self):
        """Read data history"""
        self.client.get(f"/public/data/{self.device_id}/history?limit=10")


class AuthenticatedDeviceUser(HttpUser):
    """Simulates authenticated devices (more realistic)"""
    wait_time = between(1, 3)
    
    def on_start(self):
        # Register user
        email = f"loadtest_{random.randint(1000000, 9999999)}@test.com"
        resp = self.client.post(
            "/auth/register",
            json={"email": email, "password": "testpass123"}
        )
        if resp.status_code == 201:
            self.token = resp.json().get("token")
            
            # Create device
            resp = self.client.post(
                "/devices",
                json={"name": "Load Test Device", "type": "sensor"},
                headers={"Authorization": f"Bearer {self.token}"}
            )
            if resp.status_code == 201:
                self.device_id = resp.json().get("device_id")
                self.api_key = resp.json().get("api_key")
            else:
                self.device_id = None
                self.api_key = None
        else:
            self.token = None
            self.device_id = None
            self.api_key = None
    
    @task(10)
    def send_authenticated_data(self):
        """Send data with API key auth"""
        if not self.api_key:
            return
        
        self.client.post(
            f"/data/{self.device_id}",
            json={
                "temperature": 20 + random.random() * 10,
                "humidity": 40 + random.random() * 30,
                "battery": 70 + random.random() * 30,
            },
            headers={
                "Authorization": f"Bearer {self.api_key}",
                "Content-Type": "application/json"
            }
        )
    
    @task(2)
    def read_authenticated_data(self):
        """Read data with user auth"""
        if not self.token or not self.device_id:
            return
        
        self.client.get(
            f"/data/{self.device_id}",
            headers={"Authorization": f"Bearer {self.token}"}
        )
    
    @task(1)
    def poll_commands(self):
        """Poll for commands (simulates real device behavior)"""
        if not self.api_key:
            return
        
        self.client.get(
            f"/device/{self.device_id}/commands",
            headers={"Authorization": f"Bearer {self.api_key}"}
        )


class AnalyticsUser(HttpUser):
    """Simulates dashboard querying analytics"""
    wait_time = between(2, 5)
    
    @task
    def query_analytics(self):
        """Query public analytics endpoint"""
        self.client.get("/", name="/health")
