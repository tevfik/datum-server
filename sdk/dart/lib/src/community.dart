import 'client.dart';

/// Community API (`/community/*`).
class CommunityApi {
  CommunityApi(this._c);
  final DatumClient _c;

  /// GET /community/feed — posts within radius of given coordinates.
  Future<List<Map<String, dynamic>>> feed({
    required double latitude,
    required double longitude,
    String radius = '50km',
  }) async {
    final res = await _c.request('GET', '/community/feed', query: {
      'latitude': latitude,
      'longitude': longitude,
      'radius': radius,
    }) as List;
    return res.cast<Map<String, dynamic>>();
  }

  /// POST /community/share — create a post, returns its id.
  Future<String> share({
    required String postType,
    required String content,
    String? photoBase64,
    String? spaceUuid,
    double? latitude,
    double? longitude,
  }) async {
    final res = await _c.request('POST', '/community/share', body: {
      'type': postType,
      'content': content,
      if (photoBase64 != null) 'photo_base64': photoBase64,
      if (spaceUuid != null) 'space_uuid': spaceUuid,
      if (latitude != null) 'latitude': latitude,
      if (longitude != null) 'longitude': longitude,
    }) as Map;
    return (res['id'] as String?) ?? '';
  }
}
