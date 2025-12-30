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
        SSE[SSE Handler<br/>Real-time Events]
        AUTH[Auth Middleware<br/>JWT + API Keys]
        
        subgraph "Storage Layer"
            BUNT[(BuntDB<br/>Metadata)]
            TS[(TSStorage<br/>Time-Series)]
        end
        
        RET[Retention Worker<br/>Background Cleanup]
    end

    subgraph "Clients"
        WEB[Web Dashboard]
        CLI[datumctl CLI]
        APP[Mobile App]
    end

    D1 -->|POST /data| API
    D2 -->|POST /data| API
    D3 -->|GET /push| API
    
    D1 -->|SSE| SSE
    D2 -->|Long Poll| API
    
    API --> AUTH
    AUTH --> BUNT
    AUTH --> TS
    
    SSE --> BUNT
    
    RET -->|Cleanup| TS
    
    WEB -->|JWT Auth| API
    CLI -->|JWT Auth| API
    APP -->|JWT Auth| API
    
    style API fill:#2196F3
    style BUNT fill:#4CAF50
    style TS fill:#FF9800
    style AUTH fill:#9C27B0
```

## Data Flow: Device to Storage

```mermaid
sequenceDiagram
    participant Device
    participant API as REST API
    participant Auth as Auth Middleware
    participant BuntDB
    participant TSStorage
    
    Device->>API: POST /data/{device_id}<br/>Bearer: API_KEY
    API->>Auth: Validate API Key
    Auth->>BuntDB: Lookup device by API key
    BuntDB-->>Auth: Device info
    
    alt Valid Device
        Auth-->>API: Device authenticated
        API->>TSStorage: Store data point
        TSStorage-->>API: Success
        API-->>Device: 200 OK<br/>{timestamp, commands_pending}
    else Invalid Key
        Auth-->>API: 401 Unauthorized
        API-->>Device: 401 Invalid API key
    else Banned Device
        Auth-->>API: 403 Forbidden
        API-->>Device: 403 Device banned
    end
```

## Data Flow: User Authentication

```mermaid
sequenceDiagram
    participant User
    participant API as REST API
    participant Auth as Auth Module
    participant BuntDB
    
    User->>API: POST /auth/login<br/>{email, password}
    API->>Auth: Validate credentials
    Auth->>BuntDB: Get user by email
    BuntDB-->>Auth: User record
    
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
    subgraph "BuntDB (Metadata)"
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

## Command Flow: SSE Real-time Commands

```mermaid
sequenceDiagram
    participant Admin as Admin/User
    participant API as REST API
    participant BuntDB
    participant SSE as SSE Handler
    participant Device
    
    Note over Device,SSE: Device maintains SSE connection
    Device->>SSE: GET /device/{id}/commands/stream
    SSE-->>Device: SSE Connection Open
    
    Admin->>API: POST /devices/{id}/commands<br/>{action: "reboot"}
    API->>BuntDB: Store command
    BuntDB-->>API: Command ID
    API-->>Admin: 201 Command queued
    
    Note over SSE,Device: Command delivered in real-time
    SSE->>Device: event: command<br/>data: {id, action: "reboot"}
    
    Device->>API: POST /device/&#123;id&#125;/commands/&#123;cmd_id&#125;/ack<br/>{status: "success"}
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
            SERVER[Datum Server<br/>:8080]
            DATA[(Data Volume<br/>/app/data)]
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
    API --> DEVICES["/devices"]
    API --> DATA["/data"]
    API --> DEVICE["/device"]
    API --> PUBLIC["/public"]
    API --> ADMIN["/admin"]
    API --> SYSTEM["/system"]
    
    AUTH --> REGISTER[POST /register]
    AUTH --> LOGIN[POST /login]
    
    DEVICES --> CREATE_DEV[POST /]
    DEVICES --> LIST_DEV[GET /]
    DEVICES --> DELETE_DEV[DELETE /&#123;id&#125;]

    DATA --> POST_DATA[POST /&#123;id&#125;]
    DATA --> GET_LATEST[GET /&#123;id&#125;]
    DATA --> GET_HISTORY[GET /&#123;id&#125;/history]
    
    DEVICE --> COMMANDS["/commands"]
    DEVICE --> PUSH["GET /&#123;id&#125;/push"]
    
    COMMANDS --> POLL[GET /poll]
    COMMANDS --> STREAM[GET /stream]
    COMMANDS --> ACK[POST /&#123;cmd_id&#125;/ack]
    
    ADMIN --> USERS[/users]
    ADMIN --> DEVICES_ADMIN[/devices]
    ADMIN --> DATABASE[/database]
    
    style API fill:#2196F3
    style AUTH fill:#9C27B0
    style ADMIN fill:#f44336
    style PUBLIC fill:#4CAF50
```

## Device Lifecycle

```mermaid
stateDiagram-v2
    [*] --> Created: POST /devices
    Created --> Active: First data sent
    Active --> Active: Sending data
    Active --> Suspended: Admin suspends
    Active --> Banned: Rate limit exceeded
    Suspended --> Active: Admin reactivates
    Banned --> Active: Admin unbans
    Active --> Deleted: DELETE /devices/&#123;id&#125;
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
    Phone->>Server: POST /devices/register<br/>{uid, name, wifi_creds}
    Server-->>Phone: {device_id, api_key, server_url}
    
    Phone->>Device: POST /configure<br/>{server_url, wifi}
    Device-->>Phone: OK
    
    Note over Device: Restart & connect WiFi
    Device->>Server: POST /provisioning/activate<br/>{device_uid}
    Server-->>Device: {device_id, api_key}
    
    Note over Device,Server: Normal operation begins
    Device->>Server: POST /data (with api_key)
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
