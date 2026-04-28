import 'client.dart';

class AuthApi {
  AuthApi(this._c);
  final DatumClient _c;

  Future<Map<String, dynamic>> login({
    required String email,
    required String password,
  }) async {
    final res = await _c.request('POST', '/auth/login',
        body: {'email': email, 'password': password});
    _c.token = res['token'] as String?;
    return Map<String, dynamic>.from(res as Map);
  }

  Future<void> logout() async {
    await _c.request('POST', '/auth/logout');
    _c.token = null;
  }

  Future<Map<String, dynamic>> me() async =>
      Map<String, dynamic>.from(await _c.request('GET', '/auth/me') as Map);
}
