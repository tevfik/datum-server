class Device {
  final String id;
  final String uid;
  final String name;
  final String type;
  final String status;
  final String? publicIP;
  final DateTime? lastSeen;

  Device({
    required this.id,
    required this.uid,
    required this.name,
    required this.type,
    required this.status,
    this.publicIP,
    this.lastSeen,
  });

  factory Device.fromJson(Map<String, dynamic> json) {
    return Device(
      id: json['id']?.toString() ?? '',
      uid: json['device_uid'] ?? '',
      name: json['name'] ?? 'Unknown',
      type: json['type'] ?? 'unknown',
      status: json['status'] ?? 'offline',
      publicIP: json['public_ip'],
      lastSeen:
          json['last_seen'] != null ? DateTime.parse(json['last_seen']) : null,
    );
  }
}
