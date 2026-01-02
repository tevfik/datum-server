import 'package:flutter/material.dart';
import '../api_client.dart';
import '../models/device.dart';

class DeviceProvider with ChangeNotifier {
  List<Device> _devices = [];
  List<Device> get devices => _devices;
  bool _isLoading = false;
  bool get isLoading => _isLoading;

  final ApiClient _api = ApiClient();

  Future<void> fetchDevices() async {
    _isLoading = true;
    notifyListeners();
    try {
      final data = await _api.getDevices();
      _devices = data.map((json) => Device.fromJson(json)).toList();
    } catch (e) {
      debugPrint('Error fetching devices: $e');
    } finally {
      _isLoading = false;
      notifyListeners();
    }
  }

  Future<void> createProvisioningRequest(String uid, String name, String ssid, String pass) async {
    await _api.createProvisioningRequest(uid, name, ssid, pass);
    await fetchDevices(); // Refresh list (though it will be pending)
  }
}
