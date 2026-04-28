# Datum SDKs

Client libraries for the Datum platform live here, side-by-side with the
server they target.

| SDK            | Language       | Status   | Path           |
|----------------|----------------|----------|----------------|
| `datum_sdk`    | Dart / Flutter | alpha    | [`dart/`](dart) |
| `@datum/sdk`   | TypeScript     | alpha    | [`ts/`](ts)     |

Both SDKs cover:

- `auth` (login/logout/me)
- `devices` (CRUD, send command)
- `data` (push & query telemetry)
- `buckets` (object storage)
- `notify` (publish to embedded ntfy-protocol broker)
- `realtime` (SSE subscription to any topic)

The TypeScript SDK is intended for the React frontend and any Node service.
The Dart SDK is intended for the planned Flutter mobile client.
