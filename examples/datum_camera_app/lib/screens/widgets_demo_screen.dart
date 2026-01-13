import 'package:flutter/material.dart';
import '../models/device.dart';
import '../widgets/dynamic_properties_view.dart';

class WidgetsDemoScreen extends StatelessWidget {
  const WidgetsDemoScreen({super.key});

  @override
  Widget build(BuildContext context) {
    // Construct a Mock Device with rich Thing Description
    final demoDevice = Device(
      id: 'demo_device',
      uid: 'demo_uid',
      name: 'Widgets Demo Box',
      type: 'demo',
      status: 'online',
      thingDescription: {
        "properties": {
          "voltage": {
            "title": "Battery Voltage",
            "type": "number",
            "unit": "V",
            "ui:widget": "timeseries",
            "readOnly": true
          },
          "speed": {
            "title": "Engine Speed",
            "type": "number",
            "unit": "RPM",
            "ui:widget": "gauge",
            "readOnly": true
          },
          "temperature": {
            "title": "Core Temp",
            "type": "number",
            "unit": "°C",
            "readOnly": true
          },
          "relay_1": {
            "title": "Main Power",
            "type": "boolean",
            "readOnly": false
          },
          "relay_2": {
            "title": "Auxiliary Light",
            "type": "boolean",
            "readOnly": false
          },
          "status_msg": {
            "title": "Status Message",
            "type": "string",
            "readOnly": true
          }
        }
      },
    );

    return Scaffold(
      appBar: AppBar(
        title: const Text("WoT Widgets Demo"),
      ),
      body: DynamicWoTView(device: demoDevice),
    );
  }
}
