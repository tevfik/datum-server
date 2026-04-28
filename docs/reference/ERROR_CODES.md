# Datum Server — HTTP Error Reference

> Convention: every JSON error body looks like
>
> ```json
> {
>   "error":      "human readable message",
>   "code":       "MACHINE_CODE",   // optional, future use
>   "request_id": "abc123…"          // matches X-Request-ID
> }
> ```
>
> Only `error` is currently guaranteed. `code` and `request_id` will become
> stable once we finish migrating handlers.

## Status codes by category

### 4xx — client errors

| Status | Typical cause                                                              | Example endpoint                              |
|--------|----------------------------------------------------------------------------|------------------------------------------------|
| 400    | Malformed JSON, missing required field, validation failed                  | `POST /auth/login` with empty `email`         |
| 401    | Missing or invalid Authorization header                                    | Any authenticated endpoint                     |
| 403    | Authenticated but not allowed (admin-only, ownership mismatch, RLS)       | `DELETE /admin/dev/{id}`                      |
| 404    | Resource not found                                                         | `GET /dev/{id}` for unknown ID                |
| 408    | Request timed out (server-side)                                            | Long upload exceeding `max_request_body_bytes`|
| 409    | Conflict (duplicate ID, idempotency conflict)                              | `POST /admin/users` with existing email       |
| 413    | Payload too large                                                          | Bucket `PUT` exceeding `buckets.max_object_mb`|
| 415    | Unsupported `Content-Type`                                                 | `POST /dev/data` with `text/xml`              |
| 422    | Validated but semantically invalid (e.g. bad timeseries window)            | `GET /dev/data?from=2099`                     |
| 429    | Rate limit exceeded                                                        | Any rate-limited route                         |

### 5xx — server errors

| Status | Meaning                                                       |
|--------|---------------------------------------------------------------|
| 500    | Unhandled exception (also logged; matches X-Request-ID)       |
| 502    | Upstream failure (e.g. external ntfy server unreachable)      |
| 503    | Storage layer unavailable (DB or buntdb closed)               |
| 504    | Backend timeout (Postgres slow query > deadline)              |

## Stable response components

These are reusable error responses defined in `cmd/server/openapi.yaml`:

| Name                  | HTTP | Notes                              |
|-----------------------|------|------------------------------------|
| `BadRequest`          | 400  | Invalid input                      |
| `Unauthorized`        | 401  | Missing / bad auth                 |
| `Forbidden`           | 403  | Authenticated but not allowed      |
| `NotFound`            | 404  | Resource not found                 |
| `Conflict`            | 409  | State conflict                     |
| `RateLimited`         | 429  | Throttled                          |
| `InternalServerError` | 500  | Server bug                         |
| `BadGateway`          | 502  | Upstream issue                     |

## Common patterns

### Authentication failed

```http
401 Unauthorized
Content-Type: application/json

{ "error": "missing or invalid token" }
```

### Validation failed

```http
400 Bad Request
Content-Type: application/json

{ "error": "email is required" }
```

### Not found

```http
404 Not Found
Content-Type: application/json

{ "error": "device not found" }
```

### Bucket presigned URL expired or invalid

When the presigned signature in `?sig=` does not verify, the server falls
back to the standard auth middleware:

```http
401 Unauthorized
Content-Type: application/json

{ "error": "missing or invalid token" }
```

(The presigned URL was treated as anonymous and then no auth was supplied.)

## Webhook v2 receivers

Receivers should reply with `2xx` to ack delivery. Returning `408` or `429`
will trigger up to 4 retries (5 total attempts) with exponential backoff.
Returning any other 4xx aborts retries — see [WEBHOOKS.md](../guides/WEBHOOKS.md).
