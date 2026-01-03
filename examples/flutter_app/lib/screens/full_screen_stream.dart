import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'dart:async';
import 'dart:io';
import 'dart:typed_data';
import 'package:gallery_saver/gallery_saver.dart';
import 'package:path_provider/path_provider.dart';

class FullScreenStream extends StatefulWidget {
  final String streamUrl;
  final String deviceName;

  const FullScreenStream({super.key, required this.streamUrl, required this.deviceName});

  @override
  State<FullScreenStream> createState() => _FullScreenStreamState();
}

class _FullScreenStreamState extends State<FullScreenStream> {
  String _timeString = "";
  late Timer _timer;
  bool _showOverlays = true;
  
  // Stream & FPS Logic
  HttpClient? _httpClient;
  StreamSubscription<List<int>>? _streamSubscription;
  Uint8List? _imageBytes;
  int _fps = 0;
  int _frameCount = 0;
  
  // Recording Logic
  bool _isRecording = false;
  IOSink? _recordingSink;
  File? _recordingFile;
  DateTime? _recordingStart;
  String _recDuration = "00:00";

  @override
  void initState() {
    super.initState();
    // Full Screen UI Setup
    SystemChrome.setEnabledSystemUIMode(SystemUiMode.immersiveSticky);
    SystemChrome.setPreferredOrientations([
      DeviceOrientation.landscapeLeft,
      DeviceOrientation.landscapeRight,
    ]);

    _updateTime();
    _timer = Timer.periodic(const Duration(seconds: 1), (Timer t) {
      _updateTime();
      setState(() {
         _fps = _frameCount;
         _frameCount = 0;
         if (_isRecording && _recordingStart != null) {
            final duration = DateTime.now().difference(_recordingStart!);
            final m = duration.inMinutes.toString().padLeft(2, '0');
            final s = (duration.inSeconds % 60).toString().padLeft(2, '0');
            _recDuration = "$m:$s";
         }
      });
    });

    _startStream();
  }

  void _updateTime() {
    final now = DateTime.now();
    setState(() {
      // Date + Time format: dd.MM.yyyy HH:mm:ss
      final date = "${now.day.toString().padLeft(2, '0')}.${now.month.toString().padLeft(2, '0')}.${now.year}";
      final time = "${now.hour.toString().padLeft(2, '0')}:${now.minute.toString().padLeft(2, '0')}:${now.second.toString().padLeft(2, '0')}";
      _timeString = "$date $time";
    });
  }
  
  // --- Custom MJPEG Stream Logic ---
  Future<void> _startStream() async {
    _httpClient = HttpClient();
    _httpClient!.connectionTimeout = const Duration(seconds: 10);
    
    try {
      final request = await _httpClient!.getUrl(Uri.parse(widget.streamUrl));
      final response = await request.close();
      
      List<int> buffer = [];
      
      _streamSubscription = response.listen((data) {
        buffer.addAll(data);
        
        // Simple MJPEG Parser: Find SOI (FF D8) and EOI (FF D9)
        // Optimization: Don't scan from 0 every time, but for simplicity we do it here.
        // Given network chunks, we might have multiple frames or partial frames.
        
        while (true) {
          int start = -1;
          int end = -1;
          
          // Find Start of Image
          for (int i = 0; i < buffer.length - 1; i++) {
            if (buffer[i] == 0xFF && buffer[i+1] == 0xD8) {
              start = i;
              break;
            }
          }
          
          if (start == -1) {
             // Keep a small tail in case split boundary, discard rest? 
             // Ideally we just keep the buffer. But if it grows too large (error), clear it.
             if (buffer.length > 1000000) buffer.clear(); // Safety cap
             break;
          }
          
          // Find End of Image
          for (int i = start; i < buffer.length - 1; i++) {
             if (buffer[i] == 0xFF && buffer[i+1] == 0xD9) {
               end = i + 2; // Include the D9
               break;
             }
          }
          
          if (end != -1) {
            // We have a full frame!
            final frameBytes = Uint8List.fromList(buffer.sublist(start, end));
            
            // Remove this frame from buffer
            buffer.removeRange(0, end);
            
            // Update UI
            if (mounted) {
              setState(() {
                _imageBytes = frameBytes;
                _frameCount++;
              });
            }
            
            // Handle Recording
            if (_isRecording && _recordingSink != null) {
               _recordingSink!.add(frameBytes);
            }
            
          } else {
            // Frame not complete yet
            // If we have leading garbage before start, clean it
            if (start > 0) {
               buffer.removeRange(0, start);
            }
            break;
          }
        }
        
      }, onError: (e) {
         debugPrint("Stream Error: $e");
      }, onDone: () {
         debugPrint("Stream Closed");
      });
      
    } catch (e) {
      debugPrint("Connection Error: $e");
    }
  }

  Future<void> _toggleRecording() async {
     if (_isRecording) {
       // Stop Recording
       await _recordingSink?.flush();
       await _recordingSink?.close();
       _recordingSink = null;
       
       setState(() {
         _isRecording = false;
         _recordingStart = null;
       });
       
       if (_recordingFile != null) {
          // Save to Gallery
          final result = await GallerySaver.saveVideo(_recordingFile!.path);
          if (mounted) {
             ScaffoldMessenger.of(context).showSnackBar(
               SnackBar(content: Text(result == true ? "Saved Video to Gallery!" : "Failed to Save Video")),
             );
          }
          // Cleanup
          // _recordingFile!.delete(); // Optional: keep in app cache or delete
       }
       
     } else {
       // Start Recording
       try {
         final directory = await getApplicationDocumentsDirectory();
         final timestamp = DateTime.now().millisecondsSinceEpoch;
         // .mjpeg extension is rare, .avi is safer for containers but raw MJPEG is essentially .mjpeg
         // GallerySaver might expect .mp4 or .avi. Let's try .avi as containerless MJPEG often works.
         _recordingFile = File('${directory.path}/rec_$timestamp.avi'); 
         
         _recordingSink = _recordingFile!.openWrite();
         
         setState(() {
           _isRecording = true;
           _recordingStart = DateTime.now();
           _recDuration = "00:00";
         });
         
       } catch (e) {
         debugPrint("Data recording failed: $e");
       }
     }
  }

  @override
  void dispose() {
    _timer.cancel();
    _streamSubscription?.cancel();
    _httpClient?.close();
    _recordingSink?.close();
    
    // Restore UI
    SystemChrome.setEnabledSystemUIMode(SystemUiMode.edgeToEdge);
    SystemChrome.setPreferredOrientations([
      DeviceOrientation.portraitUp,
      DeviceOrientation.portraitDown,
      DeviceOrientation.landscapeLeft,
      DeviceOrientation.landscapeRight,
    ]);
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: Colors.black,
      body: GestureDetector(
        onTap: () => setState(() => _showOverlays = !_showOverlays),
        child: Stack(
          children: [
            // Center Stream
            Center(
              child: _imageBytes != null 
                  ? Image.memory(
                      _imageBytes!, 
                      gaplessPlayback: true, // Prevents flickering
                      filterQuality: FilterQuality.medium,
                    ) 
                  : const CircularProgressIndicator(color: Colors.white),
            ),
            
            // Overlays
            if (_showOverlays) ...[
              // Top Bar Gradient
              Positioned(
                top: 0, left: 0, right: 0,
                child: Container(
                  height: 80,
                  decoration: const BoxDecoration(
                    gradient: LinearGradient(
                      colors: [Colors.black54, Colors.transparent],
                      begin: Alignment.topCenter, end: Alignment.bottomCenter,
                    ),
                  ),
                ),
              ),

              // Back Button
              Positioned(
                 top: 20, left: 20,
                 child: SafeArea(
                   child: IconButton(
                     icon: const Icon(Icons.arrow_back, color: Colors.white),
                     onPressed: () => Navigator.pop(context),
                   ),
                 ),
              ),
              
              // Clock + Date (Top-Left)
              Positioned(
                top: 32, left: 70,
                child: SafeArea(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        _timeString, 
                        style: const TextStyle(
                          color: Colors.white, 
                          fontSize: 16, 
                          fontFamily: "monospace",
                          fontWeight: FontWeight.bold,
                          shadows: [Shadow(blurRadius: 2, color: Colors.black)]
                        )
                      ),
                      Text(
                        widget.deviceName,
                        style: const TextStyle(color: Colors.white70, fontSize: 12),
                      )
                    ],
                  ),
                ),
              ),
              
              // Status Indicator + FPS (Top-Right)
              Positioned(
                top: 32, right: 30,
                child: SafeArea(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.end,
                    children: [
                      Container(
                        padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 4),
                        decoration: BoxDecoration(
                          color: Colors.redAccent.withValues(alpha: 0.8),
                          borderRadius: BorderRadius.circular(4),
                        ),
                        child: Row(
                          mainAxisSize: MainAxisSize.min,
                          children: [
                            const Icon(Icons.circle, size: 10, color: Colors.white),
                            const SizedBox(width: 5),
                            Text(
                              "LIVE | $_fps FPS", 
                              style: const TextStyle(
                                color: Colors.white, 
                                fontWeight: FontWeight.bold, 
                                fontSize: 12
                              )
                            ),
                          ],
                        ),
                      ),
                      if (_isRecording)
                         Padding(
                           padding: const EdgeInsets.only(top: 4.0),
                           child: Text(
                             "REC $_recDuration", 
                             style: const TextStyle(color: Colors.red, fontWeight: FontWeight.bold),
                           ),
                         )
                    ],
                  ),
                ),
              ),
              
              // Record Button (Right-Center)
              Positioned(
                right: 30, 
                bottom: MediaQuery.of(context).size.height / 2 - 30,
                child: SafeArea(
                  child: FloatingActionButton(
                    heroTag: "rec_btn",
                    onPressed: _toggleRecording,
                    backgroundColor: _isRecording ? Colors.white : Colors.redAccent,
                    child: Icon(
                      _isRecording ? Icons.stop : Icons.fiber_manual_record, 
                      color: _isRecording ? Colors.red : Colors.white
                    ),
                  ),
                ),
              )
            ],
          ],
        ),
      ),
    );
  }
}
