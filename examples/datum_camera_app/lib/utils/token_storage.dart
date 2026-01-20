import 'package:flutter_secure_storage/flutter_secure_storage.dart';

class TokenStorage {
  static const _storage = FlutterSecureStorage();
  static const _kAccessToken = 'access_token';
  static const _kRefreshToken = 'refresh_token';

  static Future<void> saveTokens(
      String accessToken, String refreshToken) async {
    await _storage.write(key: _kAccessToken, value: accessToken);
    await _storage.write(key: _kRefreshToken, value: refreshToken);
  }

  static Future<String?> getAccessToken() async {
    return await _storage.read(key: _kAccessToken);
  }

  static Future<String?> getRefreshToken() async {
    return await _storage.read(key: _kRefreshToken);
  }

  static Future<void> clearTokens() async {
    await _storage.delete(key: _kAccessToken);
    await _storage.delete(key: _kRefreshToken);
  }

  static Future<bool> hasToken() async {
    final token = await getAccessToken();
    return token != null;
  }
}
