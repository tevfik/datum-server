import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:flutter_mjpeg/flutter_mjpeg.dart';
import 'package:path_provider/path_provider.dart';
import 'dart:io';
import 'dart:typed_data';
import '../providers/auth_provider.dart';
import '../api_client.dart';
import '../models/device.dart';

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
  List<File> _photos = [];

  @override
  void initState() {
    super.initState();
    // Setup API with token
    final token = Provider.of<AuthProvider>(context, listen: false).token;
    if (token != null) {
      _api.setToken(token);
    }
    _loadPhotos();
  }

  Future<void> _loadPhotos() async {
    try {
      final directory = await getApplicationDocumentsDirectory();
      final photoDir = Directory('${directory.path}/photos');
      if (await photoDir.exists()) {
        setState(() {
          _photos = photoDir.listSync()
              .where((item) => item.path.endsWith(".jpg"))
              .map((item) => File(item.path))
              .toList()
              ..sort((a, b) => b.lastModifiedSync().compareTo(a.lastModifiedSync()));
        });
      }
    } catch (e) {
      debugPrint("Error loading photos: $e");
    }
  }

  String _getStreamUrl() {
    final token = Provider.of<AuthProvider>(context, listen: false).token;
    return 'https://datum.bezg.in/devices/${widget.device.id}/stream/mjpeg?token=$token';
  }

  Future<void> _sendCommand(String action) async {
    setState(() => _loadingAction = true);
    try {
      await _api.sendCommand(widget.device.id, action);
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('Command "$action" sent')),
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
      // 1. Send Snap Command
      await _api.sendCommand(widget.device.id, "snap");
      
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('Requesting High-Res Photo...')),
        );
      }

      // 2. Poll for result (Try for 15 seconds)
      bool success = false;
      if (!mounted) return;
      final token = Provider.of<AuthProvider>(context, listen: false).token;
      
      for (int i = 0; i < 6; i++) { // 6 attempts * 2.5s = 15s max
        if (!mounted) return;
        
        await Future.delayed(const Duration(milliseconds: 2500));
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
                 
                 if (mounted) {
                      ScaffoldMessenger.of(context).showSnackBar(
                       SnackBar(content: Text('Saved to Gallery: ${file.path.split("/").last}')),
                     );
                     _loadPhotos();
                     
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
          )
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
                ? Mjpeg(
                    isLive: true,
                    stream: _getStreamUrl(),
                    error: (context, error, stack) => Center(
                          child: Text("Stream Error: $error", style: const TextStyle(color: Colors.red)),
                        ),
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
                      onPressed: () => _sendCommand("led"),
                      isLoading: _loadingAction,
                    ),
                    _ActionButton(
                      icon: Icons.camera_alt,
                      label: "Take Photo",
                      color: Colors.blueAccent,
                      onPressed: _takePhoto,
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
                
                if (_photos.isNotEmpty) ...[
                  const SizedBox(height: 20),
                  const Text("Gallery", style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold)),
                  const SizedBox(height: 10),
                  SizedBox(
                    height: 120,
                    child: ListView.builder(
                      scrollDirection: Axis.horizontal,
                      itemCount: _photos.length,
                      itemBuilder: (ctx, index) {
                        final file = _photos[index];
                        final name = file.path.split("/").last.replaceAll("snap_", "").replaceAll(".jpg", "").replaceAll("T", " ");
                        return GestureDetector(
                          onTap: () {
                             showDialog(
                                context: context,
                                builder: (ctx) => AlertDialog(
                                  content: Image.file(file),
                                  actions: [
                                    TextButton(onPressed: () => Navigator.pop(ctx), child: const Text("Close")),
                                    TextButton(onPressed: () {
                                         file.deleteSync();
                                         Navigator.pop(ctx);
                                         _loadPhotos();
                                    }, child: const Text("Delete", style: TextStyle(color: Colors.red))),
                                  ],
                                ),
                              );
                          },
                          child: Card(
                            clipBehavior: Clip.antiAlias,
                            child: Column(
                            children: [
                                Expanded(child: Image.file(file, fit: BoxFit.cover)),
                                Padding(padding: const EdgeInsets.all(4), child: Text(name, style: const TextStyle(fontSize: 10)))
                            ],
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
