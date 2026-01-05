# IoT Architecture Proposal: Polling vs. Push

This document analyzes the current polling-based architecture and proposes a transition to a Push-based model (MQTT/WebSockets).

## 1. Current State: HTTP Polling

### Architecture
*   **Heartbeat**: Devices POST telemetry every 60 seconds.
*   **Command Poll**: Devices GET `/commands/poll` every 5 seconds.

### Analysis
*   **Pros**:
    *   Simple to implement (Request/Response).
    *   Works easily behind NAT/Firewalls.
    *   Stateless server (mostly).
*   **Cons**:
    *   **High Latency**: Commands can take up to 5 seconds to be received.
    *   **High Overhead**: 90%+ of traffic is empty "No commands" responses.
    *   **Scalability**: 1,000 devices = 200 req/sec constant load. 10k devices = 2,000 req/sec.

## 2. Proposed Future: Push-Based (MQTT)

### Architecture
*   **Protocol**: MQTT v3.1.1 / v5 over TCP (or WebSocket for web clients).
*   **Broker**: Mosquitto, EMQX, or embedded Go MQTT broker.
*   **Flow**:
    *   Device connects and subscribes to `devices/{id}/commands`.
    *   Server *publishes* command immediately when User clicks a button.
    *   Device receives command in milliseconds.

### Benefits
*   **Real-time**: Latency drops from ~2.5s (avg) to <100ms.
*   **Efficiency**: Keep-alive packets are bytes, not kilobytes. Bandwidth usage drops siginificantly.
*   **Scalability**: A single MQTT broker can handle 10k-100k concurrent connections on modest hardware.

## 3. Alternative: WebSockets

### Architecture
*   Devices open a persistent WebSocket connection to the Go server (`/ws`).
*   Server holds connection open and pushes JSON frames.

### Comparison to MQTT
*   **Pros**: Reuses existing HTTP port (443/80). No separate broker needed.
*   **Cons**: No built-in QoS (Quality of Service). Harder to implement reconnect/queue logic manually than using a battle-tested MQTT client.

## 4. Recommendation

**Adopt MQTT (Managed or Self-hosted)**
1.  **Phase 1**: Add MQTT Client to Firmware. Keep HTTP for large file uploads (Snapshots).
2.  **Phase 2**: Deploy an MQTT Broker (e.g., Mosquitto) alongside the Go Server.
3.  **Phase 3**: Update Go Server to publish commands to MQTT instead of queuing for Poll.

This transition will be discussed and detailed further in future syncs.
