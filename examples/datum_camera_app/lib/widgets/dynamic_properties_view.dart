import 'dart:async';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_colorpicker/flutter_colorpicker.dart';
import '../models/device.dart';
import '../providers/api_provider.dart';
import 'sparkline_widget.dart';

class DynamicWoTView extends ConsumerStatefulWidget {
  final Device device;

  const DynamicWoTView({super.key, required this.device});

  @override
  ConsumerState<DynamicWoTView> createState() => _DynamicWoTViewState();
}

class _DynamicWoTViewState extends ConsumerState<DynamicWoTView> {
  Timer? _timer;
  Map<String, dynamic> _deviceData = {};
  final List<Map<String, dynamic>> _properties = [];

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
    _extractProperties(td['properties'] as Map<String, dynamic>);
    if (mounted) setState(() {});
  }

  void _extractProperties(Map<String, dynamic> props) {
    _properties.clear();
    props.forEach((key, val) {
      if (val is Map<String, dynamic>) {
        _properties.add({
          'key': key,
          'title': (val['title'] ?? key).toString(),
          'type': (val['type'] ?? 'string').toString(),
          'unit': (val['unit'] ?? '').toString(),
          // Store as actual boolean
          'readOnly': val['readOnly'] == true ||
              val['readOnly'] ==
                  'true', // Default false if missing? No, usually default is false (writeable) or true?
          // Original code: (val['readOnly'] ?? true).toString() -> defaulted to true (read-only)
          // So if missing, it's ReadOnly.
          // Let's stick to that:
          'isReadOnly': val['readOnly'] == true ||
              val['readOnly'] == 'true' ||
              val['readOnly'] == null,
          'widget': (val['ui:widget'] ?? '').toString(),
          'enum': val['enum'], // List
          'minimum': val['minimum'], // num
          'maximum': val['maximum'], // num
        });
      }
    });
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
            final props = td['properties'] as Map<String, dynamic>;
            _extractProperties(props);
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

  Color _parseColor(String? hexString) {
    if (hexString == null || hexString.isEmpty) return Colors.white;
    try {
      var hex = hexString.replaceAll('#', '');
      if (hex.length == 6) hex = 'FF$hex';
      return Color(int.parse(hex, radix: 16));
    } catch (e) {
      return Colors.white;
    }
  }

  String _colorToHex(Color color) {
    // ignore: deprecated_member_use
    return '#${color.value.toRadixString(16).substring(2).toUpperCase()}';
  }

  void _showColorPicker(String key, String currentValue) {
    Color pickerColor = _parseColor(currentValue);
    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Pick a color'),
        content: SingleChildScrollView(
          child: ColorPicker(
            pickerColor: pickerColor,
            onColorChanged: (color) {
              pickerColor = color;
            },
            // Use block picker or material picker if preferred, but standard is fine
            enableAlpha: false,
            displayThumbColor: true,
            paletteType: PaletteType.hsvWithHue,
          ),
        ),
        actions: <Widget>[
          ElevatedButton(
            child: const Text('Got it'),
            onPressed: () {
              Navigator.of(context).pop();
              _handlePropertyChange(key, _colorToHex(pickerColor));
            },
          ),
        ],
      ),
    );
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
        final key = prop['key'] as String;
        final title = prop['title'] as String;
        final widgetType = prop['widget'] as String;
        final unit = prop['unit'] as String;
        final isReadOnly = prop['isReadOnly'] as bool;

        dynamic rawVal = _deviceData[key];

        // 1. Time Series Widget (Real Chart)
        if (widgetType == 'timeseries') {
          double? val =
              rawVal != null ? double.tryParse(rawVal.toString()) : null;
          bool isDemo = widget.device.type == 'demo';

          return Card(
            elevation: 4,
            color: Colors.grey[900],
            child: Padding(
              padding: const EdgeInsets.all(8.0),
              child: Column(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  Text(title, style: const TextStyle(color: Colors.white70)),
                  Expanded(
                    child: Padding(
                      padding: const EdgeInsets.symmetric(vertical: 8.0),
                      child: SparklineWidget(
                        value: val,
                        demoMode: isDemo,
                        color: Colors.blueAccent,
                      ),
                    ),
                  ),
                  Text("${rawVal ?? '--'} $unit",
                      style: const TextStyle(
                          color: Colors.white, fontWeight: FontWeight.bold)),
                ],
              ),
            ),
          );
        }

        // 2. Gauge Widget
        if (widgetType == 'gauge') {
          return Card(
            elevation: 4,
            color: Colors.grey[900],
            child: Padding(
              padding: const EdgeInsets.all(8.0),
              child: Column(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  Text(title, style: const TextStyle(color: Colors.white70)),
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

        // 3. Color Picker
        if (widgetType == 'color') {
          Color currentColor = _parseColor(rawVal?.toString());
          return Card(
            elevation: 4,
            color: Colors.grey[900],
            child: InkWell(
              onTap: isReadOnly
                  ? null
                  : () =>
                      _showColorPicker(key, rawVal?.toString() ?? '#FFFFFF'),
              child: Padding(
                padding: const EdgeInsets.all(8.0),
                child: Column(
                  mainAxisAlignment: MainAxisAlignment.center,
                  children: [
                    Text(title, style: const TextStyle(color: Colors.white70)),
                    const SizedBox(height: 10),
                    Container(
                      width: 40,
                      height: 40,
                      decoration: BoxDecoration(
                        color: currentColor,
                        shape: BoxShape.circle,
                        border: Border.all(color: Colors.white30, width: 2),
                      ),
                    ),
                    const SizedBox(height: 5),
                    Text(rawVal?.toString() ?? '--',
                        style: const TextStyle(
                            color: Colors.white54, fontSize: 10)),
                  ],
                ),
              ),
            ),
          );
        }

        // 4. Slider (Range)
        var min = prop['minimum'];
        var max = prop['maximum'];
        if (!isReadOnly &&
            (widgetType == 'slider' || (min != null && max != null))) {
          double minVal = (min ?? 0).toDouble();
          double maxVal = (max ?? 100).toDouble();
          double currentVal =
              double.tryParse(rawVal?.toString() ?? '0') ?? minVal;
          // Clamp
          if (currentVal < minVal) currentVal = minVal;
          if (currentVal > maxVal) currentVal = maxVal;

          return Card(
            elevation: 4,
            color: Colors.grey[900],
            child: Padding(
              padding: const EdgeInsets.all(8.0),
              child: Column(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  Text(title, style: const TextStyle(color: Colors.white70)),
                  Expanded(
                    child: Slider(
                      value: currentVal,
                      min: minVal,
                      max: maxVal,
                      activeColor: Colors.blueAccent,
                      onChanged: (v) {
                        // Local update only for smooth sliding
                        setState(() {
                          _deviceData[key] = v;
                        });
                      },
                      onChangeEnd: (v) {
                        // Commit change
                        _handlePropertyChange(key, v);
                      },
                    ),
                  ),
                  Text("${currentVal.toStringAsFixed(1)} $unit",
                      style: const TextStyle(
                          color: Colors.white, fontWeight: FontWeight.bold)),
                ],
              ),
            ),
          );
        }

        // 5. Select (Enum)
        var enumList = prop['enum'];
        if (!isReadOnly && enumList is List && enumList.isNotEmpty) {
          String currentVal = rawVal?.toString() ?? (enumList.first.toString());
          if (!enumList.contains(currentVal)) {
            // Handle case where current value is not in enum list
            // Try to match ignoring case or just add it temporarily?
            // Or just show first?
          }

          return Card(
            elevation: 4,
            color: Colors.grey[900],
            child: Padding(
              padding: const EdgeInsets.all(8.0),
              child: Column(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  Text(title, style: const TextStyle(color: Colors.white70)),
                  const SizedBox(height: 5),
                  DropdownButton<String>(
                    value:
                        enumList.map((e) => e.toString()).contains(currentVal)
                            ? currentVal
                            : null,
                    hint:
                        Text(currentVal, style: TextStyle(color: Colors.white)),
                    dropdownColor: Colors.grey[850],
                    style: const TextStyle(color: Colors.white),
                    isExpanded: true,
                    underline: Container(height: 1, color: Colors.blueAccent),
                    items: enumList.map((e) {
                      return DropdownMenuItem<String>(
                        value: e.toString(),
                        child: Text(e.toString()),
                      );
                    }).toList(),
                    onChanged: (v) {
                      if (v != null) _handlePropertyChange(key, v);
                    },
                  ),
                ],
              ),
            ),
          );
        }

        // 6. Boolean Switch
        bool isBool = prop['type'] == 'boolean';

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
                            Text(title,
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
                                    // ignore: deprecated_member_use
                                    activeColor: Colors.greenAccent,
                                  ),
                                ])
                          ]))));
        }

        // 7. Default Read-Only Display
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
                  title,
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
