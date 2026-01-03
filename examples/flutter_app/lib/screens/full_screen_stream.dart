import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'dart:async';
import 'dart:io';
import 'dart:ui' as ui;
import 'package:flutter_quick_video_encoder/flutter_quick_video_encoder.dart';
import 'package:gal/gal.dart';
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
       
       // Calculate FPS
       int targetFps = 10;
       if (durationVal > 0 && _recFrameCount > 0) {
         targetFps = (_recFrameCount / durationVal).round();
       }
       if (targetFps < 1) targetFps = 1;
       
       debugPrint("Processing Video. Frames: $_recFrameCount, Duration: ${durationVal}s, Target FPS: $targetFps");

       if (_recFrameCount < 5) {
          _showSnack("Video too short (needs > 5 frames). Discarded.");
          _finishProcessing();
          return;
       }
       
       try {
         final docDir = await getApplicationDocumentsDirectory();
         final outputPath = "${docDir.path}/output_${DateTime.now().millisecondsSinceEpoch}.mp4";
         
         // 1. Setup Encoder
         // We need width/height from the first frame
         final firstFrameFile = File("$_tempFramesPath/frame_0000.jpg");
         if (!await firstFrameFile.exists()) throw Exception("First frame not found");

         final firstImage = await decodeImageFromList(await firstFrameFile.readAsBytes());
         final width = firstImage.width;
         final height = firstImage.height;

         debugPrint("Initializing Encoder: ${width}x$height @ $targetFps FPS");

         await FlutterQuickVideoEncoder.setup(
            width: width,
            height: height,
            fps: targetFps,
            videoBitrate: 2000000, // 2Mbps high quality
            profileLevel: ProfileLevel.any,
            audioBitrate: 0,
            audioChannels: 0, // No audio
            sampleRate: 44100,
            filepath: outputPath,
         );

         // 2. Loop and Append Frames
         for (int i = 0; i < _recFrameCount; i++) {
             final fileName = "frame_${i.toString().padLeft(4, '0')}.jpg";
             final file = File("$_tempFramesPath/$fileName");
             
             if (await file.exists()) {
                 final bytes = await file.readAsBytes();
                 
                 // Decode JPEG to raw pixels (RGBA)
                 final codec = await ui.instantiateImageCodec(bytes);
                 final frameInfo = await codec.getNextFrame();
                 final rawBytes = await frameInfo.image.toByteData(format: ui.ImageByteFormat.rawRgba);
                 
                 if (rawBytes != null) {
                    await FlutterQuickVideoEncoder.appendVideoFrame(rawBytes.buffer.asUint8List());
                 }
                 frameInfo.image.dispose();
             }
             
             // Update progress occasionally
             if (i % 5 == 0 && mounted) {
                // Could update a progress bar here
             }
         }

         // 3. Finish
         await FlutterQuickVideoEncoder.finish();
         debugPrint("Native Encoding Success");
         
         // Save to Gallery
         try {
            await Gal.putVideo(outputPath);
            _showSnack("Video Saved to Gallery! (Native MP4)");
         } catch (e) {
            _showSnack("Failed to save to Gallery: $e");
         }
         
         // Cleanup Output
         final outFile = File(outputPath);
         if (await outFile.exists()) await outFile.delete();

       } catch (e) {
          _showSnack("Processing Error: $e");
          debugPrint(e.toString());
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
