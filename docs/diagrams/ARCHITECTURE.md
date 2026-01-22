# Architecture Diagrams

This document contains visual representations of the Datum IoT Platform architecture, data flows, and component interactions.

## System Architecture Overview

```mermaid
graph TB
    subgraph "IoT Devices"
        D1[ESP32/Arduino]
        D2[Raspberry Pi]
        D3[Custom Device]
    end

    subgraph "Datum Server"
        API[REST API<br/>Gin Framework]
        MQTT[MQTT Broker<br/>Mochi-MQTT]
        SSE[SSE Handler<br/>Legacy/Web]
        AUTH[Auth Middleware<br/>JWT + API Keys]
        
        subgraph "Storage Layer"
            DB[(PostgreSQL/BuntDB<br/>Metadata)]
            TS[(TSStorage<br/>Time-Series)]
        end
        
        RET[Retention Worker<br/>Background Cleanup]
    end

    subgraph "Clients"
        WEB[Web Dashboard]
        CLI[datumctl CLI]
        APP[Mobile App]
    end

    D1 -->|POST /dev/{id}/data| API
    D1 -->|MQTT dev/{id}/data| MQTT
    D2 -->|POST /dev/{id}/data| API
    D3 -->|POST /dev/{id}/data| API
    
    D1 -.->|Sub: dev/{id}/conf| MQTT
    D1 -->|GET /dev/{id}| API
    
    API --> AUTH
    MQTT --> AUTH
    AUTH --> DB
    AUTH --> TS
    
    MQTT --> TS
    
    RET -->|Cleanup| TS
    
    WEB -->|JWT Auth| API
    WEB -.->|Sub: dev/{id}/data| MQTT
    CLI -->|JWT Auth| API
    APP -->|JWT Auth| API
    
    style API fill:#2196F3
    style MQTT fill:#E91E63
    style DB fill:#4CAF50
    style TS fill:#FF9800
    style AUTH fill:#9C27B0
```

## Data Flow: Device to Storage (HTTP & MQTT)

```mermaid
sequenceDiagram
    participant Device
    participant MQTT as MQTT Broker
    participant API as REST API
    participant Auth as Auth Middleware
    participant DB as Metadata DB
    participant TSStorage
    
    par HTTP Path
        Device->>API: POST /dev/{id}/data<br/>Bearer: API_KEY
        API->>Auth: Validate Credentials
        Auth->>DB: Lookup Device
        API->>TSStorage: Store Data
        API-->>Device: 200 OK
    and MQTT Path
        Device->>MQTT: CONNECT (User=ID, Pass=Key)
        MQTT->>Auth: OnConnect Hook
        Auth->>DB: Verify Key
        MQTT-->>Device: CONNACK
        
        Device->>MQTT: PUBLISH dev/{id}/data
        MQTT->>Auth: Ingestion Hook
        Auth->>TSStorage: Store Data Batch
    end
```

## Data Flow: User Authentication

```mermaid
sequenceDiagram
    participant User
    participant API as REST API
    participant Auth as Auth Module
    participant DB as Metadata DB
    
    User->>API: POST /auth/login<br/>{email, password}
    API->>Auth: Validate credentials
    Auth->>DB: Get user by email
    DB-->>Auth: User record
    
    alt Valid Credentials
        Auth->>Auth: Verify password hash
        Auth->>Auth: Generate JWT token
        Auth-->>API: Token + User info
        API-->>User: 200 OK<br/>{token, user_id}
    else Invalid Credentials
        Auth-->>API: Authentication failed
        API-->>User: 401 Invalid credentials
    end
```

## Storage Architecture

```mermaid
graph LR
    subgraph "Metadata DB (Postgres/Bunt)"
        U[Users<br/>user:&#123;id&#125;]
        D[Devices<br/>device:&#123;id&#125;]
        AK[API Keys<br/>apikey:&#123;key&#125;]
        SYS[System Config<br/>system:*]
        CMD[Commands<br/>command:&#123;id&#125;]
    end
    
    subgraph "TSStorage (Time-Series)"
        P1[Partition<br/>2024-01-01_00]
        P2[Partition<br/>2024-01-01_01]
        P3[Partition<br/>2024-01-01_02]
        PN[Partition<br/>...]
    end
    
    U --> D
    D --> AK
    D -.->|device_id| P1
    D -.->|device_id| P2
    D -.->|device_id| P3
    
    style U fill:#4CAF50
    style D fill:#4CAF50
    style AK fill:#4CAF50
    style SYS fill:#4CAF50
    style CMD fill:#4CAF50
    style P1 fill:#FF9800
    style P2 fill:#FF9800
    style P3 fill:#FF9800
    style PN fill:#FF9800
```

## Remote Configuration Flow (Shadow Twin)

```mermaid
sequenceDiagram
    participant User
    participant API
    participant DB as Metadata DB
    participant MQTT
    participant Device

    Note over User,Device: 1. Push Update (Real-time)
    User->>API: PATCH /dev/{id}/config<br/>{ "resolution": "1080p" }
    API->>DB: Update 'desired_state' JSONB
    API->>MQTT: Publish 'dev/{id}/conf/set'<br/>Payload: { "desired": {...} }
    MQTT->>Device: Message Received (Subscribed)
    Device->>Device: Apply Settings (NVS)

    Note over User,Device: 2. Pull Sync (Boot/Fallback)
    Device->>Device: Power On / Reboot
    Device->>API: GET /dev/{id}
    API->>DB: Read Device + Desired State
    API-->>Device: JSON { ..., "desired_state": {...} }
    Device->>Device: Compare & Apply
```

## Command Flow: SSE Real-time Commands

```mermaid
sequenceDiagram
    participant Admin as Admin/User
    participant API as REST API
    participant BuntDB
    participant SSE as SSE Handler
    participant Device
    
    Note over Device,SSE: Device maintains SSE connection
    Device->>SSE: GET /dev/{id}/cmd/stream
    SSE-->>Device: SSE Connection Open
    
    Admin->>API: POST /dev/{id}/cmd<br/>{action: "reboot"}
    API->>BuntDB: Store command
    BuntDB-->>API: Command ID
    API-->>Admin: 201 Command queued
    
    Note over SSE,Device: Command delivered in real-time
    SSE->>Device: event: command<br/>data: {id, action: "reboot"}
    
    Device->>API: POST /dev/&#123;id&#125;/cmd/&#123;cmd_id&#125;/ack<br/>{status: "success"}
    API->>BuntDB: Update command status
    API-->>Device: 200 OK
```

## Rate Limiting Flow

```mermaid
graph TD
    REQ[Incoming Request] --> CHECK{Rate Limiter<br/>Check}
    
    CHECK -->|Under Limit| PROCESS[Process Request]
    CHECK -->|Over Limit| REJECT[429 Too Many Requests]
    
    PROCESS --> CONSUME[Consume Token]
    CONSUME --> RESPONSE[Send Response]
    
    REJECT --> RETRY[Retry-After Header]
    
    subgraph "Token Bucket"
        BUCKET[Bucket: 100 tokens/min]
        REFILL[Refill: 1.67 tokens/sec]
    end
    
    CHECK --> BUCKET
    BUCKET --> REFILL
    
    style CHECK fill:#9C27B0
    style REJECT fill:#f44336
    style PROCESS fill:#4CAF50
```

## Deployment Architecture

### Single Server Deployment

```mermaid
graph TB
    subgraph "Host Machine"
        subgraph "Docker"
            SERVER[Datum Server<br/>:8000]
            DATA[(Data Volume<br/>/root/data)]
        end
        
        NGINX[Nginx Reverse Proxy<br/>:443/:80]
    end
    
    INTERNET((Internet)) --> NGINX
    NGINX -->|proxy_pass| SERVER
    SERVER --> DATA
    
    style SERVER fill:#2196F3
    style NGINX fill:#4CAF50
    style DATA fill:#FF9800
```

### High Availability Deployment

```mermaid
graph TB
    subgraph "Load Balancer"
        LB[HAProxy/Nginx<br/>Load Balancer]
    end
    
    subgraph "Application Tier"
        S1[Datum Server 1]
        S2[Datum Server 2]
        S3[Datum Server 3]
    end
    
    subgraph "Storage Tier"
        NFS[(Shared Storage<br/>NFS/EFS)]
    end
    
    INTERNET((Internet)) --> LB
    LB --> S1
    LB --> S2
    LB --> S3
    
    S1 --> NFS
    S2 --> NFS
    S3 --> NFS
    
    style LB fill:#9C27B0
    style S1 fill:#2196F3
    style S2 fill:#2196F3
    style S3 fill:#2196F3
    style NFS fill:#FF9800
```

## Data Retention Flow

```mermaid
graph LR
    subgraph "Time-Series Partitions"
        OLD[Old Partitions<br/>Age > 7 days]
        CURRENT[Current Partitions<br/>Age < 7 days]
    end
    
    WORKER[Retention Worker<br/>Runs every 24h] -->|Scan| OLD
    WORKER -->|Skip| CURRENT
    OLD -->|Delete| DELETED[Deleted]
    
    CONFIG[retention_days: 7<br/>check_hours: 24] --> WORKER
    
    style OLD fill:#f44336
    style CURRENT fill:#4CAF50
    style WORKER fill:#9C27B0
    style DELETED fill:#9E9E9E
```

## API Endpoint Organization

```mermaid
graph TD
    API["/api"] --> AUTH["/auth"]
    API --> DEV["/dev"]
    API --> ADMIN["/admin"]
    API --> SYS["/sys"]
    
    AUTH --> REGISTER[POST /register]
    AUTH --> LOGIN[POST /login]
    
    DEV --> CREATE_DEV[POST /]
    DEV --> LIST_DEV[GET /]
    
    DEV --> DEVICE["/&#123;id&#125;"]
    DEVICE --> DATA["/data"]
    DEVICE --> CMD["/cmd"]
    
    DATA --> POST_DATA[POST /]
    DATA --> GET_HIST[GET /history]
    
    CMD --> LIST_CMD[GET /]
    CMD --> SEND_CMD[POST /]
    CMD --> STREAM[GET /stream]
    CMD --> ACK[POST /&#123;cmd_id&#125;/ack]
    
    ADMIN --> USERS["/users"]
    ADMIN --> DEVICES_ADMIN["/dev"]
    ADMIN --> DATABASE["/database"]
    
    style API fill:#2196F3
    style AUTH fill:#9C27B0
    style ADMIN fill:#f44336
    style DEV fill:#4CAF50
```

## Device Lifecycle

```mermaid
stateDiagram-v2
    [*] --> Created: POST /dev
    Created --> Active: First data sent
    Active --> Active: Sending data
    Active --> Suspended: Admin suspends
    Active --> Banned: Rate limit exceeded
    Suspended --> Active: Admin reactivates
    Banned --> Active: Admin unbans
    Active --> Deleted: DELETE /dev/&#123;id&#125;
    Suspended --> Deleted: Admin deletes
    Deleted --> [*]
    
    note right of Active: Normal operation
    note right of Banned: Temporary block
    note right of Suspended: Admin action
```

## User Authentication States

```mermaid
stateDiagram-v2
    [*] --> Registered: POST /auth/register
    Registered --> Active: Email verified*
    Active --> LoggedIn: POST /auth/login
    LoggedIn --> Active: Token expires
    LoggedIn --> LoggedIn: Token refresh
    Active --> Suspended: Admin suspends
    Suspended --> Active: Admin reactivates
    Active --> Deleted: Admin deletes
    Deleted --> [*]
    
    note right of LoggedIn: JWT valid for 24h
    note left of Registered: *Auto-verified in self-hosted
```

## WiFi AP Provisioning Flow

```mermaid
sequenceDiagram
    participant Device as IoT Device
    participant Phone as Mobile App
    participant Server as Datum Server

    Note over Device: Boot without credentials
    Device->>Device: Create WiFi AP<br/>"Datum-Setup-XXXX"
    
    Note over Phone: User connects to device AP
    Phone->>Device: GET /info
    Device-->>Phone: {uid, model, version}
    
    Note over Phone: User names device
    Phone->>Server: POST /dev/register<br/>{uid, name, wifi_creds}
    Server-->>Phone: {device_id, api_key, server_url}
    
    Phone->>Device: POST /configure<br/>{server_url, wifi}
    Device-->>Phone: OK
    
    Note over Device: Restart & connect WiFi
    Device->>Server: POST /prov/activate<br/>{device_uid}
    Server-->>Device: {device_id, api_key}
    
    Note over Device,Server: Normal operation begins
    Device->>Server: POST /dev/{id}/data (with api_key)
```

## Provisioning State Machine

```mermaid
stateDiagram-v2
    [*] --> Unconfigured: Power On
    Unconfigured --> SetupMode: No credentials
    SetupMode --> SetupMode: Waiting for app...
    SetupMode --> Configured: Received config
    SetupMode --> Unconfigured: Timeout
    Configured --> Connecting: Restart
    Connecting --> Activating: WiFi connected
    Connecting --> SetupMode: WiFi failed
    Activating --> Online: Activated
    Activating --> Connecting: Retry
    Online --> Online: Normal operation
    Online --> Offline: Connection lost
    Offline --> Online: Reconnected
    Online --> SetupMode: Factory reset
    
    note right of SetupMode: AP mode active
    note right of Online: API key stored
```

---

## Viewing Diagrams

These diagrams use [Mermaid](https://mermaid.js.org/) syntax. To view them:

1. **GitHub**: Renders automatically in README files
2. **VS Code**: Install "Markdown Preview Mermaid Support" extension
3. **Online**: Use [Mermaid Live Editor](https://mermaid.live/)
4. **Documentation Sites**: Most static site generators support Mermaid

## Related Documentation

- [Storage Architecture](../reference/STORAGE.md)
- [API Reference](../reference/API.md)
- [Deployment Guide](../guides/DEPLOYMENT.md)
- [Security Guide](../guides/SECURITY.md)
- [WiFi Provisioning Guide](../guides/WIFI_PROVISIONING.md)
