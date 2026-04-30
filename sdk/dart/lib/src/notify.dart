import 'client.dart';

/// Notifications API — wraps the multi-channel dispatcher and the embedded
/// ntfy-protocol broker.
class NotifyApi {
  NotifyApi(this._c);
  final DatumClient _c;

  Future<void> publish(String topic,
      {String? title, required String body, int? priority}) async {
    await _c.request('POST', '/notify/$topic', body: {
      if (title != null) 'title': title,
      'body': body,
      if (priority != null) 'priority': priority,
    });
  }
}
