# ESP32-S3 Camera Streaming with Datum Server

Complete guide for setting up video streaming from ESP32-S3 camera boards to Datum server using WebSocket and MJPEG protocols.

## Table of Contents

- [Overview](#overview)
- [Hardware Requirements](#hardware-requirements)
- [Supported Boards](#supported-boards)
- [Architecture](#architecture)
- [Quick Start](#quick-start)
- [WebSocket vs MJPEG Comparison](#websocket-vs-mjpeg-comparison)
- [Traefik Configuration](#traefik-configuration)
- [API Endpoints](#api-endpoints)
- [ESP32 Firmware](#esp32-firmware)
- [Web Viewer](#web-viewer)
- [Troubleshooting](#troubleshooting)

---

## Overview

This example demonstrates real-time camera streaming from ESP32-S3 behind NAT/proxy to web clients using Datum server as a relay.

**Supported Cameras:**
- ✅ **OV2640** (2MP) - Default sensor on most boards
- ✅ **OV3660** (3MP) - Higher resolution upgrade

**Supported Protocols:**
- **WebSocket**: Binary frames, low latency (~50-100ms), best for live streaming
- **MJPEG over HTTP**: Text-based, higher latency (~200ms), simpler but works everywhere

**Key Features:**
- ✅ Works behind NAT/firewall (no port forwarding needed)
- ✅ Command-based stream control (start/stop via datumctl)
- ✅ Multiple simultaneous viewers
- ✅ Automatic frame broadcasting
- ✅ WiFi auto-reconnection
- ✅ Compatible with Traefik reverse proxy

---

## Hardware Requirements

### ESP32-S3 (Recommended)
- **Processor**: Dual-core Xtensa LX7 @ 240MHz
- **RAM**: 512KB SRAM + 8MB PSRAM (recommended)
- **Flash**: Minimum 4MB
- **Camera Interface**: DVP (8-bit parallel)

> **Note**: ESP32-S3 is recommended over ESP32-S2 for camera applications. The ESP32-S2 has limited camera interface support and may have compatibility issues with some sensors.

### ⚠️ Important: PSRAM Configuration
Most ESP32-S3 Camera boards (especially Freenove S3 Cam and generic clones) use **8MB OPI PSRAM**.
- **Error**: `E (226) quad_psram: PSRAM chip is not connected`
- **Fix**: In Arduino IDE, go to **Tools > PSRAM** and select **"OPI PSRAM"**.
- if your board has no PSRAM, select "Disabled".

---

## Supported Boards

### 1. ESP32-S3-CAM (Seeed XIAO, Waveshare)
```
XCLK:  GPIO 10     Y5:    GPIO 16
SIOD:  GPIO 40     Y4:    GPIO 18
SIOC:  GPIO 39     Y3:    GPIO 17
Y9:    GPIO 48     Y2:    GPIO 15
Y8:    GPIO 11     VSYNC: GPIO 38
Y7:    GPIO 12     HREF:  GPIO 47
Y6:    GPIO 14     PCLK:  GPIO 13
```

### 2. Freenove ESP32-S3 WROOM CAM
```
XCLK:  GPIO 15     Y5:    GPIO 10
SIOD:  GPIO  4     Y4:    GPIO  8
SIOC:  GPIO  5     Y3:    GPIO  9
Y9:    GPIO 16     Y2:    GPIO 11
Y8:    GPIO 17     VSYNC: GPIO  6
Y7:    GPIO 18     HREF:  GPIO  7
Y6:    GPIO 12     PCLK:  GPIO 13
```

### 3. AI-Thinker ESP32-CAM (Legacy)
```
PWDN:  GPIO 32     Y5:    GPIO 21
XCLK:  GPIO  0     Y4:    GPIO 19
SIOD:  GPIO 26     Y3:    GPIO 18
SIOC:  GPIO 27     Y2:    GPIO  5
Y9:    GPIO 35     VSYNC: GPIO 25
Y8:    GPIO 34     HREF:  GPIO 23
Y7:    GPIO 39     PCLK:  GPIO 22
Y6:    GPIO 36
```

### Camera Sensors

| Sensor | Resolution | Max FPS | Notes |
|--------|------------|---------|-------|
| **OV2640** | 1600x1200 (2MP) | 15fps @ UXGA | Default, widely available |
| **OV3660** | 2048x1536 (3MP) | 15fps @ QXGA | Higher quality, may need vflip |

---

## Architecture

```
┌──────────────────┐         ┌──────────────────┐         ┌──────────────────┐
│   ESP32-S3 CAM   │         │  Datum Server    │         │   Web Browser    │
│   (Behind NAT)   │         │  (+ Traefik)     │         │   (Any Device)   │
└──────────────────┘         └──────────────────┘         └──────────────────┘
        │                            │                             │
        │ 1. Poll commands           │                             │
        │───────────────────────────>│                             │
        │    GET /device/:id/commands│                             │
        │                            │                             │
        │ 2. Receive "start-stream"  │    3. User connects         │
        │<───────────────────────────│<────────────────────────────│
        │                            │    WS /devices/:id/stream/ws│
        │                            │                             │
        │ 4. POST JPEG frames        │                             │
        │───────────────────────────>│                             │
        │  /device/:id/stream/frame  │                             │
        │                            │ 5. Broadcast frames         │
        │                            │────────────────────────────>│
        │                            │    (WebSocket binary)       │
        │                            │                             │
        │ (Repeat every 100ms)       │ (Real-time broadcast)       │
```

**Data Flow:**
1. ESP32-S3 polls for commands (every 5 seconds)
2. User sends `start-stream` command via datumctl
3. ESP32-S3 starts capturing and POSTing JPEG frames
4. Server broadcasts frames to all connected WebSocket/MJPEG clients
5. Web viewer displays real-time video

---

## Quick Start

### 1. Setup Datum Server

Ensure streaming handlers are registered:

```go
// cmd/server/main.go

// Device uploads frames
deviceCommandGroup.POST("/:device_id/stream/frame", uploadFrameHandler)

// Users watch streams
streamGroup.GET("/:device_id/stream/mjpeg", mjpegStreamHandler)  // MJPEG
streamGroup.GET("/:device_id/stream/ws", websocketStreamHandler) // WebSocket
streamGroup.GET("/:device_id/stream/info", streamInfoHandler)    // Metadata
```

Build and run:
```bash
cd cmd/server
go build
./server
```

### 2. Flash ESP32 Firmware

1. Install Arduino IDE + ESP32 board support
2. Install libraries:
   - `esp_camera`
   - `ArduinoJson` (optional, for advanced JSON parsing)

3. Open `esp32_camera_streamer.ino`

4. Configure:
```cpp
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const char* serverURL = "http://192.168.1.100:8080";
const char* deviceID = "esp32-cam-01";
const char* apiKey = "your-device-api-key";
```

5. Upload to ESP32

### 3. Register Device

```bash
# Create device and get API key
datumctl device create --name "ESP32 Camera" --type camera

# Output:
# Device ID: esp32-cam-01
# API Key: dev_xxxxxxxxxxxxxxxx
```

Update firmware with the API key.

### 4. Start Streaming

```bash
# Start stream
datumctl command send esp32-cam-01 start-stream

# Stop stream
datumctl command send esp32-cam-01 stop-stream

# Capture single frame
datumctl command send esp32-cam-01 capture-frame
```

### 5. View Stream

**Option A: Web Viewer**
1. Open `stream_viewer.html` in browser
2. Enter server URL, device ID, and JWT token
3. Select protocol (WebSocket recommended)
4. Click "Connect"

**Option B: MJPEG in Browser**
```
http://your-server:8080/devices/esp32-cam-01/stream/mjpeg?token=YOUR_JWT
```

**Option C: VLC Player**
```bash
vlc http://your-server:8080/devices/esp32-cam-01/stream/mjpeg?token=YOUR_JWT
```

---

## WebSocket vs MJPEG Comparison

| Feature | WebSocket | MJPEG over HTTP |
|---------|-----------|-----------------|
| **Protocol** | Binary (ws://) | Text/HTTP (http://) |
| **Latency** | ~50-100ms | ~200-300ms |
| **Throughput** | 5-10 MB/s | 1-2 MB/s |
| **CPU Usage (Server)** | Low | Medium |
| **Overhead** | Minimal | +33% (base64 if JSON) |
| **Browser Support** | Modern browsers | All browsers |
| **Traefik Support** | ✅ Yes (with config) | ✅ Yes (native) |
| **Mobile Support** | ✅ Yes | ✅ Yes |
| **VLC Player** | ❌ No | ✅ Yes |
| **Best For** | Real-time apps | Compatibility |

**Recommendation:**
- **Use WebSocket** for live dashboards, security cameras, robotics
- **Use MJPEG** for simple viewing, testing, VLC compatibility

---

## Traefik Configuration

### WebSocket Support

Traefik requires explicit WebSocket upgrade configuration:

```yaml
# docker-compose.yml
services:
  traefik:
    image: traefik:v2.10
    command:
      - "--api.insecure=true"
      - "--providers.docker=true"
      - "--entrypoints.web.address=:80"
      - "--entrypoints.websecure.address=:443"
    ports:
      - "80:80"
      - "443:443"
    volumes:
      - "/var/run/docker.sock:/var/run/docker.sock:ro"

  datum-server:
    image: datum-server:latest
    labels:
      - "traefik.enable=true"
      
      # HTTP routes
      - "traefik.http.routers.datum.rule=Host(`datum.example.com`)"
      - "traefik.http.routers.datum.entrypoints=websecure"
      - "traefik.http.routers.datum.tls=true"
      
      # WebSocket support (critical!)
      - "traefik.http.services.datum.loadbalancer.passhostheader=true"
      - "traefik.http.middlewares.datum-headers.headers.customrequestheaders.Connection=Upgrade"
      - "traefik.http.middlewares.datum-headers.headers.customrequestheaders.Upgrade=websocket"
      - "traefik.http.routers.datum.middlewares=datum-headers"
      
      # Port
      - "traefik.http.services.datum.loadbalancer.server.port=8080"
```

### Static Configuration (traefik.yml)

```yaml
entryPoints:
  web:
    address: ":80"
  websecure:
    address: ":443"

providers:
  docker:
    exposedByDefault: false

# Enable WebSocket upgrade
http:
  middlewares:
    websocket-headers:
      headers:
        customRequestHeaders:
          Connection: "Upgrade"
          Upgrade: "websocket"
```

### Dynamic Configuration (config.yml)

```yaml
http:
  routers:
    datum:
      rule: "Host(`datum.example.com`)"
      service: datum
      middlewares:
        - websocket-headers
      tls:
        certResolver: letsencrypt

  services:
    datum:
      loadBalancer:
        servers:
          - url: "http://datum-server:8080"
        passHostHeader: true
```

**Test WebSocket Connection:**
```bash
# Should return 101 Switching Protocols
curl -i -N \
  -H "Connection: Upgrade" \
  -H "Upgrade: websocket" \
  -H "Sec-WebSocket-Version: 13" \
  -H "Sec-WebSocket-Key: test" \
  https://datum.example.com/devices/esp32-cam-01/stream/ws
```

---

## API Endpoints

### Device Side (ESP32)

#### Upload Frame
```http
POST /device/:device_id/stream/frame
X-API-Key: {device_api_key}
Content-Type: image/jpeg

<binary JPEG data>
```

**Response:**
```json
{
  "status": "frame_received",
  "size_bytes": 25600,
  "clients": 3
}
```

#### Poll Commands
```http
GET /device/:device_id/commands
X-API-Key: {device_api_key}
```

**Response:**
```json
{
  "commands": [
    {
      "id": "cmd_abc123",
      "action": "start-stream",
      "params": {
        "fps": 10,
        "quality": 12
      }
    }
  ]
}
```

### Client Side (Web/App)

#### MJPEG Stream
```http
GET /devices/:device_id/stream/mjpeg
Authorization: Bearer {jwt_token}
```

**Response:** Multipart MJPEG stream
```
--frame
Content-Type: image/jpeg
Content-Length: 25600

<JPEG data>
--frame
Content-Type: image/jpeg
...
```

#### WebSocket Stream
```http
GET /devices/:device_id/stream/ws
Authorization: Bearer {jwt_token}
Upgrade: websocket
```

**Messages:**
- Text: Metadata (JSON)
- Binary: JPEG frames

#### Stream Info
```http
GET /devices/:device_id/stream/info
Authorization: Bearer {jwt_token}
```

**Response:**
```json
{
  "device_id": "esp32-cam-01",
  "active": true,
  "clients": 2,
  "last_updated": "2025-12-28T10:30:45Z",
  "has_frame": true,
  "frame_size": 25600
}
```

---

## ESP32 Firmware

### Custom Stream Configuration

```cpp
// Modify camera settings
sensor_t* s = esp_camera_sensor_get();

// Resolution (higher = better quality, more bandwidth)
// FRAMESIZE_QVGA  (320x240)   - Low bandwidth
// FRAMESIZE_VGA   (640x480)   - Balanced
// FRAMESIZE_SVGA  (800x600)   - High quality
// FRAMESIZE_XGA   (1024x768)  - Very high quality
config.frame_size = FRAMESIZE_VGA;

// JPEG quality (0-63, lower = better quality)
config.jpeg_quality = 12;  // Good balance

// Frame rate
#define STREAM_FPS 10  // 10 frames per second
```

### Advanced Command Handling

```cpp
void handleAdvancedCommands() {
  // Parse JSON with ArduinoJson library
  DynamicJsonDocument doc(2048);
  deserializeJson(doc, payload);
  
  JsonArray commands = doc["commands"];
  for (JsonObject cmd : commands) {
    String action = cmd["action"];
    
    if (action == "set-quality") {
      int quality = cmd["params"]["quality"] | 12;
      sensor_t* s = esp_camera_sensor_get();
      s->set_quality(s, quality);
    }
    else if (action == "set-resolution") {
      String res = cmd["params"]["resolution"];
      if (res == "QVGA") config.frame_size = FRAMESIZE_QVGA;
      else if (res == "VGA") config.frame_size = FRAMESIZE_VGA;
      esp_camera_deinit();
      esp_camera_init(&config);
    }
    else if (action == "set-fps") {
      STREAM_FPS = cmd["params"]["fps"] | 10;
    }
  }
}
```

---

## Web Viewer

### Features

- ✅ Real-time FPS counter
- ✅ Bandwidth monitor
- ✅ Frame size display
- ✅ Protocol switching (WebSocket/MJPEG)
- ✅ Connection status
- ✅ Activity log
- ✅ Responsive design

### Usage

1. Open `stream_viewer.html`
2. Configure:
   - **Server**: `http://localhost:8080`
   - **Device ID**: `esp32-cam-01`
   - **Token**: Your JWT token
   - **Protocol**: WebSocket or MJPEG
3. Click "Connect"

### Embedding in Dashboard

```html
<iframe src="stream_viewer.html?server=http://localhost:8080&device=esp32-cam-01&token=YOUR_TOKEN&protocol=ws"
        width="800" height="600" frameborder="0"></iframe>
```

---

## Troubleshooting

### ESP32 Issues

**Camera init failed:**
```
Solution:
- Check power supply (5V 2A minimum)
- Verify pin connections
- Try different camera module
```

**WiFi connection failed:**
```
Solution:
- Check SSID/password
- Ensure 2.4GHz WiFi (ESP32 doesn't support 5GHz)
- Move closer to router
```

**Frames not uploading:**
```
Solution:
- Check server URL is reachable
- Verify API key is correct
- Monitor Serial output for errors
- Check firewall rules
```

### Server Issues

**WebSocket upgrade failed:**
```
Solution:
- Add Traefik WebSocket headers (see Traefik config)
- Check nginx proxy: proxy_http_version 1.1; proxy_set_header Upgrade $http_upgrade;
- Verify CORS settings
```

**High CPU usage:**
```
Solution:
- Limit concurrent viewers
- Reduce frame rate on ESP32
- Increase JPEG compression (higher quality number)
- Add frame dropping: maxClients per device
```

**Memory leak:**
```
Solution:
- Frames are buffered (30 frame buffer)
- Old clients are cleaned up automatically
- Monitor with: GET /devices/:id/stream/info
```

### Network Issues

**Behind Traefik/NAT:**
```bash
# Test from inside network
curl http://localhost:8080/devices/esp32-cam-01/stream/info

# Test through Traefik
curl https://datum.example.com/devices/esp32-cam-01/stream/info
```

**Bandwidth issues:**
```
Solutions:
- Reduce resolution (QVGA instead of VGA)
- Reduce FPS (5 fps instead of 10)
- Increase JPEG quality number (20 instead of 12)
- Use motion detection (only stream when needed)
```

---

## Performance Tuning

### ESP32 Optimization

```cpp
// Low bandwidth (mobile networks)
config.frame_size = FRAMESIZE_QVGA;  // 320x240
config.jpeg_quality = 20;
#define STREAM_FPS 5

// Balanced (home WiFi)
config.frame_size = FRAMESIZE_VGA;   // 640x480
config.jpeg_quality = 12;
#define STREAM_FPS 10

// High quality (local network)
config.frame_size = FRAMESIZE_SVGA;  // 800x600
config.jpeg_quality = 8;
#define STREAM_FPS 15
```

### Server Optimization

```go
// Limit clients per device
const maxClientsPerDevice = 5

// Frame buffer size
clientChan := make(chan []byte, 30)  // 30 frames = ~3s at 10fps

// Cleanup inactive streams
go func() {
    ticker := time.NewTicker(5 * time.Minute)
    for range ticker.C {
        streamManager.CleanupInactiveStreams()
    }
}()
```

---

## Advanced Features

### Motion Detection

```cpp
// Only stream when motion detected
bool detectMotion(camera_fb_t* fb) {
  static uint8_t* prevFrame = NULL;
  
  if (!prevFrame) {
    prevFrame = (uint8_t*)malloc(fb->len);
    memcpy(prevFrame, fb->buf, fb->len);
    return false;
  }
  
  uint32_t diff = 0;
  for (size_t i = 0; i < fb->len; i++) {
    diff += abs(fb->buf[i] - prevFrame[i]);
  }
  
  memcpy(prevFrame, fb->buf, fb->len);
  
  return (diff > 1000000);  // Threshold
}
```

### Adaptive Quality

```cpp
// Reduce quality if upload is slow
if (uploadTime > 500) {  // > 500ms
  sensor_t* s = esp_camera_sensor_get();
  s->set_quality(s, min(quality + 5, 63));
}
```

### Multiple Cameras

```cpp
// Device IDs
const char* deviceID1 = "esp32-cam-front";
const char* deviceID2 = "esp32-cam-back";

// Alternate frames
if (frameCount % 2 == 0) {
  sendFrame(deviceID1);
} else {
  sendFrame(deviceID2);
}
```

---

## Summary

**Completed Implementation:**
- ✅ WebSocket binary streaming (low latency)
- ✅ MJPEG HTTP streaming (high compatibility)
- ✅ Traefik reverse proxy support
- ✅ Multi-client broadcasting
- ✅ Command-based stream control
- ✅ Complete ESP32-S3 firmware
- ✅ Web viewer with statistics
- ✅ NAT/firewall friendly

**Next Steps:**
1. Deploy Datum server behind Traefik
2. Flash ESP32 firmware
3. Test with `stream_viewer.html`
4. Integrate into your dashboard

**Resources:**
- Server code: `cmd/server/stream_handlers.go`
- ESP32 firmware: `examples/esp32-s3-camera/esp32_camera_streamer.ino`
- Web viewer: `examples/esp32-s3-camera/stream_viewer.html`
- This guide: `examples/esp32-s3-camera/README.md`
