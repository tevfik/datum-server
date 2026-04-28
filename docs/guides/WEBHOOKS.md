# Webhooks (v2) — Signed Delivery & Retry

Webhook v2 adds:

1. **Body signing** — `X-Datum-Signature: sha256=<hex>` over `<timestamp>.<body>`
2. **Replay window** — `X-Datum-Timestamp` (Unix seconds); receivers should
   reject timestamps older than ~5 minutes
3. **Exponential backoff retries** — up to 5 attempts on 5xx / 408 / 429,
   delays 1s, 2s, 4s, 8s with ±25% jitter
4. **Non-retryable failures** — 4xx (other than 408/429) are abandoned
   after the first attempt

## Headers

| Header                | Notes                                              |
|-----------------------|----------------------------------------------------|
| `X-Datum-Event`       | Always `webhook`                                   |
| `X-Datum-Timestamp`   | Unix seconds at send time                          |
| `X-Datum-Signature`   | `sha256=<hex>` HMAC over `<ts>.<body>` keyed by `secret` |
| `X-Webhook-Secret`    | Legacy v1 secret header (still emitted)            |
| `Content-Type`        | `application/json`                                 |

## Verifying a payload (Go)

```go
import "datum-go/internal/webhook"

ts, _ := strconv.ParseInt(r.Header.Get("X-Datum-Timestamp"), 10, 64)
if time.Since(time.Unix(ts, 0)) > 5*time.Minute {
    return // reject (replay protection)
}
body, _ := io.ReadAll(r.Body)
if !webhook.VerifySignature(secret, ts, body, r.Header.Get("X-Datum-Signature")) {
    return // reject (bad signature)
}
```

## Verifying a payload (Node.js)

```js
import crypto from "node:crypto";

const ts = Number(req.headers["x-datum-timestamp"]);
if (Math.abs(Date.now() / 1000 - ts) > 300) return res.status(401).end();

const expected =
  "sha256=" +
  crypto.createHmac("sha256", secret).update(`${ts}.`).update(req.rawBody).digest("hex");

if (
  expected.length !== req.headers["x-datum-signature"].length ||
  !crypto.timingSafeEqual(Buffer.from(expected), Buffer.from(req.headers["x-datum-signature"]))
) {
  return res.status(401).end();
}
```

## Retry timeline

| Attempt | Trigger      | Delay before send |
|---------|--------------|-------------------|
| 1       | original     | 0s                |
| 2       | 5xx/408/429  | ~1s ±25%          |
| 3       | 5xx/408/429  | ~2s ±25%          |
| 4       | 5xx/408/429  | ~4s ±25%          |
| 5       | 5xx/408/429  | ~8s ±25%          |

After 5 attempts the dispatcher logs `webhook: gave up after retries` and
drops the event.

## Subscription model

Subscriptions are managed via the existing `webhook.Dispatcher`:

```go
d.Subscribe(&webhook.Subscription{
    ID:     "alerts-prod",
    URL:    "https://example.com/hooks/datum",
    Secret: "super-secret",
    Events: []webhook.EventType{webhook.EventAlertTriggered, "*"},
})
```
