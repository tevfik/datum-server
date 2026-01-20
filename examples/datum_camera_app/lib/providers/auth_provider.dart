import 'package:riverpod_annotation/riverpod_annotation.dart';
import '../utils/token_storage.dart';
import 'api_provider.dart';

part 'auth_provider.g.dart';

@Riverpod(keepAlive: true)
class Auth extends _$Auth {
  @override
  Future<bool> build() async {
    final hasToken = await TokenStorage.hasToken();
    if (hasToken) {
      // Setup interceptor callback to logout when token expires and refresh fails
      ref.read(apiClientProvider).onUnauthorized = () {
        state = const AsyncValue.data(false);
      };
    }
    return hasToken;
  }

  bool get isAuthenticated => state.value ?? false;

  Future<bool> login(String email, String password) async {
    final api = ref.read(apiClientProvider);
    state = const AsyncValue.loading();
    try {
      await api.login(email, password);
      // Setup logout callback
      api.onUnauthorized = () {
        state = const AsyncValue.data(false);
      };
      
      state = const AsyncValue.data(true);
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
    await api.logout();
    state = const AsyncValue.data(false);
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
