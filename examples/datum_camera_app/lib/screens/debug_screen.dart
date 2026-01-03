import 'package:flutter/material.dart';
import '../api_client.dart';

class DebugScreen extends StatelessWidget {
  const DebugScreen({super.key});

  @override
  Widget build(BuildContext context) {
    final logger = DebugLogger();
    
    return Scaffold(
      appBar: AppBar(
        title: const Text('Debug Console'),
        actions: [
          IconButton(
            icon: const Icon(Icons.delete),
            onPressed: () {
              logger.clear();
            },
          ),
        ],
      ),
      body: ValueListenableBuilder<int>(
        valueListenable: logger.logCount,
        builder: (context, count, _) {
          return ListView.builder(
            itemCount: logger.logs.length,
            reverse: true, // Show newest at bottom (or top if we reverse logic)
            // Let's emulate a console: Newest at bottom usually, but listview reverse puts index 0 at bottom
            // We just want to see the list. Standard list is fine.
            // Let's actually show newest at TOP for easier mobile reading.
            itemBuilder: (context, index) {
              // Show in reverse order (newest first)
              final log = logger.logs[logger.logs.length - 1 - index];
              final isError = log.contains('ERR:');
              final isReq = log.contains('REQ:');
              
              Color color = Colors.white;
              if (isError) {
                color = Colors.redAccent;
              } else if (isReq) {
                color = Colors.cyanAccent;
              }

              return Container(
                padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 4),
                decoration: const BoxDecoration(
                  border: Border(bottom: BorderSide(color: Colors.white10)),
                ),
                child: SelectableText(
                  log,
                  style: TextStyle(fontFamily: 'monospace', fontSize: 12, color: color),
                ),
              );
            },
          );
        },
      ),
    );
  }
}
