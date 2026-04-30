import 'client.dart';

/// Admin API: covers `/admin/*` operations. Every method requires the
/// authenticated principal to have role `admin`.
///
/// The handler tree groups operations by domain:
///   - [sys]      — `/admin/sys/*` system configuration & logs
///   - [users]    — `/admin/users/*` user management
///   - [devices]  — `/admin/dev/*` cross-tenant device management
///   - [database] — `/admin/database/*` data ops
///   - [mqtt]     — `/admin/mqtt/*` MQTT broker introspection
class AdminApi {
  AdminApi(DatumClient c)
      : sys = AdminSysApi(c),
        users = AdminUsersApi(c),
        devices = AdminDevicesApi(c),
        database = AdminDatabaseApi(c),
        mqtt = AdminMqttApi(c);
  final AdminSysApi sys;
  final AdminUsersApi users;
  final AdminDevicesApi devices;
  final AdminDatabaseApi database;
  final AdminMqttApi mqtt;
}

// ── /admin/sys/* ────────────────────────────────────────────────────────────

class AdminSysApi {
  AdminSysApi(this._c);
  final DatumClient _c;

  /// GET /admin/sys/config — current system configuration.
  Future<Map<String, dynamic>> config() async =>
      Map<String, dynamic>.from(await _c.request('GET', '/admin/sys/config') as Map);

  /// PUT /admin/sys/registration — toggle public sign-up.
  Future<void> setRegistration({required bool allowRegister}) async {
    await _c.request('PUT', '/admin/sys/registration',
        body: {'allow_register': allowRegister});
  }

  /// PUT /admin/sys/retention — data retention policy.
  Future<void> setRetention({
    required int days,
    int? checkIntervalHours,
  }) async {
    await _c.request('PUT', '/admin/sys/retention', body: {
      'days': days,
      if (checkIntervalHours != null) 'check_interval_hours': checkIntervalHours,
    });
  }

  /// PUT /admin/sys/rate-limit — global rate-limiter values.
  Future<void> setRateLimit({
    required int maxRequests,
    required int windowSeconds,
  }) async {
    await _c.request('PUT', '/admin/sys/rate-limit', body: {
      'max_requests': maxRequests,
      'window_seconds': windowSeconds,
    });
  }

  /// PUT /admin/sys/alerts — alert thresholds + email enable.
  Future<void> setAlerts({
    required bool emailEnabled,
    required int diskThreshold,
    required int memoryThreshold,
  }) async {
    await _c.request('PUT', '/admin/sys/alerts', body: {
      'email_enabled': emailEnabled,
      'disk_threshold': diskThreshold,
      'memory_threshold': memoryThreshold,
    });
  }

  /// GET /admin/sys/logs — recent system logs (server caps the count).
  Future<List<Map<String, dynamic>>> logs({String? level, String? search}) async {
    final res = await _c.request('GET', '/admin/sys/logs', query: {
      if (level != null) 'level': level,
      if (search != null) 'search': search,
    });
    if (res is Map && res['logs'] is List) {
      return List<Map<String, dynamic>>.from(
          (res['logs'] as List).map((e) => Map<String, dynamic>.from(e as Map)));
    }
    return const [];
  }

  /// DELETE /admin/sys/logs — clears stored logs.
  Future<void> clearLogs() async {
    await _c.request('DELETE', '/admin/sys/logs');
  }
}

// ── /admin/users/* ─────────────────────────────────────────────────────────

class AdminUsersApi {
  AdminUsersApi(this._c);
  final DatumClient _c;

  /// GET /admin/users — list every user.
  Future<List<Map<String, dynamic>>> list() async {
    final res = await _c.request('GET', '/admin/users');
    if (res is Map && res['users'] is List) {
      return List<Map<String, dynamic>>.from(
          (res['users'] as List).map((e) => Map<String, dynamic>.from(e as Map)));
    }
    if (res is List) {
      return List<Map<String, dynamic>>.from(
          res.map((e) => Map<String, dynamic>.from(e as Map)));
    }
    return const [];
  }

  /// GET /admin/users/:user_id — single user.
  Future<Map<String, dynamic>> get(String userId) async => Map<String, dynamic>.from(
      await _c.request('GET', '/admin/users/$userId') as Map);

  /// PUT /admin/users/:id — change user status (`active` | `suspended`).
  Future<void> setStatus({required String userId, required String status}) async {
    await _c.request('PUT', '/admin/users/$userId', body: {'status': status});
  }

  /// DELETE /admin/users/:id — permanently remove a user.
  Future<void> delete(String userId) async {
    await _c.request('DELETE', '/admin/users/$userId');
  }

  /// POST /admin/users/:username/reset-password — admin-initiated reset.
  Future<Map<String, dynamic>> resetPassword(String username) async =>
      Map<String, dynamic>.from(
          await _c.request('POST', '/admin/users/$username/reset-password') as Map);
}

// ── /admin/dev/* ────────────────────────────────────────────────────────────

class AdminDevicesApi {
  AdminDevicesApi(this._c);
  final DatumClient _c;

  Future<List<Map<String, dynamic>>> list() async {
    final res = await _c.request('GET', '/admin/dev');
    if (res is Map && res['devices'] is List) {
      return List<Map<String, dynamic>>.from(
          (res['devices'] as List).map((e) => Map<String, dynamic>.from(e as Map)));
    }
    if (res is List) {
      return List<Map<String, dynamic>>.from(
          res.map((e) => Map<String, dynamic>.from(e as Map)));
    }
    return const [];
  }

  Future<Map<String, dynamic>> get(String deviceId) async =>
      Map<String, dynamic>.from(
          await _c.request('GET', '/admin/dev/$deviceId') as Map);

  Future<Map<String, dynamic>> provision(Map<String, dynamic> spec) async =>
      Map<String, dynamic>.from(
          await _c.request('POST', '/admin/dev', body: spec) as Map);

  Future<void> update(String deviceId, Map<String, dynamic> patch) async {
    await _c.request('PUT', '/admin/dev/$deviceId', body: patch);
  }

  Future<void> delete(String deviceId) async {
    await _c.request('DELETE', '/admin/dev/$deviceId');
  }

  Future<Map<String, dynamic>> rotateKey(String deviceId) async =>
      Map<String, dynamic>.from(
          await _c.request('POST', '/admin/dev/$deviceId/rotate-key') as Map);

  Future<void> revokeKey(String deviceId) async {
    await _c.request('POST', '/admin/dev/$deviceId/revoke-key');
  }
}

// ── /admin/database/* ───────────────────────────────────────────────────────

class AdminDatabaseApi {
  AdminDatabaseApi(this._c);
  final DatumClient _c;

  Future<Map<String, dynamic>> stats() async => Map<String, dynamic>.from(
      await _c.request('GET', '/admin/database/stats') as Map);

  Future<Map<String, dynamic>> export() async => Map<String, dynamic>.from(
      await _c.request('POST', '/admin/database/export') as Map);

  Future<Map<String, dynamic>> cleanup() async => Map<String, dynamic>.from(
      await _c.request('POST', '/admin/database/cleanup') as Map);

  /// DELETE /admin/database/reset — DESTROYS the entire database. Requires
  /// `{"confirm":"RESET"}` body to actually run server-side.
  Future<void> reset() async {
    await _c.request('DELETE', '/admin/database/reset', body: {'confirm': 'RESET'});
  }
}

// ── /admin/mqtt/* ───────────────────────────────────────────────────────────

class AdminMqttApi {
  AdminMqttApi(this._c);
  final DatumClient _c;

  Future<Map<String, dynamic>> stats() async => Map<String, dynamic>.from(
      await _c.request('GET', '/admin/mqtt/stats') as Map);

  Future<List<Map<String, dynamic>>> clients() async {
    final res = await _c.request('GET', '/admin/mqtt/clients');
    if (res is Map && res['clients'] is List) {
      return List<Map<String, dynamic>>.from(
          (res['clients'] as List).map((e) => Map<String, dynamic>.from(e as Map)));
    }
    return const [];
  }

  Future<void> publish({
    required String topic,
    required String payload,
    int? qos,
    bool? retain,
  }) async {
    await _c.request('POST', '/admin/mqtt/publish', body: {
      'topic': topic,
      'payload': payload,
      if (qos != null) 'qos': qos,
      if (retain != null) 'retain': retain,
    });
  }
}
