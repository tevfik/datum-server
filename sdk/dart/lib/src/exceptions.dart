class DatumException implements Exception {
  DatumException(this.statusCode, this.body);

  final int statusCode;
  final String body;

  @override
  String toString() => 'DatumException($statusCode): $body';
}
