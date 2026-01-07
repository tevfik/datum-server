import 'package:riverpod_annotation/riverpod_annotation.dart';
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

    final api = ref.read(apiClientProvider);
    final data = await api.getDevices();
    return data.map((json) => Device.fromJson(json)).toList();
  }

  Future<void> createProvisioningRequest(
      String uid, String name, String ssid, String pass) async {
    final api = ref.read(apiClientProvider);
    await api.createProvisioningRequest(uid, name, ssid, pass);
    ref.invalidateSelf(); // Refresh list
  }
}
