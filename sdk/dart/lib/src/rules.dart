import 'client.dart';

/// Rules engine API: `/admin/rules/*`. Admin-only.
///
/// Rules are JSON expressions evaluated on incoming telemetry / events;
/// when a rule fires it triggers actions such as MQTT publish, webhook
/// POST, ntfy notification, or device command.
class RulesApi {
  RulesApi(this._c);
  final DatumClient _c;

  Future<List<Map<String, dynamic>>> list() async {
    final res = await _c.request('GET', '/admin/rules');
    if (res is Map && res['rules'] is List) {
      return List<Map<String, dynamic>>.from(
          (res['rules'] as List).map((e) => Map<String, dynamic>.from(e as Map)));
    }
    if (res is List) {
      return List<Map<String, dynamic>>.from(
          res.map((e) => Map<String, dynamic>.from(e as Map)));
    }
    return const [];
  }

  Future<Map<String, dynamic>> get(String id) async =>
      Map<String, dynamic>.from(
          await _c.request('GET', '/admin/rules/$id') as Map);

  /// Create a rule. The `spec` map should follow the rules-engine schema:
  /// ```dart
  /// {
  ///   'name': 'High temperature alert',
  ///   'enabled': true,
  ///   'when': {'topic': 'device/+/data', 'condition': 'data.temp > 30'},
  ///   'then': [{'type': 'notify', 'topic': 'alerts', 'message': 'hot!'}],
  /// }
  /// ```
  Future<Map<String, dynamic>> create(Map<String, dynamic> spec) async =>
      Map<String, dynamic>.from(
          await _c.request('POST', '/admin/rules', body: spec) as Map);

  Future<void> delete(String id) async {
    await _c.request('DELETE', '/admin/rules/$id');
  }

  Future<void> enable(String id) async {
    await _c.request('POST', '/admin/rules/$id/enable');
  }

  Future<void> disable(String id) async {
    await _c.request('POST', '/admin/rules/$id/disable');
  }
}
