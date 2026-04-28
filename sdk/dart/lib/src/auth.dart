import 'client.dart';

/// Auth API: login, register, logout, token introspection.
class AuthApi {
  AuthApi(this._c);
  final DatumClient _c;

  /// POST /auth/login → {token, user_id, ...}
  Future<Map<String, dynamic>> login({
    required String email,
    required String password,
  }) async {
    final res = await _c.request('POST', '/auth/login',
        body: {'email': email, 'password': password}) as Map;
    final m = Map<String, dynamic>.from(res);
    _c.token = m['token'] as String?;
    _c.userId = m['user_id'] as String?;
    return m;
  }

  /// POST /auth/register → {token, user_id, ...}
  Future<Map<String, dynamic>> register({
    required String email,
    required String password,
  }) async {
    final res = await _c.request('POST', '/auth/register',
        body: {'email': email, 'password': password}) as Map;
    final m = Map<String, dynamic>.from(res);
    _c.token = m['token'] as String?;
    _c.userId = m['user_id'] as String?;
    return m;
  }

  /// Restore a previously saved session (e.g. from secure storage on app launch).
  void setToken({required String token, String? userId}) {
    _c.token = token;
    _c.userId = userId;
  }

  /// POST /auth/logout — clears local credentials regardless of server outcome.
  Future<void> logout() async {
    try {
      await _c.request('POST', '/auth/logout');
    } catch (_) {
      // Best-effort: tokens are still cleared locally below.
    }
    _c.token = null;
    _c.userId = null;
  }

  Future<Map<String, dynamic>> me() async =>
      Map<String, dynamic>.from(await _c.request('GET', '/auth/me') as Map);

  /// GET /auth/oauth/providers → ["google", "github", ...]
  Future<List<String>> oauthProviders() async {
    try {
      final res = await _c.request('GET', '/auth/oauth/providers');
      if (res is List) return res.map((e) => e.toString()).toList();
      return const [];
    } catch (_) {
      return const [];
    }
  }
}
