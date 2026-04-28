/// Datum IoT platform Dart/Flutter SDK.
///
/// Quick start:
///
/// ```dart
/// final datum = DatumClient(baseUrl: 'https://datum.example.com');
/// await datum.auth.login(email: 'user@example.com', password: 'secret');
///
/// final devices = await datum.devices.list();
/// final stream = datum.realtime.subscribe('device/${devices.first.id}/data');
/// stream.listen((msg) => print('telemetry: $msg'));
/// ```
library datum_sdk;

export 'src/client.dart';
export 'src/auth.dart';
export 'src/devices.dart';
export 'src/data.dart';
export 'src/buckets.dart';
export 'src/notify.dart';
export 'src/realtime.dart';
export 'src/exceptions.dart';
