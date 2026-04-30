# Notifications

Datum-server's notification subsystem is **multi-channel by design** and
intentionally **independent of any third-party push provider** (no FCM, no
APNs in the default configuration). Every notification is fanned out to one
or more *Channels* and each Channel implements a single delivery transport.

## Architecture

```
                 ┌────────────────────────────────────────────┐
   notify event  │                                            │
 ────────────────►   notify.Dispatcher                        │
                 │     ├── inapp        (SSE / MQTT command)  │
                 │     ├── ntfy         (external ntfy server)│
                 │     ├── ntfy-embedded(in-process pub/sub)  │
                 │     └── webpush      (VAPID, Phase 5)      │
                 └────────────────────────────────────────────┘
```

Each channel runs in its own goroutine with a 10 s context deadline.
Channels never block one another, and a failing channel is logged but does
not stop delivery to the other channels.

## Built-in Channels

### `inapp`
Looks up every device the user owns whose `type == "mobile"` and creates a
pending `notify` command. Mobile clients pick it up via their existing
SSE (`GET /dev/{id}/cmd/stream`) or MQTT command stream.

This is the channel mobile apps see while they are open or in normal
background mode.

### `ntfy`
Forwards the notification to an **external** ntfy server (e.g. `ntfy.sh` or
your self-hosted instance). Configure with:

```env
NTFY_URL=https://ntfy.example.com
NTFY_TOKEN=optional-bearer-token
```

If `NTFY_URL` is unset, this channel is not registered and dispatch silently
skips it.

### `ntfy-embedded`
A tiny **in-process pub/sub** broker that speaks the [ntfy.sh wire
protocol](https://docs.ntfy.sh/publish/). It is mounted at `POST/GET
/notify/{topic}` so any existing ntfy mobile/desktop client can subscribe to
topics served directly by datum-server without running a separate ntfy
daemon.

Supported endpoints:

| Method | Path                       | Description                                     |
|--------|----------------------------|-------------------------------------------------|
| POST   | `/notify/{topic}`          | Publish a message (text/plain body)             |
| GET    | `/notify/{topic}/json`     | Newline-delimited JSON stream (long-poll)       |
| GET    | `/notify/{topic}/sse`      | Server-Sent Events stream                       |
| GET    | `/notify/{topic}/raw`      | Plain-text body lines, one per message          |

Supported request headers when publishing (HTTP & ntfy-equivalent both
accepted): `Title`/`X-Title`, `Priority`/`X-Priority`,
`Tags`/`X-Tags` (comma-separated).

> ⚠️ The embedded broker has **no authentication** in this release. Bind the
> server to localhost or front it with a reverse proxy until the policy
> engine arrives in Phase 5.

### `webpush` (preview)
Stub for [VAPID Web Push](https://datatracker.ietf.org/doc/html/rfc8030) so
browsers can receive notifications without FCM. Registered automatically
when both `WEBPUSH_VAPID_PUBLIC` and `WEBPUSH_VAPID_PRIVATE` are configured.
Actual delivery wiring lands in Phase 5.

## Configuration

`internal/config.NotifyConfig`:

| YAML / Env                            | Default                      | Purpose                       |
|---------------------------------------|------------------------------|-------------------------------|
| `notify.default_channels`             | `[inapp, ntfy]`              | Fired when a Notification has no explicit channel list |
| `NTFY_SERVER_URL` / `notify.ntfy_server_url` | (unset)              | Public URL of an ntfy server (external)         |
| `WEBPUSH_VAPID_PUBLIC`                | (unset)                      | VAPID public key for web push                   |
| `WEBPUSH_VAPID_PRIVATE`               | (unset)                      | VAPID private key                               |
| `WEBPUSH_SUBJECT`                     | (unset)                      | mailto:/URL identifier for VAPID JWT            |

## Choosing channels per-notification

```go
dispatcher.Dispatch(notify.Notification{
    UserID:   "usr_abc123",
    Title:    "Pump tripped",
    Message:  "Sensor #4 went above 80°C",
    Priority: notify.PriorityHigh,
    Channels: []string{"inapp", "ntfy-embedded"},
    Tags: map[string]string{
        "click_url": "https://app.example.com/devices/dev_42",
    },
})
```

Omit `Channels` to use the dispatcher's default set.

## Why not FCM / APNs?

- **FCM** ties you to a Google account, leaks all of your message metadata
  to Google, and adds a runtime dependency that hurts self-hosting.
- **APNs** requires an Apple Developer Program enrolment and per-app
  certificates — fine for a polished consumer product but heavy for IoT
  operators.

By giving every mobile app a long-lived MQTT/SSE channel and a per-user
ntfy topic, datum-server can deliver real-time notifications **end-to-end
without either provider**. iOS background-restriction trade-offs are
documented in `docs/PROJECT.md`; an opt-in APNs bridge is on the Phase
roadmap for users who do need consumer-grade silent push.

## Testing

`internal/notify/notify_test.go` covers:

- Dispatcher fan-out to multiple channels
- Per-notification channel override beats default channels
- Embedded broker pub/sub correctness
- HTTP `/notify/{topic}` publish + `/notify/{topic}/json` stream round-trip
- Priority parsing (`min`/`low`/`default`/`high`/`max`)
