# External Traefik Configuration Guide

If you are using a centralized **External Traefik** instance (e.g., running on a separate stack) to route traffic to Datum Server, you need to configure two things:
1.  **Traefik Static Configuration**: Open ports (Entrypoints).
2.  **Datum Server Labels**: Route traffic from those entrypoints to the container.

## 1. Traefik Static Configuration (`traefik.yml` or CLI)

Add the following Entrypoints to your main Traefik instance. This ensures Traefik listens on the MQTT ports.

### `traefik.yml`
```yaml
entryPoints:
  web:
    address: ":80"
  websecure:
    address: ":443"
  
  # Add these for Datum MQTT
  mqtt:
    address: ":1883"
  mqtts:
    address: ":8883"
  mqttws:
    address: ":1884"
```

### `docker-compose` (Command Line Args)
```yaml
command:
  - "--entrypoints.web.address=:80"
  - "--entrypoints.websecure.address=:443"
  - "--entrypoints.mqtt.address=:1883"
  - "--entrypoints.mqtts.address=:8883"
  - "--entrypoints.mqttws.address=:1884"
```

---

## 2. Datum Server Configuration (`docker-compose.yaml`)

Update your `datum-server` service labels.

**Crucial**: If your External Traefik is already binding ports 1883/8883 on the host, you **MUST REMOVE** the `ports` section from `datum-server` to avoid conflicts.

```yaml
services:
  datum-server:
    # ... other config ...
    
    # 1. Remove direct port bindings if Traefik handles them
    # ports:
    #   - "1883:1883" 
    #   - "8883:8883"
    #   - "1884:1884"

    labels:
      - "traefik.enable=true"
      - "traefik.docker.network=proxy" # Ensure this matches your Traefik network

      # --- HTTP/HTTPS (Existing) ---
      - "traefik.http.routers.datum-server.rule=Host(`datum.example.com`)"
      - "traefik.http.routers.datum-server.entrypoints=websecure"
      - "traefik.http.routers.datum-server.tls=true"
      - "traefik.http.services.datum-server.loadbalancer.server.port=8080"

      # --- MQTT TCP (1883) ---
      # Uses HostSNI(`*`) because raw TCP doesn't have Host headers without TLS
      - "traefik.tcp.routers.datum-mqtt.rule=HostSNI(`*`)"
      - "traefik.tcp.routers.datum-mqtt.entrypoints=mqtt"
      - "traefik.tcp.services.datum-mqtt.loadbalancer.server.port=1883"

      # --- MQTT MQTTS (8883) ---
      # Uses SNI routing (requires client to send correct SNI)
      - "traefik.tcp.routers.datum-mqtts.rule=HostSNI(`datum.example.com`)"
      - "traefik.tcp.routers.datum-mqtts.entrypoints=mqtts"
      - "traefik.tcp.routers.datum-mqtts.tls=true"
      - "traefik.tcp.routers.datum-mqtts.tls.certresolver=letsencrypt" # Or your resolver name
      - "traefik.tcp.services.datum-mqtts.loadbalancer.server.port=1883" # Target internal 1883 port (TLS terminated by Traefik)

      # --- MQTT WSS (1884) ---
      - "traefik.http.routers.datum-mqttws.rule=Host(`datum.example.com`)"
      - "traefik.http.routers.datum-mqttws.entrypoints=mqttws"
      - "traefik.http.routers.datum-mqttws.tls=true"
      - "traefik.http.routers.datum-mqttws.tls.certresolver=letsencrypt"
      - "traefik.http.services.datum-mqttws.loadbalancer.server.port=1884"
      # Websocket Headers
      - "traefik.http.middlewares.mqtt-ws.headers.customrequestheaders.Connection=Upgrade"
      - "traefik.http.middlewares.mqtt-ws.headers.customrequestheaders.Upgrade=websocket"
      - "traefik.http.routers.datum-mqttws.middlewares=mqtt-ws"
```
