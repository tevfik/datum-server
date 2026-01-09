import 'dart:ui' as ui;
import 'dart:typed_data';
import 'package:flutter/material.dart';

import 'package:flutter_colorpicker/flutter_colorpicker.dart';
import 'package:dio/dio.dart';
import 'package:gal/gal.dart';

import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'full_screen_stream.dart';
import '../providers/api_provider.dart';
import '../providers/auth_provider.dart';
import '../models/device.dart';
import '../widgets/stream_recorder.dart';

class DeviceDetailScreen extends ConsumerStatefulWidget {
  final Device device;

  const DeviceDetailScreen({super.key, required this.device});

  @override
  ConsumerState<DeviceDetailScreen> createState() => _DeviceDetailScreenState();
}

class _DeviceDetailScreenState extends ConsumerState<DeviceDetailScreen> {
  // final ApiClient _api = ApiClient(); // Removed
  bool _isRunning = true; // For MJPEG stream
  bool _loadingAction = false;

  // Stream Recording
  final StreamRecorderController _streamController = StreamRecorderController();

  // Device Data (Telemetry)
  Map<String, dynamic> _deviceData = {};

  // State for settings
  String _vres = "VGA"; // Video Stream Res
  String _ires = "UXGA"; // Snapshot Res
  Color _ledColor = Colors.white;
  double _ledBrightness = 100;
  bool _hmirror = false;
  bool _vflip = false;
  bool _ledOn = true;

  // Motion Settings
  bool _motionEnabled = true;
  double _motionSensitivity = 50;
  int _motionPeriod = 1; // Seconds

  // Throttling
  // Timer? _throttleTimer; Using simple debouncer logic inside update

  @override
  void initState() {
    super.initState();
    _streamController.addListener(() {
      if (mounted) setState(() {});
    });

    _pollData();
    _startStream();
  }

  @override
  void dispose() {
    _stopStream();
    _streamController.dispose();
    super.dispose();
  }

  // Throttling Helper
  final Map<String, dynamic> _pendingUpdates = {};

  void _sendThrottled(String key, dynamic value) {
    _pendingUpdates[key] = value;

    // Cancel previous if any (Debounce 300ms)
    // Since we don't have easy Timer reference per-key, we use a single global timer for simplicity
    // For a robust app we'd use a Throttler class, but here:

    // Simple Debounce:
    Future.delayed(const Duration(milliseconds: 300), () {
      if (!mounted) return;
      // If this value is still the pending one, send it
      if (_pendingUpdates[key] == value) {
        _sendCommand("update_settings", params: {key: value});
      }
    });
  }

  void _updateSetting(String key, dynamic value) {
    // Immediate updates for Switches/Dropdowns
    // Throttled updates for Sliders/Pickers

    bool isSlider = (key == "lbri" || key == "msens" || key == "lcol");

    if (isSlider) {
      _sendThrottled(key, value);
    } else {
      _sendCommand("update_settings", params: {key: value});
    }
  }

  Future<void> _startStream() async {
    try {
      final api = await ref.read(authenticatedApiClientProvider.future);
      await api
          .sendCommand(widget.device.id, "stream", params: {"state": "on"});
    } catch (e) {
      debugPrint("Start Stream Error: $e");
    }
  }

  Future<void> _stopStream() async {
    try {
      // Fire and forget, no await to avoid blocking dispose
      final api = await ref.read(authenticatedApiClientProvider.future);
      api.sendCommand(widget.device.id, "stream", params: {"state": "off"});
    } catch (e) {
      debugPrint("Stop Stream Error: $e");
    }
  }

  Future<void> _pollData() async {
    try {
      final api = await ref.read(authenticatedApiClientProvider.future);
      final data = await api.getDeviceData(widget.device.id);
      if (!mounted) return;

      // Client-Side IP Sanitization (Fix for Docker environment)
      if (data.containsKey('public_ip')) {
        final ip = data['public_ip'].toString();
        if (ip.startsWith("172.")) {
          // It's a Docker internal IP. Prefer Local IP if available
          if (data.containsKey('local_ip') && data['local_ip'] != null) {
            data['public_ip'] = data['local_ip'];
          } else {
            data.remove('public_ip'); // Remove if no alternative
          }
        }
      }

      // Fallback: If Public IP missing from telemetry, check Device Metadata
      if (!data.containsKey('public_ip') && widget.device.publicIP != null) {
        final metaIP = widget.device.publicIP!;
        if (!metaIP.startsWith("172.")) {
          data['public_ip'] = metaIP;
        }
      }

      setState(() {
        _deviceData = data;

        // Sync state from telemetry
        if (data.containsKey('vres')) {
          _vres = data['vres'];
        } else if (data.containsKey('resolution')) {
          _vres = data['resolution']; // Fallback
        }

        if (data.containsKey('ires')) {
          _ires = data['ires'];
        }

        if (data.containsKey('lbri')) {
          _ledBrightness = (data['lbri'] as num).toDouble();
        } else if (data.containsKey('led_brightness')) {
          _ledBrightness = (data['led_brightness'] as num).toDouble();
        }

        if (data.containsKey('led')) {
          _ledOn = data['led'];
        } else if (data.containsKey('led_on')) {
          _ledOn = data['led_on'];
        }

        if (data.containsKey('imir')) {
          _hmirror = data['imir'];
        } else if (data.containsKey('hmirror')) {
          _hmirror = data['hmirror'];
        }

        if (data.containsKey('iflip')) {
          _vflip = data['iflip'];
        } else if (data.containsKey('vflip')) {
          _vflip = data['vflip'];
        }

        // Motion
        if (data.containsKey('mot')) {
          _motionEnabled = data['mot'];
        }
        if (data.containsKey('msens')) {
          _motionSensitivity = (data['msens'] as num).toDouble();
        }
        if (data.containsKey('mper')) {
          _motionPeriod = data['mper'];
        }

        // Parse Color
        String? hex;
        if (data.containsKey('lcol')) {
          hex = data['lcol'];
        } else if (data.containsKey('led_color')) {
          hex = data['led_color'];
        }

        if (hex != null && hex.startsWith('#') && hex.length == 7) {
          _ledColor = Color(int.parse("0xFF${hex.substring(1)}"));
        }
      });
    } catch (e) {
      // debugPrint("Poll error: $e");
    }
  }

  String _getStreamUrl() {
    // We need token synchronously for URL string.
    final token = ref.read(authProvider).value;
    return 'https://datum.bezg.in/devices/${widget.device.id}/stream/mjpeg?token=$token';
  }

  void _showSettingsDialog() {
    showModalBottomSheet(
      context: context,
      isScrollControlled: true,
      builder: (context) {
        return StatefulBuilder(
          builder: (context, setModalState) {
            return Container(
              padding: const EdgeInsets.all(20),
              height: 800,
              child: SingleChildScrollView(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    const Text("Device Settings",
                        style: TextStyle(
                            fontSize: 20, fontWeight: FontWeight.bold)),
                    const SizedBox(height: 20),

                    // --- VIDEO & IMAGE ---
                    const Text("Video Resolution (Stream)",
                        style: TextStyle(fontWeight: FontWeight.bold)),
                    DropdownButton<String>(
                      value: _vres,
                      isExpanded: true,
                      items: ["QVGA", "VGA", "SVGA", "HD", "UXGA"]
                          .map((val) =>
                              DropdownMenuItem(value: val, child: Text(val)))
                          .toList(),
                      onChanged: (val) {
                        if (val != null) {
                          setModalState(() => _vres = val);
                          setState(() => _vres = val);
                          _updateSetting("vres", val);
                        }
                      },
                    ),
                    const SizedBox(height: 10),
                    const Text("Snapshot Resolution",
                        style: TextStyle(fontWeight: FontWeight.bold)),
                    DropdownButton<String>(
                      value: _ires,
                      isExpanded: true,
                      items: ["HD", "UXGA", "QXGA"]
                          .map((val) =>
                              DropdownMenuItem(value: val, child: Text(val)))
                          .toList(),
                      onChanged: (val) {
                        if (val != null) {
                          setModalState(() => _ires = val);
                          setState(() => _ires = val);
                          _updateSetting("ires", val);
                        }
                      },
                    ),

                    const Divider(),
                    // --- ORIENTATION ---
                    SwitchListTile(
                      title: const Text("Mirror Horizontal"),
                      value: _hmirror,
                      onChanged: (val) {
                        setModalState(() => _hmirror = val);
                        setState(() => _hmirror = val);
                        _updateSetting("imir", val);
                      },
                    ),
                    SwitchListTile(
                      title: const Text("Flip Vertical"),
                      value: _vflip,
                      onChanged: (val) {
                        setModalState(() => _vflip = val);
                        setState(() => _vflip = val);
                        _updateSetting("iflip", val);
                      },
                    ),

                    const Divider(),
                    // --- MOTION DETECTION ---
                    const Text("Motion Detection",
                        style: TextStyle(
                            fontSize: 18, fontWeight: FontWeight.bold)),
                    SwitchListTile(
                      title: const Text("Enable Motion Detection"),
                      value: _motionEnabled,
                      onChanged: (val) {
                        setModalState(() => _motionEnabled = val);
                        setState(() => _motionEnabled = val);
                        _updateSetting("mot", val);
                      },
                    ),
                    if (_motionEnabled) ...[
                      Text("Sensitivity: ${_motionSensitivity.toInt()}%"),
                      Slider(
                        value: _motionSensitivity,
                        min: 0,
                        max: 100,
                        divisions: 20,
                        label: _motionSensitivity.round().toString(),
                        onChanged: (val) {
                          setModalState(() => _motionSensitivity = val);
                          _updateSetting("msens", val.toInt()); // Debounced
                        },
                        onChangeEnd: (val) {
                          setState(() => _motionSensitivity = val);
                        },
                      ),
                      const SizedBox(height: 10),
                      const Text("Check Period (Idle Mode)"),
                      Row(
                        mainAxisAlignment: MainAxisAlignment.spaceEvenly,
                        children: [1, 2, 5].map((sec) {
                          return ChoiceChip(
                            label: Text("${sec}s"),
                            selected: _motionPeriod == sec,
                            onSelected: (selected) {
                              if (selected) {
                                setModalState(() => _motionPeriod = sec);
                                setState(() => _motionPeriod = sec);
                                _updateSetting("mper", sec);
                              }
                            },
                          );
                        }).toList(),
                      ),
                    ],

                    const Divider(),
                    // --- LED CONTROL ---
                    const Text("LED Control",
                        style: TextStyle(
                            fontSize: 18, fontWeight: FontWeight.bold)),
                    SwitchListTile(
                      title: const Text("LED Power"),
                      value: _ledOn,
                      activeColor: Colors.amber,
                      onChanged: (val) {
                        setModalState(() => _ledOn = val);
                        setState(() => _ledOn = val);
                        _updateSetting("led", val);
                      },
                    ),
                    if (_ledOn) ...[
                      // Color Presets
                      Builder(builder: (ctx) {
                        void changeColor(Color color) {
                          setModalState(() => _ledColor = color);
                          setState(() => _ledColor = color);

                          int r = (color.r * 255).toInt();
                          int g = (color.g * 255).toInt();
                          int b = (color.b * 255).toInt();
                          String hex =
                              '#${r.toRadixString(16).padLeft(2, '0')}${g.toRadixString(16).padLeft(2, '0')}${b.toRadixString(16).padLeft(2, '0')}'
                                  .toUpperCase();
                          _updateSetting("lcol", hex);
                        }

                        return Wrap(
                          spacing: 10,
                          children: [
                            Colors.white,
                            Colors.red,
                            Colors.green,
                            Colors.blue,
                            Colors.amber,
                            Colors.purple
                          ].map((c) {
                            return GestureDetector(
                              onTap: () => changeColor(c),
                              child: CircleAvatar(
                                  backgroundColor: c,
                                  radius: 15,
                                  child: _ledColor == c
                                      ? const Icon(Icons.check, size: 15)
                                      : null),
                            );
                          }).toList(),
                        );
                      }),
                      const SizedBox(height: 10),
                      Text("Brightness: ${_ledBrightness.toInt()}%"),
                      Slider(
                          value: _ledBrightness,
                          min: 0,
                          max: 100,
                          divisions: 10,
                          onChanged: (val) {
                            setModalState(() => _ledBrightness = val);
                            _updateSetting("lbri", val.toInt());
                          },
                          onChangeEnd: (val) {
                            setState(() => _ledBrightness = val);
                          }),
                      // Color Picker
                      ColorPicker(
                        pickerColor: _ledColor,
                        onColorChanged: (color) {
                          setModalState(() => _ledColor = color);

                          int r = (color.r * 255).toInt();
                          int g = (color.g * 255).toInt();
                          int b = (color.b * 255).toInt();
                          String hex =
                              '#${r.toRadixString(16).padLeft(2, '0')}${g.toRadixString(16).padLeft(2, '0')}${b.toRadixString(16).padLeft(2, '0')}'
                                  .toUpperCase();
                          _updateSetting("lcol",
                              hex); // Debounced via generic handler check
                        },
                        colorPickerWidth: 250,
                        pickerAreaHeightPercent: 0.4,
                        enableAlpha: false,
                        labelTypes: const [],
                      ),
                    ],

                    const SizedBox(height: 20),
                    ElevatedButton(
                      child: const Text("Close"),
                      onPressed: () => Navigator.pop(context),
                    )
                  ],
                ),
              ),
            );
          },
        );
      },
    );
  }

  Future<void> _sendCommand(String action,
      {Map<String, dynamic>? params}) async {
    setState(() => _loadingAction = true);
    try {
      final api = await ref.read(authenticatedApiClientProvider.future);
      await api.sendCommand(widget.device.id, action, params: params);
      if (mounted) {
        String msg = 'Command "$action" sent';
        if (action == "update_settings") msg = "Settings Synced";
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text(msg), duration: const Duration(seconds: 1)),
        );
      }
    } catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('Failed: $e'), backgroundColor: Colors.red),
        );
      }
    } finally {
      if (mounted) setState(() => _loadingAction = false);
    }
  }

  Future<void> _confirmDelete() async {
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Delete Device?'),
        content: Text(
            'Are you sure you want to delete "${widget.device.name}"? This cannot be undone.'),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context, false),
            child: const Text('Cancel'),
          ),
          TextButton(
            onPressed: () => Navigator.pop(context, true),
            style: TextButton.styleFrom(foregroundColor: Colors.red),
            child: const Text('Delete'),
          ),
        ],
      ),
    );

    if (confirmed == true) {
      setState(() => _loadingAction = true);
      try {
        final api = await ref.read(authenticatedApiClientProvider.future);
        await api.deleteDevice(widget.device.id);
        if (mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            const SnackBar(content: Text('Device deleted successfully')),
          );
          Navigator.pop(context, true); // Return true to indicate deletion
        }
      } catch (e) {
        if (mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            SnackBar(
                content: Text('Delete failed: $e'),
                backgroundColor: Colors.red),
          );
        }
      } finally {
        if (mounted) setState(() => _loadingAction = false);
      }
    }
  }

  void _showUpdateDialog() {
    final TextEditingController urlController = TextEditingController(
        text: "https://datum.bezg.in/firmware/firmware.bin");

    showDialog(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text("Update Firmware"),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            const Text(
                "Enter the URL of the new firmware .bin file. The device will download it and restart."),
            const SizedBox(height: 10),
            TextField(
              controller: urlController,
              decoration: const InputDecoration(
                labelText: "Firmware URL",
                border: OutlineInputBorder(),
              ),
            ),
          ],
        ),
        actions: [
          TextButton(
              onPressed: () => Navigator.pop(ctx), child: const Text("Cancel")),
          ElevatedButton(
            onPressed: () {
              Navigator.pop(ctx);
              _sendCommand("update_firmware",
                  params: {"url": urlController.text});
            },
            child: const Text("Update"),
          ),
        ],
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text(widget.device.name),
        actions: [
          IconButton(
            icon: Icon(_isRunning ? Icons.videocam : Icons.videocam_off),
            onPressed: () {
              setState(() {
                _isRunning = !_isRunning;
              });
            },
          ),
          IconButton(
            icon: const Icon(Icons.settings),
            onPressed: _showSettingsDialog,
          ),
          PopupMenuButton<String>(
            onSelected: (value) {
              if (value == 'delete') {
                _confirmDelete();
              } else if (value == 'update') {
                _showUpdateDialog();
              }
            },
            itemBuilder: (BuildContext context) {
              return [
                const PopupMenuItem<String>(
                  value: 'update',
                  child: Row(
                    children: [
                      Icon(Icons.system_update),
                      SizedBox(width: 8),
                      Text('Update Firmware'),
                    ],
                  ),
                ),
                const PopupMenuItem<String>(
                  value: 'delete',
                  child: Row(
                    children: [
                      Icon(Icons.delete, color: Colors.red),
                      SizedBox(width: 8),
                      Text('Delete Device',
                          style: TextStyle(color: Colors.red)),
                    ],
                  ),
                ),
              ];
            },
          ),
        ],
      ),
      body: Column(
        children: [
          // Stream Area
          Container(
            color: Colors.black,
            constraints: const BoxConstraints(maxHeight: 300),
            width: double.infinity,
            child: _isRunning
                ? Stack(
                    alignment: Alignment.bottomRight,
                    children: [
                      StreamRecorder(
                        streamUrl: _getStreamUrl(),
                        controller: _streamController,
                        fit: BoxFit.contain,
                        onError: (err) => Center(
                            child: Text("Error: $err",
                                style: const TextStyle(color: Colors.red))),
                      ),

                      // Recording Indicator (Top Right)
                      if (_streamController.isRecording)
                        Positioned(
                          top: 10,
                          right: 10,
                          child: Container(
                            padding: const EdgeInsets.symmetric(
                                horizontal: 8, vertical: 4),
                            decoration: BoxDecoration(
                                color: Colors.red,
                                borderRadius: BorderRadius.circular(4)),
                            child: Text(
                              "REC ${_streamController.durationString}",
                              style: const TextStyle(
                                  color: Colors.white,
                                  fontWeight: FontWeight.bold),
                            ),
                          ),
                        ),

                      Padding(
                        padding: const EdgeInsets.all(8.0),
                        child: IconButton(
                          onPressed: () async {
                            // Full Screen
                            await Navigator.push(
                              context,
                              MaterialPageRoute(
                                builder: (_) => FullScreenStream(
                                    streamUrl: _getStreamUrl(),
                                    deviceName: widget.device.name),
                              ),
                            );
                          },
                          icon: const Icon(Icons.fullscreen),
                          style: IconButton.styleFrom(
                            backgroundColor: Colors.black54,
                            foregroundColor: Colors.white,
                          ),
                        ),
                      ),
                    ],
                  )
                : const Center(
                    child: Text("Stream Paused",
                        style: TextStyle(color: Colors.white))),
          ),

          Expanded(
            child: ListView(
              padding: const EdgeInsets.all(16),
              children: [
                const Text("Controls",
                    style:
                        TextStyle(fontSize: 18, fontWeight: FontWeight.bold)),
                const SizedBox(height: 20),

                // RESTORED ACTION BUTTONS (Legacy Style)
                Row(
                  mainAxisAlignment: MainAxisAlignment.spaceEvenly,
                  children: [
                    _ActionButton(
                      icon: _ledOn ? Icons.lightbulb : Icons.lightbulb_outline,
                      label: _ledOn ? "LED: ON" : "LED: OFF",
                      color: _ledOn ? Colors.amber : Colors.grey,
                      onPressed: () {
                        // Toggle logic
                        bool newState = !_ledOn;
                        setState(() => _ledOn = newState);
                        _updateSetting("led", newState);
                      },
                      isLoading: _loadingAction,
                    ),
                    _ActionButton(
                      icon: Icons.camera_alt,
                      label: "Photo",
                      color: Colors.blueAccent,
                      onPressed: () async {
                        setState(() => _loadingAction = true);
                        try {
                          // 1. Send Command
                          final api = await ref
                              .read(authenticatedApiClientProvider.future);
                          await api.sendCommand(widget.device.id, "snap",
                              params: {"resolution": _ires});

                          if (context.mounted) {
                            ScaffoldMessenger.of(context).showSnackBar(
                                const SnackBar(
                                    content:
                                        Text("Requesting High-Res Snapshot..."),
                                    duration: Duration(seconds: 1)));
                          }

                          // 2. Poll for snapshot with dimension check
                          // Map Resolution Name to Min Width
                          final resMap = {
                            "QVGA": 320,
                            "VGA": 640,
                            "SVGA": 800,
                            "XGA": 1024,
                            "HD": 1280,
                            "SXGA": 1280,
                            "UXGA": 1600,
                            "QXGA": 2048
                          };
                          final expectedWidth = resMap[_ires] ?? 640;

                          bool success = false;
                          final dio = Dio();
                          if (!context.mounted) return;
                          final token = ref.read(authProvider).value;
                          final url =
                              "https://datum.bezg.in/devices/${widget.device.id}/stream/snapshot";

                          for (int i = 0; i < 20; i++) {
                            // Retry 20 times (approx 10s)
                            await Future.delayed(
                                const Duration(milliseconds: 500));
                            try {
                              final response = await dio.get(url,
                                  options: Options(
                                      responseType: ResponseType.bytes,
                                      headers: {
                                        "Authorization": "Bearer $token"
                                      }),
                                  queryParameters: {
                                    "t": DateTime.now().millisecondsSinceEpoch
                                  });

                              if (response.statusCode == 200) {
                                final Uint8List bytes =
                                    Uint8List.fromList(response.data);

                                // Decode image to check dimensions
                                final codec =
                                    await ui.instantiateImageCodec(bytes);
                                final frame = await codec.getNextFrame();
                                final width = frame.image.width;
                                frame.image.dispose();

                                debugPrint(
                                    "Snapshot Poll $i: Got ${width}px (Expected >= $expectedWidth)");

                                if (width >= expectedWidth) {
                                  // Save to Gallery
                                  await Gal.putImageBytes(bytes);
                                  success = true;
                                  break;
                                }
                              }
                            } catch (e) {
                              debugPrint("Poll Error: $e");
                            }
                          }

                          if (context.mounted) {
                            if (success) {
                              ScaffoldMessenger.of(context).showSnackBar(
                                  const SnackBar(
                                      content: Text(
                                          "High-Res Photo Saved to Gallery!"),
                                      backgroundColor: Colors.green));
                            } else {
                              ScaffoldMessenger.of(context).showSnackBar(
                                  const SnackBar(
                                      content: Text(
                                          "Capture Timed Out or Low Resolution received."),
                                      backgroundColor: Colors.orange));
                            }
                          }
                        } catch (e) {
                          if (context.mounted) {
                            ScaffoldMessenger.of(context).showSnackBar(SnackBar(
                                content: Text("Capture Failed: $e"),
                                backgroundColor: Colors.red));
                          }
                        } finally {
                          if (mounted) setState(() => _loadingAction = false);
                        }
                      },
                      isLoading: _loadingAction,
                    ),
                    _ActionButton(
                      icon: _streamController.isRecording
                          ? Icons.stop_circle
                          : Icons.videocam,
                      label:
                          _streamController.isRecording ? "Stop Rec" : "Record",
                      color: _streamController.isRecording
                          ? Colors.red
                          : Colors.green,
                      onPressed: () async {
                        try {
                          // Local recording from stream
                          if (_streamController.isRecording) {
                            await _streamController.stopRecording?.call();
                            if (context.mounted) {
                              ScaffoldMessenger.of(context).showSnackBar(
                                  const SnackBar(
                                      content: Text("Video Saved to Gallery")));
                            }
                          } else {
                            _streamController.startRecording?.call();
                          }
                        } catch (e) {
                          if (context.mounted) {
                            ScaffoldMessenger.of(context).showSnackBar(SnackBar(
                                content: Text("Recording Error: $e"),
                                backgroundColor: Colors.red));
                          }
                        }
                      },
                      isLoading: _loadingAction,
                    ),
                    _ActionButton(
                      icon: Icons.restart_alt,
                      label: "Restart",
                      color: Colors.redAccent,
                      onPressed: () => _sendCommand("restart"),
                      isLoading: _loadingAction,
                    ),
                  ],
                ),

                const SizedBox(height: 30),

                const Text("Device Telemetry",
                    style:
                        TextStyle(fontSize: 18, fontWeight: FontWeight.bold)),
                const SizedBox(height: 10),
                // Display telemetry data
                if (_deviceData.isEmpty)
                  const Center(child: Text("Waiting for data..."))
                else
                  Container(
                    decoration: BoxDecoration(
                      border: Border.all(color: Colors.grey.shade300),
                      borderRadius: BorderRadius.circular(8),
                    ),
                    child: Column(
                      children: _deviceData.entries.map((e) {
                        // Simplify Keys for display
                        String keyDisplay =
                            e.key.replaceAll('_', ' ').toUpperCase();

                        return Container(
                          padding: const EdgeInsets.symmetric(
                              horizontal: 16, vertical: 12),
                          decoration: BoxDecoration(
                            border: Border(
                                bottom:
                                    BorderSide(color: Colors.grey.shade200)),
                            color: Colors.white,
                          ),
                          child: Row(
                            mainAxisAlignment: MainAxisAlignment.spaceBetween,
                            children: [
                              Text(keyDisplay,
                                  style: TextStyle(
                                      color: Colors.grey.shade600,
                                      fontWeight: FontWeight.bold,
                                      fontSize: 13)),
                              SelectableText(e.value.toString(),
                                  style: const TextStyle(
                                      fontWeight: FontWeight.w600,
                                      fontSize: 15,
                                      color: Colors.black87)),
                            ],
                          ),
                        );
                      }).toList(),
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

class _ActionButton extends StatelessWidget {
  final IconData icon;
  final String label;
  final Color color;
  final VoidCallback onPressed;
  final bool isLoading;

  const _ActionButton({
    required this.icon,
    required this.label,
    required this.color,
    required this.onPressed,
    this.isLoading = false,
  });

  @override
  Widget build(BuildContext context) {
    return Column(
      children: [
        ElevatedButton(
          onPressed: isLoading ? null : onPressed,
          style: ElevatedButton.styleFrom(
            shape: const CircleBorder(),
            padding: const EdgeInsets.all(20),
            backgroundColor: color.withValues(alpha: 0.2),
            foregroundColor: color,
            elevation: 0,
          ),
          child: isLoading
              ? const SizedBox(
                  width: 24,
                  height: 24,
                  child: CircularProgressIndicator(strokeWidth: 2))
              : Icon(icon, size: 30),
        ),
        const SizedBox(height: 8),
        Text(label, style: const TextStyle(fontSize: 12)),
      ],
    );
  }
}
