import 'client.dart';

/// System info & metrics API: covers the public `/sys/*` and `/health`
/// endpoints that any authenticated (or anonymous) caller can query.
///
/// Admin-only system configuration lives on [AdminApi.sys] in admin.dart.
class SysApi {
  SysApi(this._c);
  final DatumClient _c;

  /// GET /health — quick liveness check.
  Future<Map<String, dynamic>> health() async =>
      Map<String, dynamic>.from(await _c.request('GET', '/health') as Map);

  /// GET /sys/info — server version, build date, runtime details.
  Future<Map<String, dynamic>> info() async =>
      Map<String, dynamic>.from(await _c.request('GET', '/sys/info') as Map);

  /// GET /sys/time — current server time in unix/iso formats.
  Future<Map<String, dynamic>> time() async =>
      Map<String, dynamic>.from(await _c.request('GET', '/sys/time') as Map);

  /// GET /sys/ip — caller's public IP, honoring proxy headers.
  /// Returns the bare string when the server replies in plain-text mode.
  Future<String> ip() async {
    final res = await _c.request('GET', '/sys/ip');
    if (res is Map && res['ip'] is String) return res['ip'] as String;
    return res?.toString() ?? '';
  }

  /// GET /sys/status — initialization & setup state.
  Future<Map<String, dynamic>> status() async =>
      Map<String, dynamic>.from(await _c.request('GET', '/sys/status') as Map);

  /// GET /sys/metrics — runtime metrics (Prometheus-style JSON wrapper).
  Future<Map<String, dynamic>> metrics() async =>
      Map<String, dynamic>.from(await _c.request('GET', '/sys/metrics') as Map);
}
