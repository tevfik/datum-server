import 'dart:convert';
import 'package:http/http.dart' as http;

import 'auth.dart';
import 'buckets.dart';
import 'data.dart';
import 'devices.dart';
import 'exceptions.dart';
import 'notify.dart';
import 'realtime.dart';

/// Top-level entry point for the Datum platform.
class DatumClient {
  DatumClient({required this.baseUrl, http.Client? httpClient})
      : http = httpClient ?? http.Client() {
    auth = AuthApi(this);
    devices = DevicesApi(this);
    data = DataApi(this);
    buckets = BucketsApi(this);
    notify = NotifyApi(this);
    realtime = RealtimeClient(this);
  }

  final String baseUrl;
  final http.Client http;

  String? token;
  String? apiKey;

  late final AuthApi auth;
  late final DevicesApi devices;
  late final DataApi data;
  late final BucketsApi buckets;
  late final NotifyApi notify;
  late final RealtimeClient realtime;

  Map<String, String> headers({String? contentType = 'application/json'}) {
    final h = <String, String>{};
    if (contentType != null) h['Content-Type'] = contentType;
    if (token != null) {
      h['Authorization'] = 'Bearer $token';
    } else if (apiKey != null) {
      h['Authorization'] = 'Bearer $apiKey';
    }
    return h;
  }

  Uri uri(String path, [Map<String, dynamic>? query]) {
    final base = Uri.parse(baseUrl);
    return base.replace(
      path: '${base.path}$path',
      queryParameters: query?.map((k, v) => MapEntry(k, v.toString())),
    );
  }

  Future<dynamic> request(
    String method,
    String path, {
    Object? body,
    Map<String, dynamic>? query,
  }) async {
    final req = http.Request(method, uri(path, query));
    req.headers.addAll(headers());
    if (body != null) req.body = jsonEncode(body);
    final streamed = await http.send(req);
    final res = await http.Response.fromStream(streamed);
    if (res.statusCode >= 400) {
      throw DatumException(res.statusCode, res.body);
    }
    if (res.body.isEmpty) return null;
    return jsonDecode(res.body);
  }
}
