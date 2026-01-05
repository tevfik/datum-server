# SSE Long Polling for Commands

Battery-optimized command delivery for IoT devices.

## Endpoints

### 1. Immediate Poll
```http
GET /devices/{device_id}/commands
Authorization: Bearer {api_key}
```
Returns pending commands immediately.

### 2. HTTP Long Poll
```http
GET /devices/{device_id}/commands/poll?wait=30
Authorization: Bearer {api_key}
```
Waits up to 30 seconds for commands. Returns immediately if command arrives.

### 3. SSE Stream
```http
GET /devices/{device_id}/commands/stream?wait=30
Authorization: Bearer {api_key}
Accept: text/event-stream
```
Server-Sent Events stream. Sends keepalives and commands.

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
1. Device sends data → POST /data/{id}
   Response: {"status":"ok", "commands_pending": 1}

2. If commands_pending > 0:
   → GET /devices/{id}/commands/poll?wait=30
   
3. Execute command

4. Acknowledge:
   → POST /devices/{id}/commands/{cmd_id}/ack
   
5. Sleep, repeat
```

## Test
```bash
./tests/sse_test.sh
```
