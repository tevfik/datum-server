import 'dart:math';
import 'package:flutter/material.dart';

class SparklineWidget extends StatefulWidget {
  final double? value;
  final Color color;
  final bool demoMode;

  const SparklineWidget({
    super.key,
    required this.value,
    this.color = Colors.blueAccent,
    this.demoMode = false,
  });

  @override
  State<SparklineWidget> createState() => _SparklineWidgetState();
}

class _SparklineWidgetState extends State<SparklineWidget> {
  final List<double> _dataPoints = [];
  final int _maxPoints = 50;
  final Random _rnd = Random();

  @override
  void initState() {
    super.initState();
    // Seed with some data if in demo mode
    if (widget.demoMode) {
      double val = 50;
      for (int i = 0; i < _maxPoints; i++) {
        val += _rnd.nextDouble() * 10 - 5;
        _dataPoints.add(val.clamp(0, 100));
      }
    }
  }

  @override
  void didUpdateWidget(covariant SparklineWidget oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (widget.value != null) {
      _addPoint(widget.value!);
    } else if (widget.demoMode) {
      // Auto-generate in demo mode if value is null
      _simulateNextStep();
    }
  }

  void _addPoint(double val) {
    setState(() {
      _dataPoints.add(val);
      if (_dataPoints.length > _maxPoints) {
        _dataPoints.removeAt(0);
      }
    });
  }

  void _simulateNextStep() {
    if (_dataPoints.isEmpty) _dataPoints.add(50);
    double last = _dataPoints.last;
    double next = last + (_rnd.nextDouble() * 10 - 5);
    _addPoint(next.clamp(0, 100));
  }

  @override
  Widget build(BuildContext context) {
    return CustomPaint(
      painter: _SparklinePainter(_dataPoints, widget.color),
      child: Container(),
    );
  }
}

class _SparklinePainter extends CustomPainter {
  final List<double> data;
  final Color color;

  _SparklinePainter(this.data, this.color);

  @override
  void paint(Canvas canvas, Size size) {
    if (data.length < 2) return;

    final paint = Paint()
      ..color = color
      ..strokeWidth = 2.0
      ..style = PaintingStyle.stroke
      ..strokeCap = StrokeCap.round;

    double minVal = data.reduce(min);
    double maxVal = data.reduce(max);
    if (maxVal == minVal) {
      maxVal += 1;
      minVal -= 1;
    }

    final path = Path();
    double stepX = size.width / (data.length - 1);

    for (int i = 0; i < data.length; i++) {
      double normalized = (data[i] - minVal) / (maxVal - minVal);
      double x = i * stepX;
      double y = size.height - (normalized * size.height);
      if (i == 0) {
        path.moveTo(x, y);
      } else {
        path.lineTo(x, y);
      }
    }

    canvas.drawPath(path, paint);

    // Fill gradient (optional for polish)
    final fillPath = Path.from(path)
      ..lineTo(size.width, size.height)
      ..lineTo(0, size.height)
      ..close();

    final gradient = LinearGradient(
      colors: [color.withValues(alpha: 0.3), color.withValues(alpha: 0.0)],
      begin: Alignment.topCenter,
      end: Alignment.bottomCenter,
    );

    final fillPaint = Paint()
      ..shader =
          gradient.createShader(Rect.fromLTWH(0, 0, size.width, size.height))
      ..style = PaintingStyle.fill;

    canvas.drawPath(fillPath, fillPaint);
  }

  @override
  bool shouldRepaint(covariant _SparklinePainter oldDelegate) {
    return oldDelegate.data != data;
  }
}
