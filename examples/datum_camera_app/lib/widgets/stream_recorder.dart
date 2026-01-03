import 'package:flutter/material.dart';
import 'dart:async';
import 'dart:io';
import 'dart:ui' as ui;
import 'package:flutter/foundation.dart';
import 'package:flutter_quick_video_encoder/flutter_quick_video_encoder.dart';
import 'package:gal/gal.dart';
import 'package:path_provider/path_provider.dart';

  bool _isRecording = false;
  bool _isProcessing = false;
  String _durationString = "00:00";
  int _fps = 0;

  bool get isRecording => _isRecording;
  bool get isProcessing => _isProcessing;
  String get durationString => _durationString;
  int get fps => _fps;

  set isRecording(bool value) {
    _isRecording = value;
    notifyListeners();
  }

  set isProcessing(bool value) {
    _isProcessing = value;
    notifyListeners();
  }

  set durationString(String value) {
    _durationString = value;
    notifyListeners();
  }

  set fps(int value) {
    _fps = value;
    notifyListeners();
  }

  VoidCallback? startRecording;
  Future<void> Function()? stopRecording;
  Future<void> Function()? takeSnapshot;
  
  void notify() => notifyListeners();

class StreamRecorder extends StatefulWidget {
  final String streamUrl;
  final StreamRecorderController? controller;
  final Function(String)? onError;
  final Widget? placeholder;
  final BoxFit fit;

  const StreamRecorder({
    super.key,
    required this.streamUrl,
    this.controller,
    this.onError,
    this.placeholder,
    this.fit = BoxFit.contain,
  });

  @override
  State<StreamRecorder> createState() => _StreamRecorderState();
}

class _StreamRecorderState extends State<StreamRecorder> {
  HttpClient? _httpClient;
  StreamSubscription<List<int>>? _streamSubscription;
  Uint8List? _imageBytes;
  
  // FPS Logic
  int _frameCount = 0;
  Timer? _fpsTimer;

  // Recording Logic
  DateTime? _recordingStart;
  String? _tempFramesPath;
  int _recFrameCount = 0;

  @override
  void initState() {
    super.initState();
    _connectController();
    _startStream();
    _fpsTimer = Timer.periodic(const Duration(seconds: 1), (_) {
       if (widget.controller != null) {
          widget.controller!.fps = _frameCount;
          if (widget.controller!.isRecording && _recordingStart != null) {
              final duration = DateTime.now().difference(_recordingStart!);
              final m = duration.inMinutes.toString().padLeft(2, '0');
              final s = (duration.inSeconds % 60).toString().padLeft(2, '0');
              widget.controller!.durationString = "$m:$s";
              // widget.controller!.notifyListeners(); // Setters now handle notification
          }
       }
       _frameCount = 0;
    });
  }

  void _connectController() {
    if (widget.controller != null) {
      widget.controller!.startRecording = _startRecording;
      widget.controller!.stopRecording = _stopRecording;
      widget.controller!.takeSnapshot = _takeSnapshot;
    }
  }

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
          
          // Find Start (FF D8)
          for (int i = 0; i < buffer.length - 1; i++) {
            if (buffer[i] == 0xFF && buffer[i+1] == 0xD8) {
              start = i;
              break;
            }
          }
          
          if (start == -1) {
             if (buffer.length > 1000000) buffer.clear(); 
             break;
          }
          
          // Find End (FF D9)
          for (int i = start; i < buffer.length - 1; i++) {
             if (buffer[i] == 0xFF && buffer[i+1] == 0xD9) {
               end = i + 2;
               break;
             }
          }
          
          if (end != -1) {
            final frameBytes = Uint8List.fromList(buffer.sublist(start, end));
            buffer.removeRange(0, end);
            
            if (mounted) {
              setState(() {
                _imageBytes = frameBytes;
                _frameCount++;
              });
            }
            
            if (widget.controller?.isRecording == true && _tempFramesPath != null) {
               final fileName = "frame_${_recFrameCount.toString().padLeft(4, '0')}.jpg";
               File("$_tempFramesPath/$fileName").writeAsBytes(frameBytes); 
               _recFrameCount++;
            }
            
          } else {
            if (start > 0) buffer.removeRange(0, start);
            break;
          }
        }
      }, onError: (e) {
         if (widget.onError != null) widget.onError!(e.toString());
      });
      
    } catch (e) {
      if (widget.onError != null) widget.onError!(e.toString());
    }
  }

  void _startRecording() async {
    try {
      final docDir = await getApplicationDocumentsDirectory();
      final tempDir = Directory("${docDir.path}/temp_rec_to_video");
      if (await tempDir.exists()) await tempDir.delete(recursive: true);
      await tempDir.create();

      if (mounted) {
        setState(() {
          _tempFramesPath = tempDir.path;
          _recFrameCount = 0;
          _recordingStart = DateTime.now();
        });
        widget.controller!.isRecording = true;
        // widget.controller!.notifyListeners();
      }
    } catch (e) {
      debugPrint("Init Rec Error: $e");
    }
  }

  Future<void> _stopRecording() async {
    if (!widget.controller!.isRecording) return;
    
    // Stop recording state first
    widget.controller!.isRecording = false;
    widget.controller!.isProcessing = true;
    // widget.controller!.notifyListeners();

    // Calculate details
    final endTime = DateTime.now();
    final durationVal = endTime.difference(_recordingStart!).inMilliseconds / 1000.0;
    
    // FPS Calc
    int targetFps = 10;
    if (durationVal > 0 && _recFrameCount > 0) {
      targetFps = (_recFrameCount / durationVal).round();
    }
    if (targetFps < 1) targetFps = 1;

    try {
       // Only process if we have frames
       if (_recFrameCount < 5) {
          throw Exception("Video too short (< 5 frames)");
       }

       final docDir = await getApplicationDocumentsDirectory();
       final outputPath = "${docDir.path}/rec_${DateTime.now().millisecondsSinceEpoch}.mp4";

       // 1. Setup
       final firstFrameFile = File("$_tempFramesPath/frame_0000.jpg");
       if (!await firstFrameFile.exists()) throw Exception("No frames captured");
       
       final firstImage = await decodeImageFromList(await firstFrameFile.readAsBytes());
       
       await FlutterQuickVideoEncoder.setup(
          width: firstImage.width,
          height: firstImage.height,
          fps: targetFps,
          videoBitrate: 2000000,
          profileLevel: ProfileLevel.any,
          audioBitrate: 0,
          audioChannels: 0,
          sampleRate: 44100,
          filepath: outputPath,
       );

       // 2. Encode
       for (int i = 0; i < _recFrameCount; i++) {
           final f = File("$_tempFramesPath/frame_${i.toString().padLeft(4, '0')}.jpg");
           if (await f.exists()) {
             final bytes = await f.readAsBytes();
             final codec = await ui.instantiateImageCodec(bytes);
             final frameInfo = await codec.getNextFrame();
             final rawBytes = await frameInfo.image.toByteData(format: ui.ImageByteFormat.rawRgba);
             
             if (rawBytes != null) {
                await FlutterQuickVideoEncoder.appendVideoFrame(rawBytes.buffer.asUint8List());
             }
             frameInfo.image.dispose();
           }
       }

       // 3. Finish
       await FlutterQuickVideoEncoder.finish();
       
       // 4. Thumbnail
       final thumbPath = outputPath.replaceAll(".mp4", ".jpg");
       await firstFrameFile.copy(thumbPath);

       // 5. Gallery
       await Gal.putVideo(outputPath);
       
       // Cleanup output
       if (await File(outputPath).exists()) await File(outputPath).delete();

    } catch (e) {
      debugPrint("Encoding Error: $e");
      rethrow;
    } finally {
       // Cleanup Temp
       if (_tempFramesPath != null) {
          final d = Directory(_tempFramesPath!);
          if (await d.exists()) await d.delete(recursive: true);
       }
       
       widget.controller!.isProcessing = false;
       widget.controller!.notify();
    }
  }

  Future<void> _takeSnapshot() async {
    if (_imageBytes == null) return;
    try {
      final docDir = await getApplicationDocumentsDirectory();
      final path = "${docDir.path}/snap_${DateTime.now().millisecondsSinceEpoch}.jpg";
      await File(path).writeAsBytes(_imageBytes!);
      await Gal.putImage(path);
      // Cleanup
      if (await File(path).exists()) await File(path).delete();
    } catch (e) {
      rethrow;
    }
  }

  @override
  void dispose() {
    _fpsTimer?.cancel();
    _streamSubscription?.cancel();
    _httpClient?.close();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    if (_imageBytes == null) {
      return widget.placeholder ?? const Center(child: CircularProgressIndicator());
    }
    return Image.memory(
      _imageBytes!,
      gaplessPlayback: true,
      fit: widget.fit,
    );
  }
}
