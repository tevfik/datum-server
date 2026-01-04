import 'dart:async';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../providers/auth_provider.dart';
import '../api_client.dart';
import '../models/device.dart';
import '../models/thing_description.dart'; // Import TD
import '../utils/thing_description_registry.dart'; // Import Registry
import '../widgets/stream_recorder.dart';
import '../widgets/dynamic_properties_view.dart'; // Import Dynamic View

class DeviceDetailScreen extends StatefulWidget {
  final Device device;

  const DeviceDetailScreen({super.key, required this.device});

  @override
  State<DeviceDetailScreen> createState() => _DeviceDetailScreenState();
}

class _DeviceDetailScreenState extends State<DeviceDetailScreen> {
  final ApiClient _api = ApiClient();
  late ThingDescription _td;
  Timer? _pollTimer;
  Map<String, dynamic> _deviceData = {};
  final bool _isRunning = true; 
  
  // Camera specific (only used if device type is camera)
  final StreamRecorderController _streamController = StreamRecorderController();

  @override
  void initState() {
    super.initState();
    final token = Provider.of<AuthProvider>(context, listen: false).token;
    if (token != null) _api.setToken(token);

    // 1. Get Thing Description
    _td = ThingDescriptionRegistry.get(widget.device.type);

    // 2. Start Polling Data
    _pollData();
    _pollTimer = Timer.periodic(const Duration(seconds: 3), (_) => _pollData());

    _streamController.addListener(() {
       if (mounted) setState(() {});
    });
  }

  @override
  void dispose() {
    _pollTimer?.cancel();
    super.dispose();
  }

  Future<void> _pollData() async {
    try {
      final data = await _api.getDeviceData(widget.device.id);
      if (mounted) {
        setState(() {
          _deviceData = data;
        });
      }
    } catch (e) {
      // debugPrint("Poll error: $e"); 
    }
  }

  String _getStreamUrl() {
    final token = Provider.of<AuthProvider>(context, listen: false).token;
    return 'https://datum.bezg.in/devices/${widget.device.id}/stream/mjpeg?token=$token';
  }

  Future<void> _handlePropertyChange(String key, dynamic value) async {
    // Optimistic Update
    setState(() {
      _deviceData[key] = value;
    });

    // Send Command based on key
    // For Relays: Custom command logic
    if (key.startsWith('relay_')) {
      // index is relay_0 -> 0
      int index = int.tryParse(key.split('_')[1]) ?? 0;
      await _sendCommand('relay_control', params: {'relay_index': index, 'state': value});
    }
  }

  Future<void> _sendCommand(String action, {Map<String, dynamic>? params}) async {
    try {
      await _api.sendCommand(widget.device.id, action, params: params);
      if (!mounted) return;
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('Command "$action" sent'), duration: const Duration(seconds: 1)),
      );
    } catch (e) {
      if (!mounted) return;
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('Failed: $e'), backgroundColor: Colors.red),
      );
    }
  }

  @override
  Widget build(BuildContext context) {
    bool isCamera = widget.device.type == 'camera';

    return Scaffold(
      appBar: AppBar(
        title: Text(widget.device.name),
        actions: [
          IconButton(icon: const Icon(Icons.refresh), onPressed: _pollData),
        ],
      ),
      body: Column(
        children: [
          // Stream Area (Conditionally Rendered)
          if (isCamera)
            Container(
              color: Colors.black,
              constraints: const BoxConstraints(maxHeight: 300),
              width: double.infinity,
              child: _isRunning
                  ? StreamRecorder(
                      streamUrl: _getStreamUrl(),
                      controller: _streamController,
                      fit: BoxFit.contain,
                    )
                  : const Center(child: Text("Stream Paused")),
            ),
          
          Expanded(
            child: ListView(
              padding: const EdgeInsets.all(16),
              children: [
                // WoT Dynamic Properties
                DynamicPropertiesView(
                  td: _td,
                  data: _deviceData,
                  onValueChanged: _handlePropertyChange,
                ),
                
                const SizedBox(height: 20),
                
                // Camera Specific Legacy Controls (Only if camera)
                if (isCamera) ...[
                   const Divider(),
                   ElevatedButton(
                     onPressed: () => _sendCommand("action", params: {"type": "led"}), // Legacy LED toggle
                     child: const Text("Use Camera Flash"),
                   )
                ],

                const SizedBox(height: 20),
                // Device Info
                Card(
                  child: ListTile(
                    title: Text("ID: ${widget.device.id}"),
                    subtitle: Text("Type: ${widget.device.type}\nLast Update: ${_deviceData['timestamp'] ?? '-'}"),
                    leading: const Icon(Icons.info_outline),
                    isThreeLine: true,
                  ),
                ),
              ],
            ),
          )
        ],
      ),
    );
  }
}



