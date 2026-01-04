import 'package:flutter/material.dart';
import '../models/thing_description.dart';

class DynamicPropertiesView extends StatelessWidget {
  final ThingDescription td;
  final Map<String, dynamic> data;
  final Function(String key, dynamic value)? onValueChanged;

  const DynamicPropertiesView({
    super.key,
    required this.td,
    required this.data,
    this.onValueChanged,
  });

  @override
  Widget build(BuildContext context) {
    // Separate Booleans (Switches), Device Info (Strings/IP), and Sensors (Numbers)
    final boolProps = td.properties.entries.where((e) => e.value.type == 'boolean').toList();
    
    final infoKeys = ['local_ip', 'public_ip', 'ssid', 'bssid', 'channel', 'fw_ver', 'uptime', 'status', 'reset_reason'];
    final infoProps = td.properties.entries.where((e) => infoKeys.contains(e.key)).toList();
    
    final sensorProps = td.properties.entries
        .where((e) => e.value.type != 'boolean' && !infoKeys.contains(e.key))
        .toList();

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        // 1. Controls (Switches)
        if (boolProps.isNotEmpty) ...[
          const Text("Controls", style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold)),
          const SizedBox(height: 10),
          Card(
            child: Column(
              children: boolProps.map((entry) {
                final key = entry.key;
                final prop = entry.value;
                final bool value = data[key] == true;

                return SwitchListTile(
                  title: Text(prop.description.isNotEmpty ? prop.description : key),
                  value: value,
                  secondary: const Icon(Icons.power_settings_new),
                  onChanged: onValueChanged != null 
                    ? (val) => onValueChanged!(key, val) 
                    : null,
                );
              }).toList(),
            ),
          ),
          const SizedBox(height: 20),
        ],

        // 2. Sensors (Grid)
        if (sensorProps.isNotEmpty) ...[
          const Text("Sensors", style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold)),
          const SizedBox(height: 10),
          GridView.builder(
            shrinkWrap: true,
            physics: const NeverScrollableScrollPhysics(),
            gridDelegate: const SliverGridDelegateWithFixedCrossAxisCount(
              crossAxisCount: 2,
              childAspectRatio: 2.5,
              crossAxisSpacing: 10,
              mainAxisSpacing: 10,
            ),
            itemCount: sensorProps.length,
            itemBuilder: (ctx, i) {
              final entry = sensorProps[i];
              final key = entry.key;
              final prop = entry.value;
              final val = data[key] ?? '-';
              
              IconData icon = Icons.sensors;
              if (key.contains('rssi')) icon = Icons.wifi;
              if (key.contains('battery')) icon = Icons.battery_std;

              return Card(
                elevation: 2,
                child: Padding(
                  padding: const EdgeInsets.all(8.0),
                  child: Row(
                    children: [
                      Icon(icon, color: Colors.blueGrey),
                      const SizedBox(width: 10),
                      Expanded(
                        child: Column(
                          crossAxisAlignment: CrossAxisAlignment.start,
                          mainAxisAlignment: MainAxisAlignment.center,
                          children: [
                            Text(prop.description.isNotEmpty ? prop.description : key, 
                                 style: const TextStyle(fontSize: 12, color: Colors.grey),
                                 maxLines: 1, overflow: TextOverflow.ellipsis),
                            Text("$val${prop.unit != null ? " ${prop.unit}" : ""}", 
                                 style: const TextStyle(fontSize: 16, fontWeight: FontWeight.bold)),
                          ],
                        ),
                      )
                    ],
                  ),
                ),
              );
            },
          ),
          const SizedBox(height: 20),
        ],
        
        // 3. Device Info (List)
        if (infoProps.isNotEmpty) ...[
          const Text("Device Info", style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold)),
          const SizedBox(height: 10),
          Card(
            child: Column(
              children: infoProps.map((entry) {
                 final key = entry.key;
                 final prop = entry.value;
                 final val = data[key] ?? '-';
                 IconData icon = Icons.info;
                 if (key.contains('ip')) icon = Icons.lan;
                 if (key == 'ssid') icon = Icons.wifi_password;
                 if (key == 'bssid') icon = Icons.router;
                 if (key == 'uptime') icon = Icons.timer;
                 if (key == 'fw_ver') icon = Icons.system_update;

                 return ListTile(
                   leading: Icon(icon, color: Colors.grey),
                   title: Text(prop.description),
                   trailing: Text(val.toString(), style: const TextStyle(fontWeight: FontWeight.bold)),
                   dense: true,
                 );
              }).toList(),
            ),
          ),
        ]
      ],
    );
  }
}
