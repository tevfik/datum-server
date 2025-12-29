# Deployment Guide

Complete guide for deploying Datum IoT Platform in production environments.

## 📋 Table of Contents

- [Deployment Options](#deployment-options)
- [Docker Deployment](#docker-deployment)
- [Systemd Service](#systemd-service)
- [Kubernetes](#kubernetes)
- [Cloud Platforms](#cloud-platforms)
- [Configuration](#configuration)
- [Security](#security)
- [Monitoring](#monitoring)
- [Backup & Recovery](#backup--recovery)
- [Troubleshooting](#troubleshooting)

## 🚀 Deployment Options

### Option 1: Docker (Recommended)
Best for: Quick setup, development, small to medium deployments

**Pros:**
- Easy setup and updates
- Isolated environment
- Cross-platform support
- Built-in orchestration with Docker Compose

**Cons:**
- Overhead for very small deployments
- Requires Docker knowledge

### Option 2: Systemd Service
Best for: Linux servers, native performance, minimal overhead

**Pros:**
- Native performance
- System integration
- Low resource usage
- Fine-grained control

**Cons:**
- Manual setup required
- Platform-specific
- Update process more involved

### Option 3: Kubernetes
Best for: Large-scale, high-availability deployments

**Pros:**
- Auto-scaling
- High availability
- Rolling updates
- Advanced orchestration

**Cons:**
- Complex setup
- Higher resource requirements
- Requires K8s expertise

## 🐳 Docker Deployment

### Prerequisites

```bash
# Install Docker & Docker Compose
curl -fsSL https://get.docker.com -o get-docker.sh
sudo sh get-docker.sh
```

### Production Deployment (with Traefik)

The default configuration uses Traefik as a reverse proxy for automatic HTTPS and load balancing.

1.  **Clone repository**
    ```bash
    git clone https://github.com/your-org/datum-server.git
    cd datum-server
    ```

2.  **Configure Environment**
    Navigate to the docker directory and configure the environment:
    ```bash
    cd docker
    cp .env.example .env
    nano .env
    ```
    
    **Critical Settings:**
    - `JWT_SECRET`: Generate a secure random string.
    - `ACME_EMAIL`: Email for Let's Encrypt SSL certificates.
    - `DOMAIN`: Your domain name (e.g., `iot.example.com`).

3.  **Start Services**
    ```bash
    docker-compose up -d
    ```

### Docker Compose Configuration

**docker-compose.yml**:
```yaml
services:
  # Traefik Reverse Proxy
  traefik:
    image: traefik:v2.10
    command:
      - "--api.insecure=true"
      - "--providers.docker=true"
      - "--entrypoints.web.address=:80"
      - "--entrypoints.websecure.address=:443"
      - "--certificatesresolvers.letsencrypt.acme.email=${ACME_EMAIL}"
      - "--certificatesresolvers.letsencrypt.acme.storage=/letsencrypt/acme.json"
      - "--certificatesresolvers.letsencrypt.acme.httpchallenge.entrypoint=web"
    ports:
      - "80:80"
      - "443:443"
      - "8080:8080"
    volumes:
      - "/var/run/docker.sock:/var/run/docker.sock:ro"
      - "./letsencrypt:/letsencrypt"

  # Datum Server
  datum-server:
    build:
      context: ..
      dockerfile: docker/Dockerfile
    environment:
      - PORT=8000
      - JWT_SECRET=${JWT_SECRET}
      - RETENTION_MAX_DAYS=${RETENTION_MAX_DAYS:-7}
    volumes:
      - ${DATA_DIR:-../data}:/root/data
    labels:
      - "traefik.enable=true"
      - "traefik.http.routers.datum-server.rule=Host(`${DOMAIN}`)"
      - "traefik.http.routers.datum-server.entrypoints=websecure"
      - "traefik.http.routers.datum-server.tls.certresolver=letsencrypt"
      # WebSocket support (critical for streaming)
      - "traefik.http.services.datum-server.loadbalancer.server.port=8000"
      - "traefik.http.services.datum-server.loadbalancer.passhostheader=true"
      - "traefik.http.middlewares.datum-ws.headers.customrequestheaders.Connection=Upgrade"
      - "traefik.http.middlewares.datum-ws.headers.customrequestheaders.Upgrade=websocket"
      - "traefik.http.routers.datum-server.middlewares=datum-ws"
```

### Management Commands

```bash
# View logs
docker-compose logs -f

# Update images
docker-compose pull
docker-compose up -d

# Stop services
docker-compose down
```

### Integration with Existing Traefik

If you already have a Traefik instance running (e.g., as a global proxy), use the provided `docker-compose.external.yml` file.

**Prerequisites:**
1.  Identify your existing Traefik network name (e.g., `proxy`, `traefik-net`).
2.  Identify your existing Traefik entrypoint names (e.g., `web`, `websecure` or `http`, `https`).

**Configuration:**

Update your `.env` file with your specific Traefik details:

```bash
# Network name where your Traefik is running
TRAEFIK_NETWORK=proxy

# Entrypoint names defined in your Traefik
TRAEFIK_ENTRYPOINT_HTTP=web
TRAEFIK_ENTRYPOINT_HTTPS=websecure

# Cert resolver name defined in your Traefik
TRAEFIK_CERTRESOLVER=letsencrypt
```

**Deployment:**

Run the server using the external configuration file (from the `docker` directory):

```bash
docker-compose -f docker-compose.external.yml up -d
```

networks:
  proxy:
    external: true  # Connect to existing network
```

## ⚙️ Systemd Service

### Installation

```bash
# 1. Build binary
make build

# 2. Install binary
sudo cp datum-server /usr/local/bin/
sudo chmod +x /usr/local/bin/datum-server

# 3. Create service user
sudo useradd -r -s /bin/false datum

# 4. Create directories
sudo mkdir -p /var/lib/datum/data
sudo mkdir -p /var/log/datum
sudo chown -R datum:datum /var/lib/datum
sudo chown -R datum:datum /var/log/datum

# 5. Create systemd service file
sudo nano /etc/systemd/system/datum-server.service
```

### Service Configuration

**/etc/systemd/system/datum-server.service:**
```ini
[Unit]
Description=Datum IoT Platform Server
After=network.target
Wants=network-online.target

[Service]
Type=simple
User=datum
Group=datum

# Environment
Environment="PORT=8080"
Environment="DATA_DIR=/var/lib/datum/data"
Environment="LOG_LEVEL=INFO"
Environment="GIN_MODE=release"
EnvironmentFile=-/etc/datum/datum-server.env

# Execution
ExecStart=/usr/local/bin/datum-server \
  --port ${PORT} \
  --data-dir ${DATA_DIR}

# Security
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=/var/lib/datum /var/log/datum

# Restart policy
Restart=always
RestartSec=10
StartLimitInterval=200
StartLimitBurst=5

# Logging
StandardOutput=journal
StandardError=journal
SyslogIdentifier=datum-server

[Install]
WantedBy=multi-user.target
```

### Environment File

**/etc/datum/datum-server.env:**
```bash
PORT=8080
DATA_DIR=/var/lib/datum/data
JWT_SECRET=your-super-secret-jwt-key-min-32-chars
RETENTION_MAX_DAYS=7
RETENTION_CHECK_HOURS=24
LOG_LEVEL=INFO
GIN_MODE=release
```

### Service Management

```bash
# Reload systemd
sudo systemctl daemon-reload

# Enable service (start on boot)
sudo systemctl enable datum-server

# Start service
sudo systemctl start datum-server

# Check status
sudo systemctl status datum-server

# View logs
sudo journalctl -u datum-server -f

# Stop service
sudo systemctl stop datum-server

# Restart service
sudo systemctl restart datum-server

# Disable service
sudo systemctl disable datum-server
```

### Log Rotation

**/etc/logrotate.d/datum-server:**
```
/var/log/datum/*.log {
    daily
    rotate 7
    compress
    delaycompress
    missingok
    notifempty
    create 0644 datum datum
    postrotate
        systemctl reload datum-server > /dev/null 2>&1 || true
    endscript
}
```

## ☸️ Kubernetes Deployment

### Prerequisites

```bash
# Install kubectl
curl -LO "https://dl.k8s.io/release/$(curl -L -s https://dl.k8s.io/release/stable.txt)/bin/linux/amd64/kubectl"
sudo install -o root -g root -m 0755 kubectl /usr/local/bin/kubectl

# Verify
kubectl version --client
```

### Kubernetes Manifests

**namespace.yaml:**
```yaml
apiVersion: v1
kind: Namespace
metadata:
  name: datum-iot
```

**configmap.yaml:**
```yaml
apiVersion: v1
kind: ConfigMap
metadata:
  name: datum-config
  namespace: datum-iot
data:
  PORT: "8080"
  LOG_LEVEL: "INFO"
  GIN_MODE: "release"
  RETENTION_MAX_DAYS: "7"
  RETENTION_CHECK_HOURS: "24"
```

**secret.yaml:**
```yaml
apiVersion: v1
kind: Secret
metadata:
  name: datum-secret
  namespace: datum-iot
type: Opaque
stringData:
  jwt-secret: "your-super-secret-jwt-key-change-this"
```

**pvc.yaml:**
```yaml
apiVersion: v1
kind: PersistentVolumeClaim
metadata:
  name: datum-data
  namespace: datum-iot
spec:
  accessModes:
    - ReadWriteOnce
  resources:
    requests:
      storage: 10Gi
  storageClassName: standard
```

**deployment.yaml:**
```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: datum-server
  namespace: datum-iot
spec:
  replicas: 3
  selector:
    matchLabels:
      app: datum-server
  template:
    metadata:
      labels:
        app: datum-server
    spec:
      containers:
      - name: datum-server
        image: datum-server:latest
        imagePullPolicy: Always
        ports:
        - containerPort: 8080
          name: http
        env:
        - name: JWT_SECRET
          valueFrom:
            secretKeyRef:
              name: datum-secret
              key: jwt-secret
        envFrom:
        - configMapRef:
            name: datum-config
        volumeMounts:
        - name: data
          mountPath: /app/data
        resources:
          requests:
            memory: "256Mi"
            cpu: "250m"
          limits:
            memory: "512Mi"
            cpu: "500m"
        livenessProbe:
          httpGet:
            path: /health
            port: 8080
          initialDelaySeconds: 30
          periodSeconds: 10
        readinessProbe:
          httpGet:
            path: /ready
            port: 8080
          initialDelaySeconds: 5
          periodSeconds: 5
      volumes:
      - name: data
        persistentVolumeClaim:
          claimName: datum-data
```

**service.yaml:**
```yaml
apiVersion: v1
kind: Service
metadata:
  name: datum-server
  namespace: datum-iot
spec:
  selector:
    app: datum-server
  ports:
  - protocol: TCP
    port: 80
    targetPort: 8080
  type: LoadBalancer
```

**ingress.yaml:**
```yaml
apiVersion: networking.k8s.io/v1
kind: Ingress
metadata:
  name: datum-ingress
  namespace: datum-iot
  annotations:
    cert-manager.io/cluster-issuer: "letsencrypt-prod"
spec:
  ingressClassName: nginx
  tls:
  - hosts:
    - datum.example.com
    secretName: datum-tls
  rules:
  - host: datum.example.com
    http:
      paths:
      - path: /
        pathType: Prefix
        backend:
          service:
            name: datum-server
            port:
              number: 80
```

### Deploy to Kubernetes

```bash
# Apply manifests
kubectl apply -f namespace.yaml
kubectl apply -f configmap.yaml
kubectl apply -f secret.yaml
kubectl apply -f pvc.yaml
kubectl apply -f deployment.yaml
kubectl apply -f service.yaml
kubectl apply -f ingress.yaml

# Check status
kubectl get pods -n datum-iot
kubectl get svc -n datum-iot

# View logs
kubectl logs -f deployment/datum-server -n datum-iot

# Scale deployment
kubectl scale deployment datum-server --replicas=5 -n datum-iot

# Update image
kubectl set image deployment/datum-server datum-server=datum-server:v2.0.0 -n datum-iot

# Delete deployment
kubectl delete -f deployment.yaml
```

## ☸️ k3s Deployment (Edge/IoT)

k3s is a lightweight Kubernetes distribution ideal for IoT edge deployments.

### 1. Install k3s

```bash
curl -sfL https://get.k3s.io | sh -
# Verify installation
sudo k3s kubectl get node
```

### 2. Deployment Manifests

Save the following as `datum-k3s.yaml`:

```yaml
apiVersion: v1
kind: Namespace
metadata:
  name: datum
---
apiVersion: v1
kind: ConfigMap
metadata:
  name: datum-config
  namespace: datum
data:
  PORT: "8000"
  LOG_LEVEL: "INFO"
  RETENTION_MAX_DAYS: "7"
---
apiVersion: v1
kind: Secret
metadata:
  name: datum-secret
  namespace: datum
type: Opaque
stringData:
  jwt-secret: "CHANGE_ME_TO_SECURE_RANDOM_STRING"
---
apiVersion: v1
kind: PersistentVolumeClaim
metadata:
  name: datum-data
  namespace: datum
spec:
  accessModes:
    - ReadWriteOnce
  resources:
    requests:
      storage: 5Gi
---
apiVersion: apps/v1
kind: Deployment
metadata:
  name: datum-server
  namespace: datum
spec:
  replicas: 1
  selector:
    matchLabels:
      app: datum-server
  template:
    metadata:
      labels:
        app: datum-server
    spec:
      containers:
      - name: datum-server
        image: datum-server:latest  # Ensure image is available or use registry
        imagePullPolicy: IfNotPresent
        ports:
        - containerPort: 8000
        envFrom:
        - configMapRef:
            name: datum-config
        env:
        - name: JWT_SECRET
          valueFrom:
            secretKeyRef:
              name: datum-secret
              key: jwt-secret
        volumeMounts:
        - name: data
          mountPath: /root/data
      volumes:
      - name: data
        persistentVolumeClaim:
          claimName: datum-data
---
apiVersion: v1
kind: Service
metadata:
  name: datum-service
  namespace: datum
spec:
  selector:
    app: datum-server
  ports:
    - port: 80
      targetPort: 8000
---
apiVersion: networking.k8s.io/v1
kind: Ingress
metadata:
  name: datum-ingress
  namespace: datum
  annotations:
    traefik.ingress.kubernetes.io/router.entrypoints: web
spec:
  rules:
  - http:
      paths:
      - path: /
        pathType: Prefix
        backend:
          service:
            name: datum-service
            port:
              number: 80
```

### 3. Deploy

```bash
# Apply configuration
sudo k3s kubectl apply -f datum-k3s.yaml

# Check status
sudo k3s kubectl get pods -n datum
```

### 4. Access

The server will be available on port 80 of your node IP.
```bash
curl http://localhost/health
```

## ☁️ Cloud Platforms

### AWS Deployment

**Using EC2:**
```bash
# 1. Launch EC2 instance (Ubuntu 22.04)
# 2. Connect via SSH
ssh -i your-key.pem ubuntu@ec2-instance-ip

# 3. Install Docker
curl -fsSL https://get.docker.com -o get-docker.sh
sudo sh get-docker.sh

# 4. Deploy with Docker Compose
git clone https://github.com/your-org/datum-server.git
cd datum-server
docker-compose up -d
```

**Using ECS:**
- Create task definition with datum-server image
- Configure environment variables
- Set up load balancer
- Create ECS service

### Google Cloud Platform

```bash
# Using Compute Engine
gcloud compute instances create datum-server \
  --image-family=ubuntu-2204-lts \
  --image-project=ubuntu-os-cloud \
  --machine-type=e2-medium \
  --tags=http-server

# Using Cloud Run
gcloud run deploy datum-server \
  --image gcr.io/your-project/datum-server \
  --platform managed \
  --region us-central1 \
  --allow-unauthenticated
```

### Azure

```bash
# Using Azure Container Instances
az container create \
  --resource-group datum-rg \
  --name datum-server \
  --image datum-server:latest \
  --dns-name-label datum-iot \
  --ports 8080 \
  --environment-variables JWT_SECRET=your-secret
```

## 🔐 Security

### SSL/TLS Configuration

**Using Nginx as Reverse Proxy:**
```nginx
server {
    listen 443 ssl http2;
    server_name datum.example.com;

    ssl_certificate /etc/letsencrypt/live/datum.example.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/datum.example.com/privkey.pem;
    
    ssl_protocols TLSv1.2 TLSv1.3;
    ssl_ciphers HIGH:!aNULL:!MD5;
    ssl_prefer_server_ciphers on;

    location / {
        proxy_pass http://localhost:8080;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
    }
}
```

### Firewall Configuration

```bash
# UFW (Ubuntu)
sudo ufw allow 22/tcp    # SSH
sudo ufw allow 80/tcp    # HTTP
sudo ufw allow 443/tcp   # HTTPS
sudo ufw enable

# iptables
sudo iptables -A INPUT -p tcp --dport 22 -j ACCEPT
sudo iptables -A INPUT -p tcp --dport 80 -j ACCEPT
sudo iptables -A INPUT -p tcp --dport 443 -j ACCEPT
```

## 📊 Monitoring

### Health Checks

```bash
# Basic health check
curl http://localhost:8080/health

# Readiness check
curl http://localhost:8080/ready

# Metrics endpoint
curl http://localhost:8080/metrics
```

### Prometheus Integration

**prometheus.yml:**
```yaml
scrape_configs:
  - job_name: 'datum-server'
    static_configs:
      - targets: ['localhost:8080']
    metrics_path: '/metrics'
```

### Grafana Dashboard

Import pre-built dashboard or create custom metrics:
- Request rate
- Response time
- Error rate
- Active connections
- Data ingestion rate

## 💾 Backup & Recovery

### Automated Backups

```bash
# Create backup script
sudo nano /usr/local/bin/datum-backup.sh

#!/bin/bash
BACKUP_DIR="/var/backups/datum"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

# Create backup
tar czf $BACKUP_DIR/datum-$TIMESTAMP.tar.gz /var/lib/datum/data

# Keep only last 7 days
find $BACKUP_DIR -name "datum-*.tar.gz" -mtime +7 -delete

# Make executable
sudo chmod +x /usr/local/bin/datum-backup.sh

# Add to crontab (daily at 2 AM)
sudo crontab -e
0 2 * * * /usr/local/bin/datum-backup.sh
```

### Restore from Backup

```bash
# Stop service
sudo systemctl stop datum-server

# Restore data
sudo tar xzf /var/backups/datum/datum-20250127_020000.tar.gz -C /

# Start service
sudo systemctl start datum-server
```

## 🔧 Troubleshooting

### Common Issues

**Service won't start:**
```bash
# Check logs
sudo journalctl -u datum-server -n 50

# Check permissions
ls -la /var/lib/datum

# Test binary
/usr/local/bin/datum-server --version
```

**High memory usage:**
```bash
# Check process
ps aux | grep datum-server

# Adjust retention settings
sudo nano /etc/datum/datum-server.env
# Reduce RETENTION_MAX_DAYS
```

**Database corruption:**
```bash
# Stop service
sudo systemctl stop datum-server

# Backup current data
sudo cp -r /var/lib/datum/data /var/lib/datum/data.backup

# Restore from backup
sudo tar xzf /var/backups/datum/latest-backup.tar.gz

# Start service
sudo systemctl start datum-server
```

For more help, see [docs/guides/](../../docs/guides/) and [GitHub Issues](https://github.com/your-org/datum-server/issues).

---

**Next Steps:**
- [Security Guide](SECURITY.md)
- [Monitoring Guide](../reference/MONITORING.md)
- [API Reference](../reference/API.md)
