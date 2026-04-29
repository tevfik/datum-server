import 'client.dart';

/// Auth API: login, register, logout, sessions, keys, push tokens, password
/// management, and account lifecycle.
///
/// This module mirrors every endpoint under `/auth/*` exposed by datum-server
/// so the SDK can serve as the single source of truth for client apps. The
/// older minimal surface (login/register/logout/me/oauthProviders) is kept
/// unchanged for backward compatibility.
class AuthApi {
  AuthApi(this._c);
  final DatumClient _c;

  // ── Identity ────────────────────────────────────────────────────────────

  /// POST /auth/login → {token, refresh_token, user_id, ...}
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
  ///
  /// Throws DatumException(403) when the server has public registration
  /// disabled. The error body in that case carries a human-readable message
  /// such as "Public registration is disabled. Contact administrator."
  Future<Map<String, dynamic>> register({
    required String email,
    required String password,
    String? name,
  }) async {
    final res = await _c.request(
      'POST',
      '/auth/register',
      body: {
        'email': email,
        'password': password,
        if (name != null) 'name': name,
      },
    ) as Map;
    final m = Map<String, dynamic>.from(res);
    _c.token = m['token'] as String?;
    _c.userId = m['user_id'] as String?;
    return m;
  }

  /// POST /auth/refresh → {token, refresh_token}
  Future<Map<String, dynamic>> refresh(String refreshToken) async {
    final res = await _c.request('POST', '/auth/refresh',
        body: {'refresh_token': refreshToken}) as Map;
    final m = Map<String, dynamic>.from(res);
    if (m['token'] is String) _c.token = m['token'] as String;
    return m;
  }

  /// Restore a previously saved session (e.g. from secure storage on launch).
  void setToken({required String token, String? userId}) {
    _c.token = token;
    _c.userId = userId;
  }

  /// POST /auth/logout — revokes the current JWT (jti) server-side and
  /// clears local credentials. Best-effort: tokens are cleared even if the
  /// server call fails (offline, 401, etc.).
  Future<void> logout() async {
    try {
      await _c.request('POST', '/auth/logout');
    } catch (_) {
      // Defense-in-depth: clear local state regardless.
    }
    _c.token = null;
    _c.userId = null;
  }

  /// GET /auth/me — current user profile.
  Future<Map<String, dynamic>> me() async =>
      Map<String, dynamic>.from(await _c.request('GET', '/auth/me') as Map);

  /// PUT /auth/me — update the current user's display name.
  Future<Map<String, dynamic>> updateProfile({required String displayName}) async =>
      Map<String, dynamic>.from(await _c.request('PUT', '/auth/me',
          body: {'display_name': displayName}) as Map);

  // ── Password ────────────────────────────────────────────────────────────

  /// PUT /auth/password — change password (requires current password). The
  /// server revokes all OTHER sessions on success, keeping only the caller's
  /// session alive.
  Future<void> changePassword({
    required String oldPassword,
    required String newPassword,
  }) async {
    await _c.request('PUT', '/auth/password', body: {
      'old_password': oldPassword,
      'new_password': newPassword,
    });
  }

  /// POST /auth/forgot-password — request a password-reset email.
  Future<void> forgotPassword(String email) async {
    await _c.request('POST', '/auth/forgot-password', body: {'email': email});
  }

  /// POST /auth/reset-password — complete a password reset using the token
  /// delivered by [forgotPassword].
  Future<void> resetPassword({
    required String token,
    required String newPassword,
  }) async {
    await _c.request('POST', '/auth/reset-password', body: {
      'token': token,
      'new_password': newPassword,
    });
  }

  // ── Sessions ────────────────────────────────────────────────────────────

  /// GET /auth/sessions — list all active sessions for the current user.
  Future<List<Map<String, dynamic>>> sessions() async {
    final res = await _c.request('GET', '/auth/sessions');
    if (res is Map && res['sessions'] is List) {
      return List<Map<String, dynamic>>.from(
          (res['sessions'] as List).map((e) => Map<String, dynamic>.from(e as Map)));
    }
    return const [];
  }

  /// DELETE /auth/sessions/:jti — revoke a single session by its JTI claim.
  Future<void> revokeSession(String jti) async {
    await _c.request('DELETE', '/auth/sessions/$jti');
  }

  // ── API keys ────────────────────────────────────────────────────────────

  /// GET /auth/keys — list the current user's personal API keys.
  Future<List<Map<String, dynamic>>> keys() async {
    final res = await _c.request('GET', '/auth/keys');
    if (res is Map && res['keys'] is List) {
      return List<Map<String, dynamic>>.from(
          (res['keys'] as List).map((e) => Map<String, dynamic>.from(e as Map)));
    }
    return const [];
  }

  /// POST /auth/keys — create a personal API key. The plaintext key is only
  /// returned on creation; the caller must persist it.
  Future<Map<String, dynamic>> createKey({required String name}) async =>
      Map<String, dynamic>.from(
          await _c.request('POST', '/auth/keys', body: {'name': name}) as Map);

  /// DELETE /auth/keys/:id — revoke a personal API key.
  Future<void> deleteKey(String id) async {
    await _c.request('DELETE', '/auth/keys/$id');
  }

  // ── Push notification tokens ────────────────────────────────────────────

  /// GET /auth/push-tokens — list registered push-notification tokens.
  Future<List<Map<String, dynamic>>> pushTokens() async {
    final res = await _c.request('GET', '/auth/push-tokens');
    if (res is Map && res['tokens'] is List) {
      return List<Map<String, dynamic>>.from(
          (res['tokens'] as List).map((e) => Map<String, dynamic>.from(e as Map)));
    }
    return const [];
  }

  /// POST /auth/push-token — register a platform-specific push token.
  /// `platform` is one of `android`, `ios`, `web`.
  Future<Map<String, dynamic>> registerPushToken({
    required String platform,
    required String token,
  }) async =>
      Map<String, dynamic>.from(
          await _c.request('POST', '/auth/push-token', body: {
        'platform': platform,
        'token': token,
      }) as Map);

  /// DELETE /auth/push-token/:id — unregister a previously-registered token.
  Future<void> deletePushToken(String id) async {
    await _c.request('DELETE', '/auth/push-token/$id');
  }

  // ── Account lifecycle ───────────────────────────────────────────────────

  /// DELETE /auth/user — permanently delete the current user's account. The
  /// server revokes every session and removes the user record. Local
  /// credentials are cleared on success, regardless of the server's response.
  Future<void> deleteAccount() async {
    try {
      await _c.request('DELETE', '/auth/user');
    } finally {
      _c.token = null;
      _c.userId = null;
    }
  }

  // ── OAuth ───────────────────────────────────────────────────────────────

  /// GET /auth/providers → ["google", "github", ...]
  ///
  /// Older deployments expose the same list at /auth/oauth/providers. Both
  /// are tried so the SDK keeps working across versions.
  Future<List<String>> oauthProviders() async {
    for (final path in const ['/auth/providers', '/auth/oauth/providers']) {
      try {
        final res = await _c.request('GET', path);
        if (res is List) return res.map((e) => e.toString()).toList();
        if (res is Map && res['providers'] is List) {
          return (res['providers'] as List).map((e) => e.toString()).toList();
        }
      } catch (_) {
        continue;
      }
    }
    return const [];
  }
}
