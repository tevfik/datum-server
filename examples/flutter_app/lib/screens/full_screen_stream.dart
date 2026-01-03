import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'dart:async';
import 'dart:io';
import 'package:gal/gal.dart';
import 'package:path_provider/path_provider.dart';
import 'package:ffmpeg_kit_flutter_full_gpl/ffmpeg_kit.dart';
import 'package:ffmpeg_kit_flutter_full_gpl/return_code.dart';

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
  int _fps = 0; // Display FPS
  int _frameCount = 0; // Counter for FPS calculation
  
  // Recording Logic
  bool _isRecording = false;
  bool _isProcessing = false; // Encoding status
  DateTime? _recordingStart;
  String _recDuration = "00:00";
  String? _tempFramesPath;
  int _recFrameCount = 0; // Frames captured for recording

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
         _frameCount = 0; // Reset counter every second
         
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
        
        while (true) {
          int start = -1;
          int end = -1;
          
          // Find Start of Image (FF D8)
          for (int i = 0; i < buffer.length - 1; i++) {
            if (buffer[i] == 0xFF && buffer[i+1] == 0xD8) {
              start = i;
              break;
            }
          }
          
          if (start == -1) {
             if (buffer.length > 1000000) buffer.clear(); // Safety cap
             break;
          }
          
          // Find End of Image (FF D9)
          for (int i = start; i < buffer.length - 1; i++) {
             if (buffer[i] == 0xFF && buffer[i+1] == 0xD9) {
               end = i + 2;
               break;
             }
          }
          
          if (end != -1) {
            // Full frame found
            final frameBytes = Uint8List.fromList(buffer.sublist(start, end));
            buffer.removeRange(0, end);
            
            // Update UI
            if (mounted) {
              setState(() {
                _imageBytes = frameBytes;
                _frameCount++;
              });
            }
            
            // Save Frame if Recording
            if (_isRecording && _tempFramesPath != null) {
               final fileName = "frame_${_recFrameCount.toString().padLeft(4, '0')}.jpg";
               File("$_tempFramesPath/$fileName").writeAsBytes(frameBytes); // Fire and forgot write
               _recFrameCount++;
            }
            
          } else {
            if (start > 0) buffer.removeRange(0, start);
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
     if (_isProcessing) return; // Prevent double click

     if (_isRecording) {
       // --- STOP RECORDING ---
       final endTime = DateTime.now();
       final durationVal = endTime.difference(_recordingStart!).inMilliseconds / 1000.0;
       
       setState(() {
         _isRecording = false;
         _isProcessing = true; // Start processing UI
       });
       
       // Calculate Real FPS
       double realFps = 10.0; // Default fallback
       if (durationVal > 0 && _recFrameCount > 0) {
         realFps = _recFrameCount / durationVal;
       }
       debugPrint("Recording Stopped. Captured $_recFrameCount frames in ${durationVal}s. Real FPS: $realFps");
       
       if (_recFrameCount < 5) {
          _showSnack("Video too short (needs > 5 frames). Discarded.");
          _finishProcessing();
          return;
       }
       
       try {
         final docDir = await getApplicationDocumentsDirectory();
         final outputPath = "${docDir.path}/output_${DateTime.now().millisecondsSinceEpoch}.mp4";
         
         // FFmpeg Command
         // -framerate: Input FPS
         // -i: Input pattern
         // -c:v libx264: H.264 Encoder (widely supported)
         // -pix_fmt yuv420p: Required for Android compatibility
         final command = "-framerate $realFps -i $_tempFramesPath/frame_%04d.jpg -c:v libx264 -pix_fmt yuv420p $outputPath";
         
         await FFmpegKit.execute(command).then((session) async {
           final returnCode = await session.getReturnCode();
           
           if (ReturnCode.isSuccess(returnCode)) {
              debugPrint("FFmpeg Success");
              // Save to Gallery
              try {
                await Gal.putVideo(outputPath);
                _showSnack("Video Saved to Gallery! (Standard MP4)");
              } catch (e) {
                _showSnack("Failed to save to Gallery: $e");
              }
           } else {
              debugPrint("FFmpeg Failed");
              final logs = await session.getLogs();
              for (var log in logs) { debugPrint(log.getMessage()); }
              _showSnack("Video Encoding Failed.");
           }
         });
         
         // Cleanup Temp Output
         final outFile = File(outputPath);
         if (await outFile.exists()) await outFile.delete();

       } catch (e) {
          _showSnack("Processing Error: $e");
       } finally {
         _finishProcessing();
         // Cleanup Temp Frames
         final dir = Directory(_tempFramesPath!);
         if (await dir.exists()) await dir.delete(recursive: true);
       }
       
     } else {
       // --- START RECORDING ---
       try {
         final docDir = await getApplicationDocumentsDirectory();
         final tempDir = Directory("${docDir.path}/temp_rec_frames");
         
         if (await tempDir.exists()) await tempDir.delete(recursive: true);
         await tempDir.create();
         
         setState(() {
           _tempFramesPath = tempDir.path;
           _recFrameCount = 0;
           _isRecording = true;
           _recordingStart = DateTime.now();
           _recDuration = "00:00";
         });
         
       } catch (e) {
         _showSnack("Failed to init recording: $e");
       }
     }
  }

  void _finishProcessing() {
    if (mounted) {
       setState(() {
         _isProcessing = false;
         _recordingStart = null;
       });
    }
  }

  void _showSnack(String msg) {
    if (mounted) {
       ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text(msg)));
    }
  }

  @override
  void dispose() {
    _timer.cancel();
    _streamSubscription?.cancel();
    _httpClient?.close();
    
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
                      gaplessPlayback: true, 
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
                     onPressed: _isProcessing ? null : () => Navigator.pop(context), // Disable back during processing
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
                          color: _isProcessing 
                             ? Colors.orangeAccent.withValues(alpha: 0.8)
                             : Colors.redAccent.withValues(alpha: 0.8),
                          borderRadius: BorderRadius.circular(4),
                        ),
                        child: Row(
                          mainAxisSize: MainAxisSize.min,
                          children: [
                            if (_isProcessing)
                               const SizedBox(
                                 width: 10, height: 10, 
                                 child: CircularProgressIndicator(color: Colors.white, strokeWidth: 2)
                               )
                            else 
                               const Icon(Icons.circle, size: 10, color: Colors.white),
                            
                            const SizedBox(width: 5),
                            Text(
                              _isProcessing ? "PROCESSING..." : "LIVE | $_fps FPS", 
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
                    onPressed: _isProcessing ? null : _toggleRecording,
                    backgroundColor: _isRecording ? Colors.white : Colors.redAccent,
                    child: Icon(
                      _isRecording ? Icons.stop : Icons.fiber_manual_record, 
                      color: _isRecording ? Colors.red : Colors.white
                    ),
                  ),
                ),
              )
            ],
            
            // Blocking Processing Overlay (Full Screen if needed, but we used small badge)
            if (_isProcessing)
               Container(
                 color: Colors.black54,
                 child: const Center(
                   child: Column(
                     mainAxisSize: MainAxisSize.min,
                     children: [
                       CircularProgressIndicator(color: Colors.white),
                       SizedBox(height: 10),
                       Text("Saving Video...", style: TextStyle(color: Colors.white, fontSize: 16))
                     ],
                   ),
                 ),
               )
          ],
        ),
      ),
    );
  }
}
