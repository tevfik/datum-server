# Datum IoT System Overview

Datum IoT is a high-performance, self-hosted IoT platform designed for scalability, security, and ease of use. This document provides a high-level overview of the system architecture, core concepts, and design philosophy.

## 1. Introduction

Datum Server provides a complete backend solution for IoT applications, handling:
- Device provisioning and management
- High-throughput time-series data ingestion
- Real-time command and control
- User authentication and role-based access control (RBAC)
- Multi-protocol support (HTTP, MQTT, WebSockets)
- Lightweight rule engine for data-driven automation
- Webhook event notifications
- API versioning (`/api/v1/` prefix)
- TimescaleDB support for production-scale time-series
- Configurable data retention policies
- Comprehensive audit logging

The platform is written in Go, ensuring low latency and high concurrency support.

## 2. Core Concepts

### 2.1 Devices
A **Device** is the fundamental unit of the platform. Each device:
- Has a unique `device_id` (e.g., `dev_abc123`).
- Is owned by a **User** or the System Admin.
- Authenticates using an **API Key** (`dk_...`).
- Sends telemetry data and receives commands.

### 2.2 Users
**Users** manage devices and visualize data.
- Authenticate via JWT (JSON Web Tokens).
- Can have `admin` or `user` roles.
- Can group devices using tags (future feature).

### 2.3 Data (Telemetry)
Time-series data sent by devices.
- Stored in a specialized Release-optimized TSStorage engine.
- Supports structured JSON payloads.
- Indexed by time and device ID.
- Access Pattern: `/dev/:id/data`

### 2.4 Commands
Mechanism to control devices remotely.
- **Asynchronous**: Queued on the server until the device polls.
- **Real-time**: Delivered via Server-Sent Events (SSE) or WebSockets.
- Lifecycle: `pending` -> `delivered` -> `success/failed`.
- Access Pattern: `/dev/:id/cmd`

## 3. Architecture

Datum follows a modular service-oriented architecture:

```mermaid
graph TD
    Client[Mobile/Web App] -->|HTTPS| Server
    Device[IoT Device] -->|HTTPS/MQTT| Server
    CLI[datumctl] -->|HTTPS| Server

    subgraph "Datum Server"
        Handler[API Handlers]
        Auth[Auth Service]
        Engine[Processing Engine]
        
        subgraph "Storage Layer"
            MetaDB[BuntDB (Metadata)]
            TSDB[TSStorage (Time Series)]
        end
    end

    Server --> Handler
    Handler --> Auth
    Handler --> Engine
    Engine --> MetaDB
    Engine --> TSDB
```

### 3.1 Network Interface
- **HTTP/REST**: Primary interface for all operations (Port 8000).
- **MQTT**: Optional interface for lightweight device communication (Port 1883).
- **WebSockets**: For real-time data streaming and logs.

### 3.2 Storage
The platform uses a dual-storage approach:
1.  **Metadata Storage**: BuntDB (in-memory with disk persistence) for fast lookups of users, devices, and keys.
2.  **Time-Series Storage**: Custom TSStorage engine optimized for high-write-throughput sensor data.

## 4. API Design Philosophy

The API is standardized around resource-based paths:

- **/sys**: System-level operations (status, setup, health).
- **/auth**: Authentication services (login, register).
- **/admin**: Administrative functions (user management, system config).
- **/dev**: Device-centric operations.
    - `/dev`: List devices (User context)
    - `/dev/:id`: Device details
    - `/dev/:id/data`: Data operations
    - `/dev/:id/cmd`: Command operations
    - `/dev/:id/stream`: Streaming endpoints

## 5. Security

- **Transport**: HTTPS/TLS recommended for production.
- **Authentication**: 
    - Users: JWT (Bearer Token).
    - Devices: API Key (Bearer Token).
- **Isolation**: Users can only access devices they own (unless Admin).

## 6. Where to Go Next?

| If you want to... | Go to... |
|-------------------|----------|
| **Install the server** | [Quick Start](tutorials/QUICK_START.md) |
| **Integrate a device** | [WoT Integration Guide](guides/wot_integration.md) |
| **Use the CLI** | [CLI Tutorial](tutorials/CLI.md) |
| **Understand the API** | [API Reference](reference/API.md) |
| **Develop Firmware** | [Firmware Tutorial](tutorials/FIRMWARE.md) |
