import 'client.dart';

class DevicesApi {
  DevicesApi(this._c);
  final DatumClient _c;

  Future<List<dynamic>> list() async =>
      (await _c.request('GET', '/dev')) as List<dynamic>? ?? [];

  Future<Map<String, dynamic>> get(String id) async =>
      Map<String, dynamic>.from(await _c.request('GET', '/dev/$id') as Map);

  Future<Map<String, dynamic>> create(Map<String, dynamic> body) async =>
      Map<String, dynamic>.from(await _c.request('POST', '/dev', body: body) as Map);

  Future<void> delete(String id) async {
    await _c.request('DELETE', '/dev/$id');
  }

  Future<Map<String, dynamic>> sendCommand(
    String deviceId,
    String action, {
    Map<String, dynamic>? params,
  }) async =>
      Map<String, dynamic>.from(await _c.request(
        'POST',
        '/api/v1/devices/$deviceId/commands',
        body: {'action': action, if (params != null) 'params': params},
      ) as Map);
}
