// Datum IoT platform — TypeScript SDK.
//
// Usage:
//   import { DatumClient } from "@datum/sdk";
//   const datum = new DatumClient({ baseUrl: "https://datum.example.com" });
//   await datum.auth.login("user@example.com", "password");
//   const devices = await datum.devices.list();
//   const stream  = datum.realtime.subscribe(`device/${devices[0].id}/data`);
//   stream.on(msg => console.log("telemetry", msg));

export { DatumClient } from "./client.js";
export type { DatumClientOptions } from "./client.js";
export { DatumError } from "./errors.js";
