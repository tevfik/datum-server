import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'dart:async';
import '../widgets/stream_recorder.dart';

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
  final StreamRecorderController _streamController = StreamRecorderController();

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
    });
    
    // Listen to updates from stream controller (for FPS/Duration)
    _streamController.addListener(() {
      if (mounted) setState(() {});
    });
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
  
  void _showSnack(String msg) {
    if (mounted) {
       ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text(msg)));
    }
  }

  @override
  void dispose() {
    _timer.cancel();
    
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
            StreamRecorder(
               streamUrl: widget.streamUrl,
               controller: _streamController,
               fit: BoxFit.contain, // Maintain aspect ratio in full screen
               onError: (e) => Center(child: Text("Stream Error: $e", style: const TextStyle(color: Colors.white))),
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
                     onPressed: _streamController.isProcessing ? null : () => Navigator.pop(context), 
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
                          color: _streamController.isProcessing 
                             ? Colors.orangeAccent.withValues(alpha: 0.8)
                             : Colors.redAccent.withValues(alpha: 0.8),
                          borderRadius: BorderRadius.circular(4),
                        ),
                        child: Row(
                          mainAxisSize: MainAxisSize.min,
                          children: [
                            if (_streamController.isProcessing)
                               const SizedBox(
                                 width: 10, height: 10, 
                                 child: CircularProgressIndicator(color: Colors.white, strokeWidth: 2)
                               )
                            else 
                               const Icon(Icons.circle, size: 10, color: Colors.white),
                            
                            const SizedBox(width: 5),
                            Text(
                              _streamController.isProcessing ? "PROCESSING..." : "LIVE | ${_streamController.fps} FPS", 
                              style: const TextStyle(
                                color: Colors.white, 
                                fontWeight: FontWeight.bold, 
                                fontSize: 12
                              )
                            ),
                          ],
                        ),
                      ),
                      if (_streamController.isRecording)
                         Padding(
                           padding: const EdgeInsets.only(top: 4.0),
                           child: Text(
                             "REC ${_streamController.durationString}", 
                             style: const TextStyle(color: Colors.red, fontWeight: FontWeight.bold),
                           ),
                         )
                    ],
                  ),
                ),
              ),
              
              // Controls (Right-Center)
              Positioned(
                right: 30, 
                bottom: MediaQuery.of(context).size.height / 2 - 60,
                child: SafeArea(
                  child: Column(
                      mainAxisSize: MainAxisSize.min,
                      children: [
                          // Take Photo Button
                          FloatingActionButton(
                            heroTag: "snap_btn",
                            mini: true,
                            backgroundColor: Colors.white,
                            onPressed: () async {
                               try {
                                   await _streamController.takeSnapshot?.call();
                                   _showSnack("Snapshot Saved!");
                               } catch (e) {
                                   _showSnack("Error: $e");
                               }
                            },
                            child: const Icon(Icons.camera_alt, color: Colors.black),
                          ),
                          const SizedBox(height: 20),
                          
                          // Record Button
                          FloatingActionButton(
                            heroTag: "rec_btn",
                            onPressed: _streamController.isProcessing ? null : () {
                                if (_streamController.isRecording) {
                                    _streamController.stopRecording?.call().then((_) {
                                        _showSnack("Video Saved!");
                                    }).catchError((e) {
                                        _showSnack("Error: $e");
                                    });
                                } else {
                                    _streamController.startRecording?.call();
                                }
                            },
                            backgroundColor: _streamController.isRecording ? Colors.white : Colors.redAccent,
                            child: Icon(
                              _streamController.isRecording ? Icons.stop : Icons.fiber_manual_record, 
                              color: _streamController.isRecording ? Colors.red : Colors.white
                            ),
                          ),
                      ],
                  ),
                ),
              )
            ],
            
            // Blocking Processing Overlay
            if (_streamController.isProcessing)
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
