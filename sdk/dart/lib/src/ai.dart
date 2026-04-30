import 'dart:convert';
import 'package:http/http.dart' as http;

import 'client.dart';
import 'exceptions.dart';

/// AI proxy API (`/ai/*`).
///
/// datum-server forwards these requests internally to gleann (the AI
/// service); the client never talks to gleann directly.
class AiApi {
  AiApi(this._c);
  final DatumClient _c;

  /// POST /ai/ask — RAG-powered Q&A.
  Future<String> ask({
    required String question,
    String? spaceUuid,
    String? plantUuid,
  }) async {
    final res = await _c.request('POST', '/ai/ask', body: {
      'question': question,
      if (spaceUuid != null) 'space_uuid': spaceUuid,
      if (plantUuid != null) 'plant_uuid': plantUuid,
    }) as Map;
    return (res['answer'] as String?) ?? '';
  }

  /// POST /ai/ask?stream=true — server-sent events streaming answer tokens.
  Stream<String> askStream({
    required String question,
    String? spaceUuid,
    String? plantUuid,
  }) async* {
    final body = jsonEncode({
      'question': question,
      'stream': true,
      if (spaceUuid != null) 'space_uuid': spaceUuid,
      if (plantUuid != null) 'plant_uuid': plantUuid,
    });

    final req = http.Request('POST', _c.uri('/ai/ask', {'stream': 'true'}))
      ..headers.addAll(_c.headers())
      ..body = body;

    final resp = await _c.httpClient.send(req);
    if (resp.statusCode != 200) {
      final err = await resp.stream.bytesToString();
      throw DatumException(resp.statusCode, err);
    }

    await for (final chunk in resp.stream.transform(utf8.decoder)) {
      for (final line in chunk.split('\n')) {
        if (!line.startsWith('data: ')) continue;
        final payload = line.substring(6).trim();
        if (payload.isEmpty || payload == '[DONE]') return;
        try {
          final json = jsonDecode(payload) as Map<String, dynamic>;
          final token = json['token'] as String?;
          if (token != null) yield token;
        } catch (_) {/* skip malformed */}
      }
    }
  }

  /// POST /ai/analyze-photo — "What can I plant here?" analysis.
  Future<Map<String, dynamic>> analyzePhoto({
    required String photoBase64,
    double? latitude,
    double? longitude,
    double? compassHeading,
    DateTime? timestamp,
  }) async {
    final res = await _c.request('POST', '/ai/analyze-photo', body: {
      'photo_base64': photoBase64,
      if (latitude != null) 'latitude': latitude,
      if (longitude != null) 'longitude': longitude,
      if (compassHeading != null) 'compass_heading': compassHeading,
      if (timestamp != null) 'timestamp': timestamp.toIso8601String(),
    }) as Map;
    return Map<String, dynamic>.from(res);
  }

  /// POST /ai/diagnose — disease/pest diagnosis.
  Future<Map<String, dynamic>> diagnose({
    String? photoBase64,
    String? plantUuid,
    String? symptoms,
  }) async {
    final res = await _c.request('POST', '/ai/diagnose', body: {
      if (photoBase64 != null) 'photo_base64': photoBase64,
      if (plantUuid != null) 'plant_uuid': plantUuid,
      if (symptoms != null) 'symptoms': symptoms,
    }) as Map;
    return Map<String, dynamic>.from(res);
  }

  /// GET /ai/calendar — regional planting calendar.
  Future<List<Map<String, dynamic>>> calendar({
    required double latitude,
    required double longitude,
  }) async {
    final res = await _c.request('GET', '/ai/calendar', query: {
      'latitude': latitude,
      'longitude': longitude,
    }) as List;
    return res.cast<Map<String, dynamic>>();
  }
}
