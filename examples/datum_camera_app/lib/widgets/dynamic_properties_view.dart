import 'dart:async';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../models/device.dart';
import '../providers/api_provider.dart';

class DynamicWoTView extends ConsumerStatefulWidget {
  final Device device;

  const DynamicWoTView({super.key, required this.device});

  @override
  ConsumerState<DynamicWoTView> createState() => _DynamicWoTViewState();
}

class _DynamicWoTViewState extends ConsumerState<DynamicWoTView> {
  Timer? _timer;
  Map<String, dynamic> _deviceData = {};
  final List<Map<String, String>> _properties = [];

  @override
  void initState() {
    super.initState();
    _parseThingDescription();
    _startPolling();
  }

  @override
  void dispose() {
    _timer?.cancel();
    super.dispose();
  }

  void _parseThingDescription() {
    final td = widget.device.thingDescription;
    if (td == null || !td.containsKey('properties')) return;

    _properties.clear();
    final props = td['properties'] as Map<String, dynamic>;
    props.forEach((key, val) {
      if (val is Map<String, dynamic>) {
        _properties.add({
          'key': key,
          'title': (val['title'] ?? key).toString(),
          'type': (val['type'] ?? 'string').toString(),
          'unit': (val['unit'] ?? '').toString(),
          'readOnly': (val['readOnly'] ?? true).toString(),
          'widget': (val['ui:widget'] ?? '').toString(),
        });
      }
    });

    if (mounted) setState(() {});
  }

  void _startPolling() {
    _pollData();
    _timer = Timer.periodic(const Duration(seconds: 2), (timer) {
      _pollData();
    });
  }

  Future<void> _pollData() async {
    if (!mounted) return;
    try {
      final api = await ref.read(authenticatedApiClientProvider.future);

      // If we don't have properties yet (TD missing), fetch full device info
      if (_properties.isEmpty) {
        final deviceJson = await api.getDevice(widget.device.id);
        if (deviceJson.containsKey('thing_description') &&
            deviceJson['thing_description'] != null) {
          final td = deviceJson['thing_description'];
          if (td != null) {
            _properties.clear();
            final props = td['properties'] as Map<String, dynamic>;
            props.forEach((key, val) {
              if (val is Map<String, dynamic>) {
                _properties.add({
                  'key': key,
                  'title': (val['title'] ?? key).toString(),
                  'type': (val['type'] ?? 'string').toString(),
                  'unit': (val['unit'] ?? '').toString(),
                  'readOnly': (val['readOnly'] ?? true).toString(),
                  'widget': (val['ui:widget'] ?? '').toString(),
                });
              }
            });
          }
        }
      }

      final data = await api.getDeviceData(widget.device.id);

      if (mounted) {
        setState(() {
          _deviceData = data;
          if (data.containsKey('shadow_state')) {
            final shadow = data['shadow_state'] as Map<String, dynamic>;
            _deviceData.addAll(shadow);
          }
        });
      }
    } catch (e) {
      // debugPrint("Poll Error: $e");
    }
  }

  Future<void> _handlePropertyChange(String key, dynamic value) async {
    setState(() {
      _deviceData[key] = value;
    });

    try {
      final api = await ref.read(authenticatedApiClientProvider.future);
      await api.sendCommand(widget.device.id, "set_property",
          params: {"key": key, "value": value});

      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(const SnackBar(
            content: Text("Command Sent"),
            duration: Duration(milliseconds: 500)));
      }
    } catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
            SnackBar(content: Text("Error: $e"), backgroundColor: Colors.red));
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    if (_properties.isEmpty) {
      return const Center(child: Text("No Properties Found in Description"));
    }

    return GridView.builder(
      padding: const EdgeInsets.all(16),
      gridDelegate: const SliverGridDelegateWithFixedCrossAxisCount(
        crossAxisCount: 2,
        crossAxisSpacing: 10,
        mainAxisSpacing: 10,
        childAspectRatio: 1.5,
      ),
      itemCount: _properties.length,
      itemBuilder: (context, index) {
        final prop = _properties[index];
        final key = prop['key']!;
        final widgetType = prop['widget'];

        dynamic rawVal = _deviceData[key];

        // 1. Time Series Widget (Graph Placeholder)
        if (widgetType == 'timeseries') {
          return Card(
            elevation: 4,
            color: Colors.grey[900],
            child: Padding(
              padding: const EdgeInsets.all(8.0),
              child: Column(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  Text(prop['title']!,
                      style: const TextStyle(color: Colors.white70)),
                  Expanded(
                    child: const Center(
                      child: Icon(Icons.show_chart,
                          color: Colors.blueAccent, size: 40),
                    ),
                  ),
                  Text("${rawVal ?? '--'} ${prop['unit']}",
                      style: const TextStyle(
                          color: Colors.white, fontWeight: FontWeight.bold)),
                ],
              ),
            ),
          );
        }

        // 2. Gauge Widget (Placeholder)
        if (widgetType == 'gauge') {
          return Card(
            elevation: 4,
            color: Colors.grey[900],
            child: Padding(
              padding: const EdgeInsets.all(8.0),
              child: Column(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  Text(prop['title']!,
                      style: const TextStyle(color: Colors.white70)),
                  Expanded(
                    child: Center(
                      child: Stack(
                        alignment: Alignment.center,
                        children: [
                          const Icon(Icons.speed,
                              color: Colors.orangeAccent, size: 40),
                          Positioned(
                              bottom: 0,
                              child: Text("${rawVal ?? '--'}",
                                  style: const TextStyle(
                                      color: Colors.white, fontSize: 10)))
                        ],
                      ),
                    ),
                  ),
                ],
              ),
            ),
          );
        }

        // 3. Boolean Switch
        bool isBool = prop['type'] == 'boolean';
        bool isReadOnly = prop['readOnly'] == 'true';

        if (isBool && !isReadOnly) {
          bool switchVal = rawVal == true || rawVal == 'true' || rawVal == 1;
          return Card(
              elevation: 4,
              color: switchVal ? Colors.blueGrey[800] : Colors.grey[900],
              shape: RoundedRectangleBorder(
                  borderRadius: BorderRadius.circular(12),
                  side: switchVal
                      ? const BorderSide(color: Colors.blueAccent, width: 2)
                      : BorderSide.none),
              child: InkWell(
                  borderRadius: BorderRadius.circular(12),
                  onTap: () => _handlePropertyChange(key, !switchVal),
                  child: Padding(
                      padding: const EdgeInsets.all(12),
                      child: Column(
                          mainAxisAlignment: MainAxisAlignment.spaceBetween,
                          crossAxisAlignment: CrossAxisAlignment.start,
                          children: [
                            Text(prop['title']!,
                                style: const TextStyle(
                                    color: Colors.white70,
                                    fontWeight: FontWeight.bold)),
                            Row(
                                mainAxisAlignment:
                                    MainAxisAlignment.spaceBetween,
                                children: [
                                  Text(switchVal ? "ON" : "OFF",
                                      style: TextStyle(
                                          fontSize: 20,
                                          fontWeight: FontWeight.bold,
                                          color: switchVal
                                              ? Colors.greenAccent
                                              : Colors.white38)),
                                  Switch(
                                    value: switchVal,
                                    onChanged: (v) =>
                                        _handlePropertyChange(key, v),
                                    activeColor: Colors.greenAccent,
                                  ),
                                ])
                          ]))));
        }

        // 4. Default Read-Only Display
        String unit = prop['unit'] ?? '';
        String displayVal = rawVal?.toString() ?? '--';

        return Card(
          elevation: 4,
          color: Colors.grey[900],
          child: Padding(
            padding: const EdgeInsets.all(12.0),
            child: Column(
              mainAxisAlignment: MainAxisAlignment.center,
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  prop['title']!,
                  style: const TextStyle(
                      color: Colors.white70,
                      fontSize: 14,
                      fontWeight: FontWeight.bold),
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                ),
                const Spacer(),
                Text(
                  displayVal,
                  style:
                      const TextStyle(color: Colors.blueAccent, fontSize: 24),
                ),
                if (unit.isNotEmpty)
                  Text(unit,
                      style: const TextStyle(color: Colors.grey, fontSize: 12)),
              ],
            ),
          ),
        );
      },
    );
  }
}
