import 'package:dio/dio.dart';
import 'package:flutter/foundation.dart';
import 'utils/token_storage.dart';

class DebugLogger {
  static final DebugLogger _instance = DebugLogger._internal();
  factory DebugLogger() => _instance;
  DebugLogger._internal();

  final List<String> logs = [];
  final ValueNotifier<int> logCount = ValueNotifier(0);

  void log(String message) {
    if (kReleaseMode) return;
    final timestamp =
        DateTime.now().toIso8601String().split('T').last.substring(0, 8);
    logs.add('[$timestamp] $message');
    if (logs.length > 100) logs.removeAt(0); // Keep last 100 logs
    logCount.value++;
    debugPrint('[$timestamp] $message');
  }

  void clear() {
    logs.clear();
    logCount.value = 0;
  }
}

class ApiClient {
  final Dio _dio = Dio();
  final DebugLogger _logger = DebugLogger();
  bool _isRefreshing = false;

  VoidCallback? onUnauthorized;

  ApiClient() {
    _dio.options.baseUrl = 'https://datum.bezg.in'; // Default to production
    _dio.options.connectTimeout = const Duration(seconds: 10);
    _dio.options.receiveTimeout = const Duration(seconds: 10);

    _dio.interceptors.add(QueuedInterceptorsWrapper(
      onRequest: (options, handler) async {
        _logger.log('REQ: ${options.method} ${options.path}');
        if (options.data != null) _logger.log('DATA: ${options.data}');

        // Attach Access Token if available
        final token = await TokenStorage.getAccessToken();
        if (token != null) {
          options.headers['Authorization'] = 'Bearer $token';
        }

        return handler.next(options);
      },
      onResponse: (response, handler) {
        _logger.log('RES: ${response.statusCode} ${response.statusMessage}');
        return handler.next(response);
      },
      onError: (DioException e, handler) async {
        _logger.log('ERR: ${e.response?.statusCode} ${e.message}');
        if (e.response != null) _logger.log('RES DATA: ${e.response?.data}');

        // Handle 401 Unauthorized (Token Expiry)
        if (e.response?.statusCode == 401) {
          if (!_isRefreshing) {
            _isRefreshing = true;
            try {
              final refreshed = await _refreshToken();
              _isRefreshing = false;
              if (refreshed) {
                // Retry the original request
                final opts = Options(
                  method: e.requestOptions.method,
                  headers: e.requestOptions.headers,
                );
                // Update header with new token
                final newToken = await TokenStorage.getAccessToken();
                opts.headers?['Authorization'] = 'Bearer $newToken';

                final cloneReq = await _dio.request(
                  e.requestOptions.path,
                  options: opts,
                  data: e.requestOptions.data,
                  queryParameters: e.requestOptions.queryParameters,
                );
                return handler.resolve(cloneReq);
              }
            } catch (refreshErr) {
              _isRefreshing = false;
              _logger.log('Token Refresh Failed: $refreshErr');
            }
          }
          // If refresh failed or not possible, trigger logout
          await TokenStorage.clearTokens();
          onUnauthorized?.call();
        }

        return handler.next(e);
      },
    ));
  }

  Future<bool> _refreshToken() async {
    final refreshToken = await TokenStorage.getRefreshToken();
    if (refreshToken == null) return false;

    try {
      // Create a separate Dio instance to avoid interceptor loop
      final tokenDio = Dio(BaseOptions(baseUrl: _dio.options.baseUrl));
      final response = await tokenDio.post('/auth/refresh', data: {
        'refresh_token': refreshToken,
      });

      if (response.statusCode == 200) {
        final newAccessToken = response.data['token'] ?? response.data['access_token'];
        // Refresh token might be rotated or stay same depending on server policy
        final newRefreshToken =
            response.data['refresh_token'] ?? refreshToken;

        await TokenStorage.saveTokens(newAccessToken, newRefreshToken);
        _logger.log('Token Refreshed Successfully');
        return true;
      }
    } catch (e) {
      _logger.log('Error refreshing token: $e');
    }
    return false;
  }

  void setBaseUrl(String url) {
    _dio.options.baseUrl = url;
    _logger.log('Base URL set to: $url');
  }

  Future<String> login(String email, String password) async {
    try {
      final response = await _dio.post('/auth/login', data: {
        'email': email,
        'password': password,
      });
      // Accept both single token (legacy) and pair
      final accessToken = response.data['token'] ?? response.data['access_token'];
      final refreshToken = response.data['refresh_token'] ?? '';
      
      if (accessToken != null) {
        await TokenStorage.saveTokens(accessToken, refreshToken);
      }
      return accessToken;
    } catch (e) {
      if (e is DioException && e.response != null) {
        throw Exception('Login failed: ${e.response?.data}');
      }
      throw Exception('Login failed: $e');
    }
  }

  Future<void> logout() async {
      await TokenStorage.clearTokens();
  }

  Future<Map<String, dynamic>> register(String email, String password) async {
    try {
      final response = await _dio.post('/auth/register', data: {
        'email': email,
        'password': password,
      });
      return response.data;
    } catch (e) {
      if (e is DioException && e.response != null) {
        throw Exception('Registration failed: ${e.response?.data}');
      }
      throw Exception('Registration failed: $e');
    }
  }

  Future<List<dynamic>> getDevices() async {
    final response = await _dio.get('/dev');
    return response.data['devices'] ?? [];
  }

  Future<Map<String, dynamic>> getDevice(String id) async {
    final response = await _dio.get('/dev/$id');
    return response.data;
  }

  Future<Map<String, dynamic>> getDeviceData(String id) async {
    final response = await _dio.get('/dev/$id/data');
    final Map<String, dynamic> raw = response.data;

    final Map<String, dynamic> flattened =
        Map<String, dynamic>.from(raw['data'] ?? {});
    if (raw.containsKey('timestamp')) {
      flattened['timestamp'] = raw['timestamp'];
    }
    return flattened;
  }

  // Updated to support new Auth Mode and Return API Key
  Future<Map<String, dynamic>> registerDeviceOnServer({
    required String uid,
    required String name,
    required String type,
    String authMode = 'static',
    String? wifiSSID,
    String? wifiPass,
  }) async {
    final data = {
      'device_uid': uid,
      'device_name': name,
      'device_type': type,
      'auth_mode': authMode,
    };
    if (wifiSSID != null) data['wifi_ssid'] = wifiSSID;
    if (wifiPass != null) data['wifi_pass'] = wifiPass;

    final response = await _dio.post('/dev/register', data: data);
    return response.data; // Should return { "api_key": "...", "device_id": "..." }
  }

  // Legacy support
  Future<Map<String, dynamic>> createProvisioningRequest(
      String uid, String name, String ssid, String pass) async {
    return registerDeviceOnServer(
        uid: uid, name: name, type: 'unknown', wifiSSID: ssid, wifiPass: pass);
  }

  Future<void> sendCommand(String deviceId, String action,
      {Map<String, dynamic>? params}) async {
    await _dio.post('/dev/$deviceId/cmd', data: {
      'action': action,
      'params': params ?? {},
    });
  }

  Future<void> deleteDevice(String deviceId) async {
    try {
      await _dio.delete('/dev/$deviceId');
      _logger.log('Device deleted: $deviceId');
    } catch (e) {
      if (e is DioException && e.response != null) {
        throw Exception('Delete failed: ${e.response?.data}');
      }
      throw Exception('Delete failed: $e');
    }
  }

  Future<void> changePassword(String oldPassword, String newPassword) async {
    try {
      await _dio.put('/auth/password', data: {
        'old_password': oldPassword,
        'new_password': newPassword,
      });
      _logger.log('Password changed successfully');
    } catch (e) {
      if (e is DioException && e.response != null) {
        throw Exception('Password change failed: ${e.response?.data}');
      }
      throw Exception('Password change failed: $e');
    }
  }

  Future<void> deleteAccount() async {
    try {
      await _dio.delete('/auth/user');
      _logger.log('Account deleted successfully');
    } catch (e) {
      if (e is DioException && e.response != null) {
        throw Exception('Account deletion failed: ${e.response?.data}');
      }
      throw Exception('Account deletion failed: $e');
    }
  }

  Future<void> forgotPassword(String email) async {
    try {
      await _dio.post('/auth/forgot-password', data: {'email': email});
      _logger.log('Forgot password request sent for: $email');
    } catch (e) {
      if (e is DioException && e.response != null) {
        throw Exception('Request failed: ${e.response?.data}');
      }
      throw Exception('Request failed: $e');
    }
  }

  Future<void> resetPassword(String token, String newPassword) async {
    try {
      await _dio.post('/auth/reset-password', data: {
        'token': token,
        'new_password': newPassword,
      });
      _logger.log('Password reset successfully');
    } catch (e) {
      if (e is DioException && e.response != null) {
        throw Exception('Reset failed: ${e.response?.data}');
      }
      throw Exception('Reset failed: $e');
    }
  }

  // --- API Key Management ---

  Future<List<dynamic>> getApiKeys() async {
    final response = await _dio.get('/auth/keys');
    return response.data['keys'] ?? [];
  }

  Future<Map<String, dynamic>> createApiKey(String name) async {
    final response = await _dio.post('/auth/keys', data: {'name': name});
    return response.data;
  }

  Future<void> deleteApiKey(String id) async {
    await _dio.delete('/auth/keys/$id');
  }
}
