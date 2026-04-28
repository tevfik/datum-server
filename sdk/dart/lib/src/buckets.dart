import 'dart:typed_data';

import 'client.dart';

class BucketsApi {
  BucketsApi(this._c);
  final DatumClient _c;

  Future<List<String>> list() async {
    final res = await _c.request('GET', '/storage') as Map;
    return List<String>.from(res['buckets'] as List? ?? []);
  }

  Future<void> create(String bucket) async {
    await _c.request('POST', '/storage/$bucket');
  }

  Future<List<dynamic>> objects(String bucket, {String? prefix, int? limit}) async {
    final res = await _c.request('GET', '/storage/$bucket', query: {
      if (prefix != null) 'prefix': prefix,
      if (limit != null) 'limit': limit,
    }) as Map;
    return List<dynamic>.from(res['objects'] as List? ?? []);
  }

  Future<Map<String, dynamic>> put(
      String bucket, String path, Uint8List body, {String? contentType}) async {
    final url = _c.uri('/storage/$bucket/$path');
    final headers = _c.headers(contentType: contentType ?? 'application/octet-stream');
    final res = await _c.http.put(url, headers: headers, body: body);
    if (res.statusCode >= 400) {
      throw Exception('PUT failed (${res.statusCode}): ${res.body}');
    }
    return Map<String, dynamic>.from(
        (res.body.isEmpty ? {} : await _c.request('HEAD', '/storage/$bucket/$path')) as Map? ?? {});
  }

  Future<Map<String, dynamic>> presign(String bucket, String path,
      {String method = 'GET', int expiresSecs = 900}) async {
    return Map<String, dynamic>.from(await _c.request(
      'POST',
      '/storage/$bucket/presign',
      body: {'path': path, 'method': method, 'expires_secs': expiresSecs},
    ) as Map);
  }

  Future<void> delete(String bucket, String path) async {
    await _c.request('DELETE', '/storage/$bucket/$path');
  }
}
