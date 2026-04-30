import 'dart:convert';
import 'package:http/http.dart' as http;

import 'ai.dart';
import 'admin.dart';
import 'auth.dart';
import 'buckets.dart';
import 'community.dart';
import 'data.dart';
import 'db.dart';
import 'devices.dart';
import 'exceptions.dart';
import 'notify.dart';
import 'realtime.dart';
import 'rules.dart';
import 'sys.dart';

/// Top-level entry point for the Datum platform.
class DatumClient {
  DatumClient({required this.baseUrl, http.Client? httpClient})
      : httpClient = httpClient ?? http.Client() {
    auth = AuthApi(this);
    devices = DevicesApi(this);
    data = DataApi(this);
    buckets = BucketsApi(this);
    notify = NotifyApi(this);
    realtime = RealtimeClient(this);
    db = DbApi(this);
    ai = AiApi(this);
    community = CommunityApi(this);
    sys = SysApi(this);
    admin = AdminApi(this);
    rules = RulesApi(this);
  }

  final String baseUrl;
  final http.Client httpClient;

  String? token;
  String? userId;
  String? apiKey;

  bool get isAuthenticated => token != null;

  late final AuthApi auth;
  late final DevicesApi devices;
  late final DataApi data;
  late final BucketsApi buckets;
  late final NotifyApi notify;
  late final RealtimeClient realtime;
  late final DbApi db;
  late final AiApi ai;
  late final CommunityApi community;
  late final SysApi sys;
  late final AdminApi admin;
  late final RulesApi rules;

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
    final streamed = await httpClient.send(req);
    final res = await http.Response.fromStream(streamed);
    if (res.statusCode >= 400) {
      throw DatumException(res.statusCode, res.body);
    }
    if (res.body.isEmpty) return null;
    return jsonDecode(res.body);
  }
}
