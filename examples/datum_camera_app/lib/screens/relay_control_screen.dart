import 'dart:async';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../models/device.dart';
import '../providers/auth_provider.dart';
import '../api_client.dart';

class RelayControlScreen extends StatefulWidget {
  final Device device;

  const RelayControlScreen({super.key, required this.device});

  @override
  State<RelayControlScreen> createState() => _RelayControlScreenState();
}

class _RelayControlScreenState extends State<RelayControlScreen> {
  late ApiClient _api;
  Timer? _timer;
  Map<String, dynamic> _deviceData = {};
  bool _isLoading = true;

  // Local state for toggles (optimistic UI)
  final List<bool> _relayStates = [false, false, false, false];

  @override
  void initState() {
    super.initState();
    // Get API client from provider (or create new if not in provider, but Home uses Provider)
    // Actually Home uses DeviceProvider which uses ApiClient internally usually.
    // DeviceDetailScreen creates ApiClient locally or uses context?
    // Checking DeviceDetailScreen imports: it imports '../api_client.dart' and instantiates it?
    // DeviceDetailScreen uses: `final _api = ApiClient();` (if I recall correctly or similar)
    // Actually the Outline showed it uses `_api` but didn't show instantiation.
    // AuthProvider usually holds the token.
    
    _api = ApiClient();
    _initAuth();
  }

  Future<void> _initAuth() async {
    final authProvider = Provider.of<AuthProvider>(context, listen: false);
    if (authProvider.token != null) {
      _api.setToken(authProvider.token!);
    }
    _pollData(); // Initial fetch
    _startPolling();
  }

  @override
  void dispose() {
    _timer?.cancel();
    super.dispose();
  }

  void _startPolling() {
    _timer = Timer.periodic(const Duration(seconds: 2), (timer) {
      if (mounted) _pollData();
    });
  }

  Future<void> _pollData() async {
    try {
      final data = await _api.getDeviceData(widget.device.id);
      if (mounted) {
        setState(() {
          _deviceData = data;
          _isLoading = false;
          // Sync relays
          for (int i = 0; i < 4; i++) {
            if (_deviceData.containsKey('relay_$i')) {
               _relayStates[i] = _deviceData['relay_$i'] == 1 || _deviceData['relay_$i'] == true;
            }
          }
        });
      }
    } catch (e) {
      debugPrint("Poll error: $e");
    }
  }

  Future<void> _toggleRelay(int index, bool value) async {
    // Optimistic update
    setState(() {
      _relayStates[index] = value;
    });

    try {
      await _api.sendCommand(widget.device.id, 'relay_control', params: {
        'relay_index': index,
        'state': value,
      });
      // Success - waiting for next poll to confirm
    } catch (e) {
      // Revert on failure
      if (mounted) {
        setState(() {
          _relayStates[index] = !value;
        });
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('Failed: $e')),
        );
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    // Voltage from telemetry or dummy
    double voltage = 0.0;
    if (_deviceData.containsKey('voltage')) voltage = (_deviceData['voltage'] as num).toDouble();
    if (_deviceData.containsKey('battery_adc')) {
       // Firmware sends raw ADC or calculated? Firmware sends analogRead.
       // Assuming simplistic conversion for display if raw.
       // Or assuming firmware sends 'voltage' if calculated.
       // Let's display raw or 0 if missing.
       // Actually Firmware sendData sends 'battery_adc'.
       // Map ADC to Voltage (approx 0-1024 -> 0-15V?)
       // Just showing ADC if 'voltage' missing.
       voltage = (_deviceData['battery_adc'] as num).toDouble(); 
    }

    return Scaffold(
      appBar: AppBar(title: Text(widget.device.name)),
      body: _isLoading 
          ? const Center(child: CircularProgressIndicator())
          : SingleChildScrollView(
              padding: const EdgeInsets.all(16),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  _buildVoltageCard(voltage),
                  const SizedBox(height: 24),
                  const Text("RELAY CONTROL", style: TextStyle(fontSize: 14, fontWeight: FontWeight.bold, color: Colors.grey)),
                  const SizedBox(height: 12),
                  _buildRelayGrid(),
                ],
              ),
            ),
    );
  }

  Widget _buildVoltageCard(double val) {
    return Container(
      padding: const EdgeInsets.all(20),
      decoration: BoxDecoration(
        color: Colors.white,
        borderRadius: BorderRadius.circular(16),
        boxShadow: [const BoxShadow(color: Colors.black12, blurRadius: 10, offset: Offset(0, 4))],
      ),
      child: Row(
        mainAxisAlignment: MainAxisAlignment.spaceBetween,
        children: [
          Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              const Text("INPUT VOLTAGE", style: TextStyle(fontSize: 12, fontWeight: FontWeight.bold, color: Colors.grey)),
              const SizedBox(height: 4),
              // If val is > 20 likely ADC, otherwise Voltage. quick hack
              Text(val > 50 ? "$val (ADC)" : "${val.toStringAsFixed(1)} V", 
                style: const TextStyle(fontSize: 28, fontWeight: FontWeight.bold, color: Colors.blueAccent)),
            ],
          ),
          const Icon(Icons.electric_bolt, size: 40, color: Colors.amber),
        ],
      ),
    );
  }

  Widget _buildRelayGrid() {
    return GridView.builder(
      physics: const NeverScrollableScrollPhysics(),
      shrinkWrap: true,
      gridDelegate: const SliverGridDelegateWithFixedCrossAxisCount(
        crossAxisCount: 2,
        childAspectRatio: 1.5,
        mainAxisSpacing: 12,
        crossAxisSpacing: 12,
      ),
      itemCount: 4,
      itemBuilder: (ctx, index) {
        return _buildRelaySwitch(index);
      },
    );
  }

  Widget _buildRelaySwitch(int index) {
    bool isOn = _relayStates.length > index ? _relayStates[index] : false;
    return Container(
      padding: const EdgeInsets.all(12),
      decoration: BoxDecoration(
        color: Colors.white, 
        borderRadius: BorderRadius.circular(16),
        border: isOn ? Border.all(color: Colors.blueAccent, width: 2) : null,
        boxShadow: [BoxShadow(color: Colors.black.withValues(alpha: 0.05), blurRadius: 8)],
      ),
      child: Column(
        mainAxisAlignment: MainAxisAlignment.spaceBetween,
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            mainAxisAlignment: MainAxisAlignment.spaceBetween,
            children: [
              Icon(Icons.power_settings_new, color: isOn ? Colors.blueAccent : Colors.grey),
              Switch(
                value: isOn, 
                onChanged: (v) => _toggleRelay(index, v),
                activeColor: Colors.blueAccent,
              ),
            ],
          ),
          Text("Relay ${index + 1}", style: const TextStyle(fontWeight: FontWeight.bold)),
        ],
      ),
    );
  }

}
