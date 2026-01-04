import 'package:dio/dio.dart';
import 'package:flutter/foundation.dart';

class DebugLogger {
  static final DebugLogger _instance = DebugLogger._internal();
  factory DebugLogger() => _instance;
  DebugLogger._internal();

  final List<String> logs = [];
  final ValueNotifier<int> logCount = ValueNotifier(0);

  void log(String message) {
    final timestamp = DateTime.now().toIso8601String().split('T').last.substring(0, 8);
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

  ApiClient() {
    _dio.options.baseUrl = 'https://datum.bezg.in'; // Default to production
    _dio.options.connectTimeout = const Duration(seconds: 10);
    _dio.options.receiveTimeout = const Duration(seconds: 10);

    _dio.interceptors.add(InterceptorsWrapper(
      onRequest: (options, handler) {
        _logger.log('REQ: ${options.method} ${options.path}');
        if (options.data != null) _logger.log('DATA: ${options.data}');
        return handler.next(options);
      },
      onResponse: (response, handler) {
        _logger.log('RES: ${response.statusCode} ${response.statusMessage}');
        return handler.next(response);
      },
      onError: (DioException e, handler) {
        _logger.log('ERR: ${e.response?.statusCode} ${e.message}');
        if (e.response != null) _logger.log('RES DATA: ${e.response?.data}');
        return handler.next(e);
      },
    ));
  }

  void setBaseUrl(String url) {
    _dio.options.baseUrl = url;
    _logger.log('Base URL set to: $url');
  }

  void setToken(String token) {
    _dio.options.headers['Authorization'] = 'Bearer $token';
    _logger.log('Auth Token set');
  }

  void clearToken() {
    _dio.options.headers.remove('Authorization');
    _logger.log('Auth Token cleared');
  }

  Future<String> login(String email, String password) async {
    try {
      final response = await _dio.post('/auth/login', data: {
        'email': email,
        'password': password,
      });
      return response.data['token'];
    } catch (e) {
      if (e is DioException && e.response != null) {
          throw Exception('Login failed: ${e.response?.data}');
      }
      throw Exception('Login failed: $e');
    }
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
    final response = await _dio.get('/devices');
    return response.data['devices'] ?? [];
  }

  Future<Map<String, dynamic>> getDevice(String id) async {
    final response = await _dio.get('/devices/$id');
    return response.data;
  }

  Future<Map<String, dynamic>> createProvisioningRequest(
      String uid, String name, String ssid, String pass) async {
    final response = await _dio.post('/devices/register', data: {
      'device_uid': uid,
      'device_name': name,
      'wifi_ssid': ssid,
      'wifi_pass': pass,
    });
    return response.data;
  }

  Future<void> sendCommand(String deviceId, String action, {Map<String, dynamic>? params}) async {
    await _dio.post('/devices/$deviceId/commands', data: {
      'action': action,
      'params': params ?? {},
    });
  }

  Future<void> deleteDevice(String deviceId) async {
    try {
      await _dio.delete('/devices/$deviceId');
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
      // 200 OK is returned even if email not found (security), so this catches actual errors
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
}
