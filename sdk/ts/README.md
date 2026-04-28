# @datum/sdk

Official TypeScript SDK for the [Datum IoT platform](../../README.md).

> Status: **alpha**. Mirrors the server's stable REST surface; realtime via
> the embedded ntfy-protocol SSE broker.

## Install

```bash
# from a workspace folder
npm install --save ../datum-server/sdk/ts
```

## Quick start

```ts
import { DatumClient } from "@datum/sdk";

const datum = new DatumClient({ baseUrl: "https://datum.example.com" });
await datum.auth.login("admin@example.com", "secret");

const devices = await datum.devices.list();
await datum.data.push(devices[0]!.id as string, { temperature: 22.5 });

// Realtime (SSE)
const sub = datum.realtime.subscribe(`user/${devices[0]!.owner_id}/notifications`);
for await (const msg of sub) console.log("event:", msg);
sub.close();

// Buckets
await datum.buckets.create("uploads");
const url = await datum.buckets.presign("uploads", "cats/1.jpg", {
  method: "PUT",
  expires_secs: 600,
});
```

## Modules

| Module     | Endpoints                            |
|------------|--------------------------------------|
| `auth`     | `/auth/*`                            |
| `devices`  | `/dev/*`, `/api/v1/devices/*`        |
| `data`     | `/dev/data`                          |
| `buckets`  | `/storage/*`                         |
| `notify`   | `/notify/{topic}`                    |
| `realtime` | `/notify/{topic}/sse`                |

## Build

```bash
npm install
npm run build
```
