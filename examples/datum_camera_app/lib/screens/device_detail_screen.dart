import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:flutter_mjpeg/flutter_mjpeg.dart';
import 'package:path_provider/path_provider.dart';
import 'dart:io';
import 'package:gal/gal.dart';
import 'package:share_plus/share_plus.dart';
import 'package:open_file_plus/open_file_plus.dart';
import 'dart:typed_data';
import 'full_screen_stream.dart';
import '../providers/auth_provider.dart';
import '../api_client.dart';
import 'package:video_player/video_player.dart';
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
  List<File> _mediaFiles = [];

  bool _loadingAction = false;
  List<File> _mediaFiles = [];
  
  // Stream Recording
  final StreamRecorderController _streamController = StreamRecorderController();

  @override
  void initState() {
    super.initState();
    // Setup API with token
    final token = Provider.of<AuthProvider>(context, listen: false).token;
    if (token != null) {
      _api.setToken(token);
    }
    _loadMedia();
    
    // Listen to recording state changes for UI updates
    _streamController.addListener(() {
       if (mounted) setState(() {});
    });
  }

  Future<void> _loadMedia() async {
    try {
      final directory = await getApplicationDocumentsDirectory();
      final photoDir = Directory('${directory.path}/photos');
      if (await photoDir.exists()) {
        final photos = photoDir.listSync()
              .where((item) => item.path.endsWith(".jpg"))
              .map((item) => File(item.path))
              .toList();
        
        // Also check for videos in documents root (where we store them temporarily) or photos details?
        // In FullScreenStream we used getApplicationDocumentsDirectory() root for videos.
        final docs = directory.listSync()
              .where((item) => item.path.endsWith(".mp4") || item.path.endsWith(".avi"))
              .map((item) => File(item.path))
              .toList();
              
        setState(() {
          _mediaFiles = [...photos, ...docs]
              ..sort((a, b) => b.lastModifiedSync().compareTo(a.lastModifiedSync()));
        });
      }
    } catch (e) {
      debugPrint("Error loading media: $e");
    }
  }

  String _getStreamUrl() {
    final token = Provider.of<AuthProvider>(context, listen: false).token;
    return 'https://datum.bezg.in/devices/${widget.device.id}/stream/mjpeg?token=$token';
  }

  // State for settings
  String _streamRes = "VGA";
  String _snapRes = "UXGA"; 
  Color _ledColor = Colors.white;
  double _ledBrightness = 100;
  bool _hmirror = false;
  bool _vflip = false;
  bool _ledOn = true; // LED Power state
  
  void _sendSettings() {
     // Convert Color to Hex string #RRGGBB using component accessors (Fix Deprecation)
     int r = (_ledColor.r * 255).toInt();
     int g = (_ledColor.g * 255).toInt();
     int b = (_ledColor.b * 255).toInt();
     String hex = '#${r.toRadixString(16).padLeft(2, '0')}${g.toRadixString(16).padLeft(2, '0')}${b.toRadixString(16).padLeft(2, '0')}'.toUpperCase();
     
     final params = {
       "resolution": _streamRes,
       "led_color": hex,
       "led_brightness": _ledOn ? _ledBrightness.toInt() : 0,
       "hmirror": _hmirror,
       "vflip": _vflip
     };
     
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
              height: 600,
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
                       // Offline: Do not send immediately
                    },
                  ),
                   SwitchListTile(
                    title: const Text("Flip Vertical"),
                    value: _vflip,
                    onChanged: (val) {
                       setModalState(() => _vflip = val);
                       setState(() => _vflip = val);
                       // Offline: Do not send immediately
                    },
                  ),

                  const Divider(),
                  const Text("LED Control"),
                  const SizedBox(height: 10),
                  
                  // Simple Color Picker (Red, Green, Blue, White)
                  Row(
                    mainAxisAlignment: MainAxisAlignment.spaceEvenly,
                    children: [
                      _colorBtn(Colors.white, setModalState),
                      _colorBtn(Colors.red, setModalState),
                      _colorBtn(Colors.green, setModalState),
                      _colorBtn(Colors.blue, setModalState),
                    ],
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
                       // Offline: Do not send immediately
                    },
                  ),
                  
                  const SizedBox(height: 20),
                  
                  ElevatedButton.icon(
                      onPressed: _sendSettings,
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
                        _sendSettings(); // Toggle sends immediately for responsiveness
                    },
                  ),
                ],
              ),
            );
          },
        );
      },
    );
  }
  
  Widget _colorBtn(Color color, StateSetter setModalState) {
    return GestureDetector(
      onTap: () {
        setModalState(() => _ledColor = color);
        setState(() => _ledColor = color); 
        // Offline: Do not send immediately
      },
      child: Container(
        width: 40, height: 40,
        decoration: BoxDecoration(
          color: color,
          shape: BoxShape.circle,
          border: Border.all(color: _ledColor == color ? Colors.amber : Colors.grey, width: 3),
        ),
      ),
    );
  }



  Future<void> _sendCommand(String action, {Map<String, dynamic>? params}) async {
    setState(() => _loadingAction = true);
    try {
      await _api.sendCommand(widget.device.id, action, params: params);
      if (mounted) {
        String msg = 'Command "$action" sent';
        if (action == "set_resolution") msg = "Resolution set to ${params?['resolution']}";
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

  Future<void> _takePhoto() async {
    setState(() => _loadingAction = true);
    try {
      // 1. Send Snap Command with resolution
      await _api.sendCommand(widget.device.id, "snap", params: {"resolution": _snapRes});
      
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
           SnackBar(content: Text('Capturing $_snapRes Photo...')),
        );
      }

      // 2. Poll for result (Try for 15 seconds)
      bool success = false;
      if (!mounted) return;
      final token = Provider.of<AuthProvider>(context, listen: false).token;
      
      // Wait for ESP32 to upload (it takes ~2-5s)
      // Poll faster (every 500ms) to catch the 4s window
      for (int i = 0; i < 40; i++) { 
        if (!mounted) return;
        
        await Future.delayed(const Duration(milliseconds: 500));
        if (!mounted) return;
        
        try {
          final imageUrl = 'https://datum.bezg.in/devices/${widget.device.id}/stream/snapshot?token=$token&t=${DateTime.now().millisecondsSinceEpoch}';
          
          final request = await HttpClient().getUrl(Uri.parse(imageUrl));
          final response = await request.close();
          
          if (response.statusCode == 200) {
              final bytes = (await response.fold<BytesBuilder>(BytesBuilder(), (b, d) => b..add(d))).takeBytes();
              
              // Verify size (Snapshots should be > 20KB usually, streams are smaller)
              // This ensures we don't accidentally grab an old low-res frame if stream is active
              if (bytes.length > 20000) { 
                 final directory = await getApplicationDocumentsDirectory();
                 final photoDir = Directory('${directory.path}/photos');
                 if (!await photoDir.exists()) await photoDir.create(recursive: true);
                 
                 final timestamp = DateTime.now().toIso8601String().replaceAll(':', '-').split('.').first;
                 final file = File('${photoDir.path}/snap_$timestamp.jpg');
                 await file.writeAsBytes(bytes);
                 
                 try {
                     await Gal.putImage(file.path);
                     if (mounted) {
                          ScaffoldMessenger.of(context).showSnackBar(
                           SnackBar(content: Text('Saved to Gallery: ${file.path.split("/").last}')),
                         );
                     }
                 } catch (e) {
                     if (mounted) {
                          ScaffoldMessenger.of(context).showSnackBar(
                           SnackBar(content: Text('Saved to App Folder (Gallery Failed: $e)')),
                         );
                     }
                 }
                 
                  if (mounted) {
                     _loadMedia();
                     
                     showDialog(
                       context: context,
                       builder: (ctx) => AlertDialog(
                         title: const Text("Captured"),
                         content: Image.file(file),
                         actions: [
                           TextButton(onPressed: () => Navigator.pop(ctx), child: const Text("Close")),
                         ],
                       ),
                     );
                 }
                 success = true;
                 break; // Success!
              }
          }
        } catch (e) {
           debugPrint("Retry $i failed: $e");
        }
      }
      
      if (!success && mounted) {
           throw Exception("Snapshot took too long or failed.");
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
          if (e.toString().contains("204")) {
             // Handle 204 No Content as success
             Navigator.pop(context, true);
             return;
          }
          ScaffoldMessenger.of(context).showSnackBar(
            SnackBar(content: Text('Delete failed: $e'), backgroundColor: Colors.red),
          );
        }
      } finally {
        if (mounted) setState(() => _loadingAction = false);
      }
    }
  }

  void _openMedia(File file) {
     final isVideo = file.path.endsWith('.mp4');
     
     showDialog(
       context: context,
       builder: (ctx) => AlertDialog(
         contentPadding: EdgeInsets.zero,
         content: Column(
           mainAxisSize: MainAxisSize.min,
           children: [
             if (isVideo)
                Stack(
                  alignment: Alignment.center,
                  children: [
                    if (File(file.path.replaceAll(".mp4", ".jpg")).existsSync())
                      Image.file(File(file.path.replaceAll(".mp4", ".jpg")), height: 200, width: double.infinity, fit: BoxFit.cover)
                    else
                      Container(
                        color: Colors.black,
                        height: 200, 
                        width: double.infinity,
                      ),
                    const Icon(Icons.play_circle_fill, size: 64, color: Colors.white70),
                  ],
                )
             else 
               Image.file(file),
               
             Padding(
               padding: const EdgeInsets.all(8.0),
               child: Text(file.path.split('/').last, style: const TextStyle(fontWeight: FontWeight.bold)),
             )
           ],
         ),
         actions: [
            TextButton.icon(
              icon: const Icon(Icons.share),
              label: const Text("Share"),
              onPressed: () {
                 Share.shareXFiles([XFile(file.path)]);
               },
            ),
              if (isVideo)
                TextButton.icon(
                  icon: const Icon(Icons.play_arrow),
                  label: const Text("Play Preview"),
                  onPressed: () {
                    Navigator.pop(ctx); // Close dialog
                    Navigator.push(context, MaterialPageRoute(builder: (_) => VideoPreviewScreen(videoFile: file)));
                  },
                )
            else 
               // For images, we already show it, but 'Open' can open in external viewer too
              TextButton.icon(
                icon: const Icon(Icons.open_in_new),
                label: const Text("Open"),
                onPressed: () {
                  OpenFile.open(file.path);
                },
              ),
              
            TextButton(
              onPressed: () {
                 file.deleteSync();
                 Navigator.pop(ctx);
                 _loadMedia();
              }, 
              child: const Text("Delete", style: TextStyle(color: Colors.red))
            ),
            TextButton(onPressed: () => Navigator.pop(ctx), child: const Text("Close")),
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
              }
            },
            itemBuilder: (BuildContext context) {
              return [
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
                             // Wait for return to reload gallery (in case video was recorded)
                             await Navigator.push(
                               context,
                               MaterialPageRoute(
                                 builder: (_) => FullScreenStream(
                                   streamUrl: _getStreamUrl(), 
                                   deviceName: widget.device.name
                                 ),
                               ),
                             );
                             _loadMedia();
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
                const SizedBox(height: 10),
                
                Row(
                  mainAxisAlignment: MainAxisAlignment.spaceEvenly,
                  children: [
                    _ActionButton(
                      icon: Icons.lightbulb,
                      label: "Toggle LED",
                      color: Colors.amber,
                      onPressed: () {
                          if (_ledBrightness > 0) {
                             setState(() => _ledBrightness = 0);
                          } else {
                             setState(() => _ledBrightness = 100);
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
                         try {
                           await _streamController.takeSnapshot?.call();
                           ScaffoldMessenger.of(context).showSnackBar(const SnackBar(content: Text("Photo Saved to Gallery")));
                           _loadMedia();
                         } catch (e) {
                           ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text("Error: $e")));
                         }
                      },
                      isLoading: _loadingAction,
                    ),
                    _ActionButton(
                      icon: _streamController.isRecording ? Icons.stop_circle : Icons.videocam,
                      label: _streamController.isRecording ? "Stop" : "Record",
                      color: _streamController.isRecording ? Colors.red : Colors.green,
                      onPressed: () async {
                         if (_streamController.isRecording) {
                            try {
                              setState(() => _loadingAction = true);
                              await _streamController.stopRecording?.call();
                              ScaffoldMessenger.of(context).showSnackBar(const SnackBar(content: Text("Video Saved to Gallery")));
                              _loadMedia();
                            } catch (e) {
                              ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text("Error: $e")));
                            } finally {
                              setState(() => _loadingAction = false);
                            }
                         } else {
                            _streamController.startRecording?.call();
                         }
                      },
                      isLoading: _loadingAction || _streamController.isProcessing,
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
                
                const SizedBox(height: 20),
                const Text("Device Info", style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold)),
                Card(
                  child: ListTile(
                    title: Text("ID: ${widget.device.id}"),
                    subtitle: Text("Type: ${widget.device.type}\nStatus: ${widget.device.status}"),
                    leading: const Icon(Icons.info_outline),
                    isThreeLine: true,
                  ),
                ),
                
                if (_mediaFiles.isNotEmpty) ...[
                  const SizedBox(height: 20),
                  const Text("Device Gallery", style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold)),
                  const SizedBox(height: 10),
                  SizedBox(
                    height: 120,
                    child: ListView.builder(
                      scrollDirection: Axis.horizontal,
                      itemCount: _mediaFiles.length,
                      itemBuilder: (ctx, index) {
                        final file = _mediaFiles[index];
                        final isVideo = file.path.endsWith(".mp4");
                        final name = file.path.split("/").last.replaceAll("snap_", "").replaceAll(".jpg", "").replaceAll(".mp4", "");
                        
                        return GestureDetector(
                          onTap: () => _openMedia(file),
                          child: Card(
                            clipBehavior: Clip.antiAlias,
                            child: SizedBox(
                              width: 120,
                              child: Stack(
                                fit: StackFit.expand,
                                children: [
                                  if (isVideo)

                                     Stack(
                                       fit: StackFit.expand,
                                       children: [
                                          if (File(file.path.replaceAll(".mp4", ".jpg")).existsSync())
                                            Image.file(File(file.path.replaceAll(".mp4", ".jpg")), fit: BoxFit.cover)
                                          else
                                            Container(color: Colors.black),
                                          const Center(child: Icon(Icons.play_circle_outline, color: Colors.white, size: 40)),
                                       ],
                                     )
                                  else
                                     Image.file(file, fit: BoxFit.cover),
                                     
                                  Positioned(
                                    bottom: 0, left: 0, right: 0,
                                    child: Container(
                                      color: Colors.black54,
                                      padding: const EdgeInsets.all(2),
                                      child: Text(name, style: const TextStyle(color: Colors.white, fontSize: 10), textAlign: TextAlign.center, overflow: TextOverflow.ellipsis),
                                    ),
                                  )
                                ],
                              ),
                            ),
                          ),
                        );
                      },
                    ),
                  )
                ],
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
            padding: const EdgeInsets.all(24),
            backgroundColor: color.withValues(alpha: 0.2), 
            foregroundColor: color, 
          ),
          child: isLoading 
            ? const SizedBox(width: 24, height: 24, child: CircularProgressIndicator(strokeWidth: 2)) 
            : Icon(icon, size: 32),
        ),
        const SizedBox(height: 8),
        Text(label),
      ],
    );
  }
}

class VideoPreviewScreen extends StatefulWidget {
  final File videoFile;

  const VideoPreviewScreen({super.key, required this.videoFile});

  @override
  State<VideoPreviewScreen> createState() => _VideoPreviewScreenState();
}

class _VideoPreviewScreenState extends State<VideoPreviewScreen> {
  late VideoPlayerController _controller;

  @override
  void initState() {
    super.initState();
    _controller = VideoPlayerController.file(widget.videoFile)
      ..initialize().then((_) {
        setState(() {});
        _controller.play();
      });
  }

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: Colors.black,
      appBar: AppBar(
        backgroundColor: Colors.transparent,
        iconTheme: const IconThemeData(color: Colors.white),
      ),
      body: Center(
        child: _controller.value.isInitialized
            ? AspectRatio(
                aspectRatio: _controller.value.aspectRatio,
                child: Stack(
                  alignment: Alignment.bottomCenter,
                  children: [
                    VideoPlayer(_controller),
                    VideoProgressIndicator(_controller, allowScrubbing: true),
                    Center(
                      child: IconButton(
                        icon: Icon(
                          _controller.value.isPlaying ? Icons.pause_circle : Icons.play_circle,
                          color: Colors.white.withValues(alpha: 0.5),
                          size: 64,
                        ),
                        onPressed: () {
                          setState(() {
                            _controller.value.isPlaying
                                ? _controller.pause()
                                : _controller.play();
                          });
                        },
                      ),
                    ),
                  ],
                ),
              )
            : const CircularProgressIndicator(),
      ),
    );
  }
}
