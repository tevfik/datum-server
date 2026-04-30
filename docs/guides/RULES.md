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

## Persistence

Rules live at `data/rules.json` and are reloaded on server startup. Use
`/admin/rules` (admin auth required) to CRUD them at runtime — changes are
applied immediately and persisted to disk.

```bash
# List
curl -H "Authorization: Bearer $TOKEN" $SERVER/admin/rules

# Create
curl -H "Authorization: Bearer $TOKEN" -H "Content-Type: application/json" \
     -X POST $SERVER/admin/rules \
     -d '{"id":"low-batt","field":"battery","operator":"<","value":15,
          "actions":[{"type":"notify","channels":["ntfy"],"title":"Low battery"}]}'
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
