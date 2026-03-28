# SSE Long Polling for Commands

Battery-optimized command delivery for IoT devices.

## Endpoints

### 1. Immediate Poll
```http
GET /dev/{device_id}/cmd
Authorization: Bearer {api_key}
```
Returns pending commands immediately.

### 2. HTTP Long Poll
```http
GET /dev/{device_id}/cmd/poll?wait=30
Authorization: Bearer {api_key}
```
Waits up to 30 seconds for commands. Returns immediately if command arrives.

### 3. SSE Stream
```http
GET /dev/{device_id}/cmd/stream?wait=30
Authorization: Bearer {api_key}
Accept: text/event-stream
```
Server-Sent Events stream. The server polls for pending commands every **3 seconds** and sends keepalives and commands as they arrive.

## Response Format

**HTTP Poll:**
```json
{"commands": [{"command_id": "cmd_xxx", "action": "reboot", "params": {}}]}
```

**SSE Stream:**
```
event:keepalive
data:{"time":"2025-12-22T20:39:31Z"}

event:command
data:[{"command_id":"cmd_xxx","action":"reboot"}]
```

## Battery-Optimized Flow

```
1. Device sends data → POST /dev/{id}/data
   Response: {"status":"ok", "commands_pending": 1}

2. If commands_pending > 0:
   → GET /dev/{id}/cmd/poll?wait=30
   
3. Execute command

4. Acknowledge:
   → POST /dev/{id}/cmd/{cmd_id}/ack
   
5. Sleep, repeat
```

## Test
```bash
./tests/sse_test.sh
```
