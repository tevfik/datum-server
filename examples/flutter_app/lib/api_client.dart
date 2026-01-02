import 'package:dio/dio.dart';

class ApiClient {
  final Dio _dio = Dio();
  String? _token;

  ApiClient() {
    _dio.options.baseUrl = 'http://localhost:8080'; // Default, updated dynamically
    _dio.options.connectTimeout = const Duration(seconds: 5);
    _dio.options.receiveTimeout = const Duration(seconds: 3);
  }

  void setBaseUrl(String url) {
    _dio.options.baseUrl = url;
  }

  void setToken(String token) {
    _token = token;
    _dio.options.headers['Authorization'] = 'Bearer $token';
  }

  void clearToken() {
    _token = null;
    _dio.options.headers.remove('Authorization');
  }

  Future<String> login(String email, String password) async {
    try {
      final response = await _dio.post('/auth/login', data: {
        'email': email,
        'password': password,
      });
      return response.data['token'];
    } catch (e) {
      throw Exception('Login failed: $e');
    }
  }

  Future<Map<String, dynamic>> register(String email, String password) async {
      final response = await _dio.post('/auth/register', data: {
        'email': email,
        'password': password,
      });
      return response.data;
  }

  Future<List<dynamic>> getDevices() async {
    final response = await _dio.get('/devices');
    return response.data['devices'];
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
}
