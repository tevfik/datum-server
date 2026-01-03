import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_mjpeg/flutter_mjpeg.dart';
import 'dart:async';

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

  @override
  void initState() {
    super.initState();
    // Hide UI initially
    SystemChrome.setEnabledSystemUIMode(SystemUiMode.immersiveSticky);
    SystemChrome.setPreferredOrientations([
      DeviceOrientation.landscapeLeft,
      DeviceOrientation.landscapeRight,
    ]);

    _updateTime();
    _timer = Timer.periodic(const Duration(seconds: 1), (Timer t) => _updateTime());
  }

  void _updateTime() {
    final now = DateTime.now();
    setState(() {
      _timeString = "${now.hour.toString().padLeft(2, '0')}:${now.minute.toString().padLeft(2, '0')}:${now.second.toString().padLeft(2, '0')}";
    });
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

  void _toggleOverlays() {
    setState(() {
      _showOverlays = !_showOverlays;
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: Colors.black,
      body: GestureDetector(
        onTap: _toggleOverlays,
        child: Stack(
          children: [
            // Center Stream
            Center(
              child: Mjpeg(
                isLive: true,
                stream: widget.streamUrl,
                error: (context, error, stack) => Center(
                   child: Text("Stream Error: $error", style: const TextStyle(color: Colors.red)),
                ),
              ),
            ),
            
            // Overlays
            if (_showOverlays) ...[
              // Top Bar Background Gradient
              Positioned(
                top: 0, left: 0, right: 0,
                child: Container(
                  height: 80,
                  decoration: const BoxDecoration(
                    gradient: LinearGradient(
                      begin: Alignment.topCenter,
                      end: Alignment.bottomCenter,
                      colors: [Colors.black54, Colors.transparent],
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
              
              // Clock (Top-Left, next to back)
              Positioned(
                top: 32, left: 70,
                child: SafeArea(
                  child: Row(
                    children: [
                      Text(
                        _timeString, 
                        style: const TextStyle(
                          color: Colors.white, 
                          fontSize: 18, 
                          fontWeight: FontWeight.bold,
                          fontFamily: "monospace",
                          shadows: [Shadow(blurRadius: 2, color: Colors.black)]
                        )
                      ),
                      const SizedBox(width: 10),
                      Text(
                        widget.deviceName,
                        style: const TextStyle(color: Colors.white70, fontSize: 14),
                      )
                    ],
                  ),
                ),
              ),
              
              // Status Indicator (Top-Right)
              Positioned(
                top: 32, right: 30,
                child: SafeArea(
                  child: Container(
                    padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 4),
                    decoration: BoxDecoration(
                      color: Colors.redAccent.withOpacity(0.8),
                      borderRadius: BorderRadius.circular(4),
                    ),
                    child: const Row(
                      mainAxisSize: MainAxisSize.min,
                      children: [
                        Icon(Icons.circle, size: 10, color: Colors.white),
                        SizedBox(width: 5),
                        Text(
                          "LIVE", 
                          style: TextStyle(
                            color: Colors.white, 
                            fontWeight: FontWeight.bold, 
                            fontSize: 12
                          )
                        ),
                      ],
                    ),
                  ),
                ),
              ),
            ],
          ],
        ),
      ),
    );
  }
}
