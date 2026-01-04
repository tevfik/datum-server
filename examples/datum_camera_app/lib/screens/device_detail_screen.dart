import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:flutter_colorpicker/flutter_colorpicker.dart';

import 'full_screen_stream.dart';
import '../providers/auth_provider.dart';
import '../api_client.dart';
import '../models/device.dart';
import '../widgets/stream_recorder.dart';

class DeviceDetailScreen extends StatefulWidget {
  final Device device;

  const DeviceDetailScreen({super.key, required this.device});

  @override
  State<DeviceDetailScreen> createState() => _DeviceDetailScreenState();
}

class _DeviceDetailScreenState extends State<DeviceDetailScreen> {
  final ApiClient _api = ApiClient();
  bool _isRunning = true; // For MJPEG stream
  bool _loadingAction = false;

  // Stream Recording
  final StreamRecorderController _streamController = StreamRecorderController();

  // Device Data (Telemetry)
  Map<String, dynamic> _deviceData = {};

  // State for settings (Initialized with defaults, updated from telemetry)
  String _streamRes = "VGA";
  String _snapRes = "UXGA"; 
  Color _ledColor = Colors.white;
  double _ledBrightness = 100;
  bool _hmirror = false;
  bool _vflip = false;
  bool _ledOn = true; // LED Power state

  @override
  void initState() {
    super.initState();
    final token = Provider.of<AuthProvider>(context, listen: false).token;
    if (token != null) {
      _api.setToken(token);
    }
    
    _streamController.addListener(() {
       if (mounted) setState(() {});
    });

    _pollData();
  }

  Future<void> _pollData() async {
    try {
      final data = await _api.getDeviceData(widget.device.id);
      if (!mounted) return;
      setState(() {
        _deviceData = data;
        
        // Sync state from telemetry if available
        if (_deviceData.containsKey('led_brightness')) {
           _ledBrightness = (_deviceData['led_brightness'] as num).toDouble();
        }
        if (_deviceData.containsKey('resolution')) {
          _streamRes = _deviceData['resolution'];
        }
        if (_deviceData.containsKey('hmirror')) {
          _hmirror = _deviceData['hmirror'];
        }
         if (_deviceData.containsKey('vflip')) {
          _vflip = _deviceData['vflip'];
        }
        // Parse Color
        if (_deviceData.containsKey('led_color')) {
           String hex = _deviceData['led_color'];
           if (hex.startsWith('#') && hex.length == 7) {
             _ledColor = Color(int.parse("0xFF${hex.substring(1)}"));
           }
        }
      });
    } catch (e) {
      // debugPrint("Poll error: $e");
    }
  }

  String _getStreamUrl() {
    final token = Provider.of<AuthProvider>(context, listen: false).token;
    return 'https://datum.bezg.in/devices/${widget.device.id}/stream/mjpeg?token=$token';
  }

  void _sendSettings() {
     // Convert Color to Hex string #RRGGBB
     int r = (_ledColor.r * 255).toInt();
     int g = (_ledColor.g * 255).toInt();
     int b = (_ledColor.b * 255).toInt();
     // Fix: Pad left with 0 to ensure valid hex (e.g., 0A instead of A)
     String hex = '#${r.toRadixString(16).padLeft(2, '0')}${g.toRadixString(16).padLeft(2, '0')}${b.toRadixString(16).padLeft(2, '0')}'.toUpperCase();
     
     final params = {
       "resolution": _streamRes,
       "led_color": hex,
       "led_brightness": _ledOn ? _ledBrightness.toInt() : 0,
       "hmirror": _hmirror,
       "vflip": _vflip
     };
     
     // CRITICAL: Send "update_settings" action, NOT generic action
     _sendCommand("update_settings", params: params);
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
              height: 700, // Increased height for ColorPicker
              child: SingleChildScrollView(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    const Text("Device Settings", style: TextStyle(fontSize: 20, fontWeight: FontWeight.bold)),
                    const SizedBox(height: 20),
                    
                    const Text("Stream Resolution"),
                    DropdownButton<String>(
                      value: _streamRes,
                      isExpanded: true,
                      items: ["QVGA", "VGA", "SVGA", "HD"].map((String value) {
                        return DropdownMenuItem<String>(
                          value: value,
                          child: Text(value),
                        );
                      }).toList(),
                      onChanged: (newValue) {
                        if (newValue != null) {
                          setModalState(() => _streamRes = newValue);
                          setState(() => _streamRes = newValue);
                          // Offline: Do not send immediately
                        }
                      },
                    ),
                    
                    const SizedBox(height: 15),
                    const Text("Snapshot Resolution"),
                    DropdownButton<String>(
                      value: _snapRes,
                      isExpanded: true,
                      items: ["HD", "UXGA", "QXGA"].map((String value) {
                        return DropdownMenuItem<String>(
                          value: value,
                          child: Text(value),
                        );
                      }).toList(),
                      onChanged: (newValue) {
                        if (newValue != null) {
                          setModalState(() => _snapRes = newValue);
                          setState(() => _snapRes = newValue);
                        }
                      },
                    ),

                    const Divider(),
                    const Text("Orientation"),
                    SwitchListTile(
                      title: const Text("Mirror Horizontal"),
                      value: _hmirror,
                      onChanged: (val) {
                         setModalState(() => _hmirror = val);
                         setState(() => _hmirror = val);
                      },
                    ),
                     SwitchListTile(
                      title: const Text("Flip Vertical"),
                      value: _vflip,
                      onChanged: (val) {
                         setModalState(() => _vflip = val);
                         setState(() => _vflip = val);
                      },
                    ),

                    const Divider(),
                    const Text("LED Control"),
                    const SizedBox(height: 10),
                    
                    // Full Color Picker
                    ColorPicker(
                      pickerColor: _ledColor,
                      onColorChanged: (color) {
                         setModalState(() => _ledColor = color);
                         setState(() => _ledColor = color); // Update parent state too
                      },
                      colorPickerWidth: 300,
                      pickerAreaHeightPercent: 0.7,
                      enableAlpha: false,
                      labelTypes: const [], // Hide hex input
                      paletteType: PaletteType.hueWheel,
                    ),
                    
                    const SizedBox(height: 10),
                    Text("Brightness: ${_ledBrightness.toInt()}%"),
                    Slider(
                      value: _ledBrightness,
                      min: 0,
                      max: 100,
                      divisions: 10,
                      label: _ledBrightness.round().toString(),
                      onChanged: (double value) {
                        setModalState(() => _ledBrightness = value);
                      },
                      onChangeEnd: (double value) {
                         setState(() => _ledBrightness = value);
                      },
                    ),
                    
                    const SizedBox(height: 20),
                    
                    ElevatedButton.icon(
                        onPressed: () {
                           Navigator.pop(context);
                           _sendSettings();
                        },
                        icon: const Icon(Icons.send),
                        label: const Text("Sync Settings"),
                        style: ElevatedButton.styleFrom(
                            minimumSize: const Size.fromHeight(40),
                            backgroundColor: Colors.blueAccent,
                            foregroundColor: Colors.white,
                        ),
                    ),
                    
                    const SizedBox(height: 10),
                    SwitchListTile(
                      title: const Text("LED Power"),
                      subtitle: const Text("Toggle Light On/Off"),
                      value: _ledOn,
                      activeColor: Colors.amber,
                      onChanged: (val) {
                          setModalState(() => _ledOn = val);
                          setState(() => _ledOn = val);
                          _sendSettings(); // Toggle sends immediately
                      },
                    ),
                    const SizedBox(height: 20),
                  ],
                ),
              ),
            );
          },
        );
      },
    );
  }
  
  Future<void> _sendCommand(String action, {Map<String, dynamic>? params}) async {
    setState(() => _loadingAction = true);
    try {
      await _api.sendCommand(widget.device.id, action, params: params);
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
        content: Text('Are you sure you want to delete "${widget.device.name}"? This cannot be undone.'),
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
        await _api.deleteDevice(widget.device.id);
        if (mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            const SnackBar(content: Text('Device deleted successfully')),
          );
          Navigator.pop(context, true); // Return true to indicate deletion
        }
      } catch (e) {
        if (mounted) {
           ScaffoldMessenger.of(context).showSnackBar(
            SnackBar(content: Text('Delete failed: $e'), backgroundColor: Colors.red),
          );
        }
      } finally {
        if (mounted) setState(() => _loadingAction = false);
      }
    }
  }

  void _showUpdateDialog() {
    final TextEditingController urlController = TextEditingController(text: "https://datum.bezg.in/firmware/firmware.bin");
    
    showDialog(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text("Update Firmware"),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            const Text("Enter the URL of the new firmware .bin file. The device will download it and restart."),
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
          TextButton(onPressed: () => Navigator.pop(ctx), child: const Text("Cancel")),
          ElevatedButton(
            onPressed: () {
               Navigator.pop(ctx);
               _sendCommand("update_firmware", params: {"url": urlController.text});
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
                      Text('Delete Device', style: TextStyle(color: Colors.red)),
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
                        onError: (err) => Center(child: Text("Error: $err", style: const TextStyle(color: Colors.red))),
                      ),
                      
                      // Recording Indicator (Top Right)
                      if (_streamController.isRecording)
                        Positioned(
                          top: 10, right: 10,
                          child: Container(
                            padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 4),
                            decoration: BoxDecoration(color: Colors.red, borderRadius: BorderRadius.circular(4)),
                            child: Text(
                              "REC ${_streamController.durationString}",
                              style: const TextStyle(color: Colors.white, fontWeight: FontWeight.bold),
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
                                   deviceName: widget.device.name
                                 ),
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
                : const Center(child: Text("Stream Paused", style: TextStyle(color: Colors.white))),
          ),
          
          Expanded(
            child: ListView(
              padding: const EdgeInsets.all(16),
              children: [
                const Text("Controls", style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold)),
                const SizedBox(height: 20),
                
                // RESTORED ACTION BUTTONS (Legacy Style)
                Row(
                  mainAxisAlignment: MainAxisAlignment.spaceEvenly,
                  children: [
                    _ActionButton(
                      icon: Icons.lightbulb,
                      label: "Toggle LED",
                      color: Colors.amber,
                      onPressed: () {
                          // Toggle logic: If brightness > 0, set to 0. Else max.
                          if (_ledBrightness > 0) {
                             setState(() => _ledBrightness = 0);
                          } else {
                             setState(() => _ledBrightness = 100);
                             setState(() => _ledOn = true);
                          }
                          _sendSettings();
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
                           await _api.sendCommand(widget.device.id, "snap", params: {"resolution": _snapRes});
                           
                           // 2. Wait for FW to process and Frame to arrive (FW takes ~2s, waits 4s)
                           if (context.mounted) {
                             ScaffoldMessenger.of(context).showSnackBar(const SnackBar(content: Text("Capturing High-Res..."), duration: Duration(seconds: 2)));
                           }
                           await Future.delayed(const Duration(seconds: 3));
                           
                           // 3. Save the frame currently on stream (which should be the high-res one)
                           await _streamController.takeSnapshot?.call();
                           
                           if (context.mounted) {
                             ScaffoldMessenger.of(context).showSnackBar(const SnackBar(content: Text("High-Res Photo Saved to Gallery!")));
                           }
                         } catch (e) {
                            if (context.mounted) {
                              ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text("Capture Failed: $e"), backgroundColor: Colors.red));
                            }
                         } finally {
                            if (mounted) setState(() => _loadingAction = false);
                         }
                      },
                      isLoading: _loadingAction,
                    ),
                    _ActionButton(
                      icon: _streamController.isRecording ? Icons.stop_circle : Icons.videocam,
                      label: _streamController.isRecording ? "Stop Rec" : "Record",
                      color: _streamController.isRecording ? Colors.red : Colors.green,
                      onPressed: () async {
                         try {
                           // Local recording from stream
                           if (_streamController.isRecording) {
                              await _streamController.stopRecording?.call();
                              if (context.mounted) {
                                ScaffoldMessenger.of(context).showSnackBar(const SnackBar(content: Text("Video Saved to Gallery")));
                              }
                           } else {
                              _streamController.startRecording?.call();
                           }
                         } catch (e) {
                            if (context.mounted) {
                               ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text("Recording Error: $e"), backgroundColor: Colors.red));
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
                
                const Text("Device Telemetry", style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold)),
                const SizedBox(height: 10),
                // Display telemetry data
                if (_deviceData.isEmpty)
                   const Center(child: Text("Waiting for data..."))
                else
                   Wrap(
                     spacing: 10, runSpacing: 10,
                     children: _deviceData.entries.map((e) {
                        return Chip(
                          label: Text(
                             "${e.key}: ${e.value}", 
                             style: const TextStyle(color: Colors.black, fontWeight: FontWeight.w500)
                          ),
                          backgroundColor: Colors.white,
                          shape: const StadiumBorder(side: BorderSide(color: Colors.grey)),
                          elevation: 1,
                        );
                     }).toList(),
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
            ? const SizedBox(width: 24, height: 24, child: CircularProgressIndicator(strokeWidth: 2)) 
            : Icon(icon, size: 30),
        ),
        const SizedBox(height: 8),
        Text(label, style: const TextStyle(fontSize: 12)),
      ],
    );
  }
}
