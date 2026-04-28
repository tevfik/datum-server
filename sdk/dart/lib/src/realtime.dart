import 'dart:async';
import 'dart:convert';

import 'package:http/http.dart' as http;

import 'client.dart';

/// Realtime client built on Server-Sent Events.
///
/// The ntfy-protocol-compatible broker mounted at `/notify/{topic}/sse` is
/// the canonical streaming surface for both notifications and arbitrary
/// pub/sub. Each subscription is a single long-lived HTTP request.
class RealtimeClient {
  RealtimeClient(this._c);
  final DatumClient _c;

  /// Subscribe to a topic. The returned stream emits decoded payload bodies.
  /// The HTTP request is cancelled when the stream subscription is cancelled.
  Stream<Map<String, dynamic>> subscribe(String topic) {
    final ctrl = StreamController<Map<String, dynamic>>();
    final req = http.Request('GET', _c.uri('/notify/$topic/sse'));
    req.headers.addAll(_c.headers(contentType: null));
    http.Client client = http.Client();
    () async {
      try {
        final res = await client.send(req);
        await for (final chunk in res.stream.transform(utf8.decoder).transform(const LineSplitter())) {
          if (chunk.startsWith('data:')) {
            final body = chunk.substring(5).trim();
            if (body.isEmpty) continue;
            try {
              final decoded = jsonDecode(body);
              if (decoded is Map<String, dynamic>) {
                ctrl.add(decoded);
              } else {
                ctrl.add({'data': decoded});
              }
            } catch (_) {
              ctrl.add({'data': body});
            }
          }
        }
      } catch (e, s) {
        ctrl.addError(e, s);
      } finally {
        client.close();
        await ctrl.close();
      }
    }();
    ctrl.onCancel = () => client.close();
    return ctrl.stream;
  }
}
