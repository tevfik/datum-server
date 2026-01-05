import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../providers/device_provider.dart';
import 'provisioning_wizard.dart';
import 'device_detail_screen.dart';
import 'relay_control_screen.dart';
import 'settings_screen.dart';

class HomeScreen extends StatelessWidget {
  const HomeScreen({super.key});

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Datum Dashboard'),
        actions: [
          IconButton(
            icon: const Icon(Icons.settings),
            onPressed: () {
               Navigator.push(
                context,
                MaterialPageRoute(builder: (_) => const SettingsScreen()),
              );
            },
          ),
        ],
      ),
      floatingActionButton: FloatingActionButton(
        onPressed: () async {
          // Wait for wizard to complete
          final result = await Navigator.push(
            context,
            MaterialPageRoute(builder: (_) => const ProvisioningWizard()),
          );
          
          // Refresh list if new device was added
          if (result == true && context.mounted) {
             Provider.of<DeviceProvider>(context, listen: false).fetchDevices();
          }
        },
        child: const Icon(Icons.add),
      ),
      body: Consumer<DeviceProvider>(
        builder: (context, deviceProvider, _) {

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
                    device.type == 'camera' ? Icons.camera_alt : 
                    device.type == 'relay_board' ? Icons.power : Icons.sensors,
                    color: device.status == 'online' ? Colors.green : Colors.grey,
                  ),
                  title: Text(device.name),
                  subtitle: Text(device.uid),
                  trailing: Text(device.status.toUpperCase()),
                  onTap: () async {
                    final result = await Navigator.push(
                      context,
                      MaterialPageRoute(
                        builder: (_) => device.type == 'relay_board' 
                            ? RelayControlScreen(device: device)
                            : DeviceDetailScreen(device: device),
                      ),
                    );
                    
                    if (result == true) {
                      // Refresh list if device was deleted
                      if (context.mounted) {
                        Provider.of<DeviceProvider>(context, listen: false).fetchDevices();
                      }
                    }
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
