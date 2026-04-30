# datum_sdk (Dart/Flutter)

Official Dart SDK for the [Datum IoT platform](../../README.md).

Status: **alpha** — surface area is intentionally minimal and tracks the
server's stable REST endpoints. Realtime support is via Server-Sent Events
on the embedded ntfy-protocol broker. MQTT support will be added once the
auth handshake is finalised.

## Install

```yaml
dependencies:
  datum_sdk:
    path: ../datum-server/sdk/dart
```

## Quick start

```dart
import 'package:datum_sdk/datum_sdk.dart';

Future<void> main() async {
  final datum = DatumClient(baseUrl: 'https://datum.example.com');
  await datum.auth.login(email: 'admin@example.com', password: 'secret');

  final devices = await datum.devices.list();
  final dev = devices.first as Map<String, dynamic>;

  await datum.data.push(dev['id'] as String, {
    'temperature': 22.5,
    'humidity': 60,
  });

  // Realtime subscribe (SSE)
  datum.realtime
      .subscribe('user/${dev['owner_id']}/notifications')
      .listen((msg) => print('event: $msg'));

  // Buckets
  await datum.buckets.create('uploads');
  final url = await datum.buckets.presign('uploads', 'cats/1.jpg',
      method: 'PUT', expiresSecs: 600);
  print('Presigned upload URL: ${url['url']}');
}
```

## Modules

| Module     | Backed by                                     |
|------------|-----------------------------------------------|
| `auth`     | `/auth/*`                                     |
| `devices`  | `/dev/*`, `/api/v1/devices/*`                 |
| `data`     | `/dev/data`                                   |
| `buckets`  | `/storage/*` (Phase 3)                        |
| `notify`   | `/notify/{topic}` (Phase 2)                   |
| `realtime` | `/notify/{topic}/sse`                         |
