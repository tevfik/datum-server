import 'package:riverpod_annotation/riverpod_annotation.dart';
import 'package:dio/dio.dart';
import '../models/device.dart';
import 'api_provider.dart';
import 'auth_provider.dart';

part 'device_provider.g.dart';

@riverpod
class Devices extends _$Devices {
  @override
  Future<List<Device>> build() async {
    final token = await ref.watch(authProvider.future);
    if (token == null) return [];

    final api = await ref.watch(authenticatedApiClientProvider.future);
    try {
      final data = await api.getDevices();
      return data.map((json) => Device.fromJson(json)).toList();
    } on DioException catch (e) {
      if (e.response?.statusCode == 401) return [];
      rethrow;
    }
  }

  Future<void> createProvisioningRequest(
      String uid, String name, String ssid, String pass) async {
    final api = await ref.read(authenticatedApiClientProvider.future);
    await api.createProvisioningRequest(uid, name, ssid, pass);
    ref.invalidateSelf(); // Refresh list
  }
}
