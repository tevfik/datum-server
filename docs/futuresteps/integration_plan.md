# RTSP Integration Proposal for Datum Server

This document outlines the strategy for adding RTSP (Real-Time Streaming Protocol) support to Datum Server, transforming it into a lightweight NVR (Network Video Recorder) compatible with professional IP cameras.

## 1. Core Technology: MediaMTX Integration
We recommend embedding or side-loading **MediaMTX** (written in Go) as the media engine.

### Why MediaMTX?
- **All-in-One:** Supports RTSP, RTMP, HLS, WebRTC, and SRT.
- **Zero Dependency:** Single binary, easy to bundle with Datum Server.
- **Low Latency:** Optimized for sub-second streaming.

### Architecture: "Datum Media Gateway" (Microservice)
Instead of embedding MediaMTX directly into the Datum Server monolith:
1.  **Standalone Service:** A separate `datum-media-gateway` binary/container running MediaMTX and FFmpeg.
2.  **API Communication:** Datum Server talks to Media Gateway via REST API (e.g., `POST /camera/add`, `GET /stream/url`).
3.  **Authentication:** The Gateway validates tokens against Datum Server (central auth).

This decouples the heavy media processing from the core IoT logic.

## 2. Proposed Features

### A. Universal Camera Support
- **Support:** Any ONVIF/RTSP compatible camera (Hikvision, Dahua, TP-Link Tapo).
- **Setup:** User enters `rtsp://user:pass@ip:554/stream` in Datum Dash.
- **Benefit:** Mix ESP32 cameras (MJPEG) with Pro cameras (H.264/H.265) in one dashboard.

### B. High-Performance Web Player
- **Technique:** WebRTC (UDP) or HLS (Low Latency).
- **Latency:** <500ms for commercial cameras.
- **Efficiency:** Uses GPU decoding in the browser, unlike MJPEG canvas drawing.

### C. Smart NVR (Network Video Recording)
Instead of continuous recording:
- **Segmented Recording:** Save video as 10-minute `.mp4` segments.
- **Retention Policy:** Auto-delete footage older than X days (using `tstorage` concepts but for files).
- **Event-Based Clip:** Trigger recording only when a Datum IoT sensor (e.g., PIR sensor) fires.

### D. Edge AI Pipeline (Future)
Since we have a clean H.264 stream:
- **Object Detection:** Pipe keyframes to a local YOLO model (or Opus API) to detect "Person", "Car", "Package".
- **Smart Alerts:** "Person seen at Front Door" notification via MQTT.

## 3. Implementation Steps
1.  **Media Gateway Repo:** Create a new repository for the Media Gateway service.
2.  **Orchestrator:** Write a Go package in Datum to communicate with the Gateway.
3.  **Proxy API:** Create endpoints in Datum to proxy HLS/WebRTC content so users don't need open ports.

## 4. ESP32 Compatibility
- ESP32 can send RTSP (via libraries like Micro-RTSP), but it's CPU intensive.
- **Better approach:** Keep ESP32 on MJPEG, let Datum Server transcode MJPEG -> H.264 (using FFmpeg) for unified storage.

---
**Recommendation:** Start by adding **MediaMTX** as a sub-process managed by Datum. This gives you immediate access to HLS/WebRTC streaming without rewriting the wheel.
