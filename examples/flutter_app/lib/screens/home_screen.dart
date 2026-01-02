import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../providers/auth_provider.dart';
import '../providers/device_provider.dart';
import 'provisioning_wizard.dart';
import 'device_detail_screen.dart';

class HomeScreen extends StatelessWidget {
  const HomeScreen({super.key});

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Datum Dashboard'),
        actions: [
          IconButton(
            icon: const Icon(Icons.logout),
            onPressed: () => Provider.of<AuthProvider>(context, listen: false).logout(),
          ),
        ],
      ),
      floatingActionButton: FloatingActionButton(
        onPressed: () {
          Navigator.push(
            context,
            MaterialPageRoute(builder: (_) => const ProvisioningWizard()),
          );
        },
        child: const Icon(Icons.add),
      ),
      body: Consumer<DeviceProvider>(
        builder: (context, deviceProvider, _) {
          if (deviceProvider.devices.isEmpty && !deviceProvider.isLoading) {
            deviceProvider.fetchDevices();
          }

          if (deviceProvider.isLoading) {
            return const Center(child: CircularProgressIndicator());
          }

          if (deviceProvider.devices.isEmpty) {
            return const Center(child: Text('No devices found. Tap + to add one.'));
          }

          return RefreshIndicator(
            onRefresh: () => deviceProvider.fetchDevices(),
            child: ListView.builder(
              itemCount: deviceProvider.devices.length,
              itemBuilder: (context, index) {
                final device = deviceProvider.devices[index];
                return ListTile(
                  leading: Icon(
                    device.type == 'camera' ? Icons.camera_alt : Icons.sensors,
                    color: device.status == 'online' ? Colors.green : Colors.grey,
                  ),
                  title: Text(device.name),
                  subtitle: Text(device.uid),
                  trailing: Text(device.status.toUpperCase()),
                  onTap: () {
                    Navigator.push(
                      context,
                      MaterialPageRoute(
                        builder: (_) => DeviceDetailScreen(device: device),
                      ),
                    );
                  },
                );
              },
            ),
          );
        },
      ),
    );
  }
}
