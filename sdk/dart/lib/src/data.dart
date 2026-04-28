import 'client.dart';

class DataApi {
  DataApi(this._c);
  final DatumClient _c;

  /// Push a telemetry sample for a device.
  Future<void> push(String deviceId, Map<String, dynamic> sample) async {
    await _c.request('POST', '/dev/data', body: {
      'device_id': deviceId,
      ...sample,
    });
  }

  Future<List<dynamic>> query(String deviceId,
      {DateTime? from, DateTime? to, int? limit}) async {
    final query = <String, dynamic>{
      'device_id': deviceId,
      if (from != null) 'from': from.toUtc().toIso8601String(),
      if (to != null) 'to': to.toUtc().toIso8601String(),
      if (limit != null) 'limit': limit,
    };
    return (await _c.request('GET', '/dev/data', query: query)) as List<dynamic>? ?? [];
  }
}
