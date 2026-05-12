# Rule Engine

> Reference for `internal/rules`, the alert/automation engine wired into
> `/admin/rules` and persisted as `data/rules.json`.

A *rule* watches incoming telemetry / events and triggers one or more
*actions* (notification, MQTT publish, webhook). Rules are evaluated
in-process by the rule engine immediately after telemetry hits the
`processing.TelemetryProcessor`.

## Rule shape

```json
{
  "id": "high-temp",
  "name": "High temperature alert",
  "enabled": true,
  "device_filter": "*",
  "field": "temperature",
  "operator": ">",
  "value": 80,
  "actions": [
    { "type": "notify", "channels": ["inapp", "ntfy"], "title": "High temp", "message": "Temp {{value}}°C on {{device_id}}" },
    { "type": "mqtt", "topic": "alert/{{device_id}}/high-temp", "payload": "{{value}}" },
    { "type": "webhook", "id": "alerts-prod" }
  ],
  "throttle_seconds": 60
}
```

| Field             | Type    | Notes                                                                |
|-------------------|---------|----------------------------------------------------------------------|
| `id`              | string  | unique per rule                                                      |
| `name`            | string  | human label                                                          |
| `enabled`         | bool    | rules with `false` are loaded but skipped                            |
| `device_filter`   | string  | glob (`*`, `room-*`) matched against incoming `device_id`            |
| `field`           | string  | telemetry key (`temperature`, `humidity`, `state.online`, …)         |
| `operator`        | string  | `>`, `<`, `>=`, `<=`, `==`, `!=`                                     |
| `value`           | any     | comparison RHS                                                       |
| `actions`         | array   | see below                                                            |
| `throttle_seconds`| int     | minimum seconds between consecutive triggers                         |

## Action types

### `notify`

Routes through the multi-channel dispatcher (Phase 2). See
[NOTIFICATIONS.md](NOTIFICATIONS.md).

```json
{ "type": "notify", "channels": ["inapp", "ntfy"], "title": "...", "message": "..." }
```

### `mqtt`

Publishes through the embedded broker. Topic and payload may use the
`{{device_id}}`, `{{value}}`, `{{field}}` template tokens.

```json
{ "type": "mqtt", "topic": "alert/{{device_id}}", "payload": "{{value}}" }
```

### `webhook`

Re-emits a webhook event through `webhook.Dispatcher`. Either reference an
existing subscription `id` or pass `url` + `secret` inline.

```json
{ "type": "webhook", "id": "alerts-prod" }
```

## Persistence & Multi-User Support

Rules are no longer just static files. They are managed in two ways:

1. **System Rules (`data/rules.json`)**: Legacy/Bootstrap rules loaded on startup. Read-only at runtime.
2. **User-Defined Rules (Database)**: Rules created by users are stored in the **Document Store** (under the `rules` collection). These are:
   - Persisted across restarts.
   - Associated with an `owner_id`.
   - Manageable via user-level API endpoints.

## API Management

### User Endpoints (`/api/v1/rules`)
Users can manage rules for their own devices. The system automatically verifies device ownership.

```bash
# List my rules
curl -H "Authorization: Bearer $USER_TOKEN" $SERVER/api/v1/rules

# Create a rule for my device
curl -H "Authorization: Bearer $USER_TOKEN" -H "Content-Type: application/json" \
     -X POST $SERVER/api/v1/rules \
     -d '{"name":"Living Room Heat","device_id":"dev_123",
          "conditions":[{"field":"temperature","operator":"gt","value":28}],
          "actions":[{"type":"log"}]}'
```

### Admin Endpoints (`/api/v1/admin/rules`)
Admins can see and manage rules across all users.

```bash
# List all rules globally
curl -H "Authorization: Bearer $ADMIN_TOKEN" $SERVER/api/v1/admin/rules
```

## Template tokens

Available inside action `title`, `message`, `topic`, `payload`:

| Token            | Resolves to                                |
|------------------|--------------------------------------------|
| `{{device_id}}`  | device that produced the sample            |
| `{{field}}`      | rule's `field`                             |
| `{{value}}`      | numeric/string value that triggered it     |
| `{{operator}}`   | comparison operator                        |
| `{{rule_id}}`    | rule id                                    |
| `{{ts}}`         | RFC3339 timestamp                          |

## Throttling

Each rule maintains a `last_triggered` timestamp. If a new sample matches
within `throttle_seconds`, actions are suppressed but the match is still
counted in metrics.

## Testing rules locally

```bash
# Push a high-temperature sample as a device:
curl -X POST $SERVER/dev/data \
     -H "Authorization: Bearer $DEVICE_KEY" \
     -H "Content-Type: application/json" \
     -d '{"device_id":"dev-1","temperature":95}'
```

Watch the logs (`make logs SERVICE=datum-server | grep rule`) to confirm
the rule fired and the action was dispatched.
