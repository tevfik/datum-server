import 'package:flutter/material.dart';
import 'package:fl_chart/fl_chart.dart';
import '../models/device.dart';

class DeviceDetailScreen extends StatelessWidget {
  final Device device;

  const DeviceDetailScreen({super.key, required this.device});

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: Text(device.name)),
      body: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Column(
          children: [
            Card(
              child: ListTile(
                title: const Text('Status'),
                trailing: Chip(
                  label: Text(device.status.toUpperCase()),
                  backgroundColor: device.status == 'online' ? Colors.green : Colors.grey,
                ),
              ),
            ),
            const SizedBox(height: 20),
            const Text('Live Data History', style: TextStyle(fontSize: 18)),
            const SizedBox(height: 10),
            SizedBox(
              height: 200,
              child: LineChart(
                LineChartData(
                  gridData: FlGridData(show: false),
                  titlesData: FlTitlesData(show: false),
                  borderData: FlBorderData(show: true),
                  lineBarsData: [
                    LineChartBarData(
                      spots: const [
                        FlSpot(0, 20),
                        FlSpot(1, 22),
                        FlSpot(2, 21),
                        FlSpot(3, 24),
                        FlSpot(4, 25),
                      ],
                      isCurved: true,
                      color: Colors.cyan,
                      barWidth: 3,
                    ),
                  ],
                ),
              ),
            ),
            const SizedBox(height: 20),
            Row(
              mainAxisAlignment: MainAxisAlignment.spaceEvenly,
              children: [
                ElevatedButton.icon(
                  onPressed: () {},
                  icon: const Icon(Icons.refresh),
                  label: const Text('Reboot'),
                ),
                ElevatedButton.icon(
                  onPressed: () {},
                  icon: const Icon(Icons.videocam),
                  label: const Text('Stream'),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }
}
