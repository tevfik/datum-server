import 'package:riverpod_annotation/riverpod_annotation.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'api_provider.dart';

part 'auth_provider.g.dart';

@Riverpod(keepAlive: true)
class Auth extends _$Auth {
  @override
  Future<String?> build() async {
    final prefs = await SharedPreferences.getInstance();
    final token = prefs.getString('auth_token');

    if (token != null) {
      ref.read(apiClientProvider).setToken(token);
    } else {
      ref.read(apiClientProvider).clearToken();
    }

    return token;
  }

  bool get isAuthenticated => state.value != null;

  Future<bool> login(String email, String password,
      {bool rememberMe = false}) async {
    final api = ref.read(apiClientProvider);
    state = const AsyncValue.loading();
    try {
      final token = await api.login(email, password);
      api.setToken(token);

      final prefs = await SharedPreferences.getInstance();
      if (rememberMe) {
        await prefs.setString('auth_token', token);
      } else {
        await prefs.remove('auth_token'); // Don't persist if not asked
      }

      state = AsyncValue.data(token);
      return true;
    } catch (e, st) {
      state = AsyncValue.error(e, st);
      return false;
    }
  }

  Future<bool> register(String email, String password) async {
    final api = ref.read(apiClientProvider);
    try {
      await api.register(email, password);
      return true;
    } catch (e) {
      return false;
    }
  }

  Future<void> logout() async {
    final api = ref.read(apiClientProvider);
    api.clearToken();
    final prefs = await SharedPreferences.getInstance();
    await prefs.remove('auth_token');
    state = const AsyncValue.data(null);
  }

  Future<void> changePassword(String oldPassword, String newPassword) async {
    await ref.read(apiClientProvider).changePassword(oldPassword, newPassword);
  }

  Future<void> deleteAccount() async {
    await ref.read(apiClientProvider).deleteAccount();
    await logout();
  }

  Future<void> forgotPassword(String email) async {
    await ref.read(apiClientProvider).forgotPassword(email);
  }

  Future<void> resetPassword(String token, String newPassword) async {
    await ref.read(apiClientProvider).resetPassword(token, newPassword);
  }
}
