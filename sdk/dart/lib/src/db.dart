/// Generic Document Store API (`/db/:collection/:id`).
///
/// Maps to datum-server's `db_handlers.go` (RegisterDBRoutes). Useful for
/// app-specific structured data — gardens, plants, journal entries, etc.
import 'client.dart';

class DbApi {
  DbApi(this._c);
  final DatumClient _c;

  /// POST /db/:collection — create a document, returns its id.
  Future<String> create(String collection, Map<String, dynamic> doc) async {
    final res = await _c.request('POST', '/db/$collection', body: doc) as Map;
    return res['id'] as String;
  }

  /// GET /db/:collection — list all documents.
  Future<List<Map<String, dynamic>>> list(String collection) async {
    final res = await _c.request('GET', '/db/$collection') as List;
    return res.cast<Map<String, dynamic>>();
  }

  /// GET /db/:collection/:id — fetch a document.
  Future<Map<String, dynamic>> get(String collection, String id) async =>
      Map<String, dynamic>.from(
          await _c.request('GET', '/db/$collection/$id') as Map);

  /// PUT /db/:collection/:id — replace a document.
  Future<void> update(
      String collection, String id, Map<String, dynamic> doc) async {
    await _c.request('PUT', '/db/$collection/$id', body: doc);
  }

  /// DELETE /db/:collection/:id
  Future<void> delete(String collection, String id) async {
    await _c.request('DELETE', '/db/$collection/$id');
  }
}
