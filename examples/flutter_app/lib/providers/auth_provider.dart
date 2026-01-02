import 'package:flutter/material.dart';
import 'package:shared_preferences/shared_preferences.dart';
import '../api_client.dart';

class AuthProvider with ChangeNotifier {
  String? _token;
  bool get isAuthenticated => _token != null;

  late final ApiClient _api;

  AuthProvider({ApiClient? apiClient}) {
    _api = apiClient ?? ApiClient();
    _loadToken();
  }

  Future<void> _loadToken() async {
    final prefs = await SharedPreferences.getInstance();
    _token = prefs.getString('auth_token');
    if (_token != null) {
      _api.setToken(_token!);
    }
    notifyListeners();
  }

  Future<bool> login(String email, String password) async {
    try {
      final token = await _api.login(email, password);
      _token = token;
      _api.setToken(token);
      final prefs = await SharedPreferences.getInstance();
      await prefs.setString('auth_token', token);
      notifyListeners();
      return true;
    } catch (e) {
      return false;
    }
  }

  Future<void> logout() async {
    _token = null;
    _api.clearToken();
    final prefs = await SharedPreferences.getInstance();
    await prefs.remove('auth_token');
    notifyListeners();
  }
}
