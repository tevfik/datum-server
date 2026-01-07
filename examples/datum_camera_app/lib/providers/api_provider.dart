import 'package:riverpod_annotation/riverpod_annotation.dart';
import '../api_client.dart';
import 'auth_provider.dart';

part 'api_provider.g.dart';

@riverpod
ApiClient apiClient(ApiClientRef ref) {
  return ApiClient();
}

@riverpod
Future<ApiClient> authenticatedApiClient(AuthenticatedApiClientRef ref) async {
  final token = await ref.watch(authProvider.future);
  final client = ApiClient();
  if (token != null) {
    client.setToken(token);
  }
  return client;
}
