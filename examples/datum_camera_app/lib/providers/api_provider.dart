import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:riverpod_annotation/riverpod_annotation.dart';
import '../api_client.dart';
import 'auth_provider.dart';

part 'api_provider.g.dart';

@riverpod
ApiClient apiClient(Ref ref) {
  return ApiClient();
}

@riverpod
Future<ApiClient> authenticatedApiClient(Ref ref) async {
  // Ensure auth is ready
  await ref.watch(authProvider.future);
  
  final client = ref.read(apiClientProvider);
  
  return client;
}
