import 'package:flutter_test/flutter_test.dart';
import 'package:datum_app/models/device.dart';

void main() {
  group('Device Model Tests', () {
    test('fromJson creates correct Device object', () {
      final json = {
        'id': '123',
        'device_uid': 'device_abc',
        'name': 'Living Room Camera',
        'type': 'camera',
        'status': 'online',
        'last_seen': '2023-01-01T12:00:00Z',
      };

      final device = Device.fromJson(json);

      expect(device.id, '123');
      expect(device.uid, 'device_abc');
      expect(device.name, 'Living Room Camera');
      expect(device.type, 'camera');
      expect(device.status, 'online');
      expect(device.lastSeen, DateTime.utc(2023, 1, 1, 12, 0, 0));
    });

    test('fromJson handles missing optional fields', () {
      final json = {
        'id': '123',
        'device_uid': 'device_abc',
        // name missing -> default Unknown
        // type missing -> default unknown
        // status missing -> default offline
      };

      final device = Device.fromJson(json);

      expect(device.name, 'Unknown');
      expect(device.type, 'unknown');
      expect(device.status, 'offline');
      expect(device.lastSeen, isNull);
    });
  });
}
