class ThingDescription {
  final String title;
  final String deviceType;
  final Map<String, ThingProperty> properties;
  final Map<String, ThingAction> actions;

  ThingDescription({
    required this.title,
    required this.deviceType,
    this.properties = const {},
    this.actions = const {},
  });

  factory ThingDescription.fromJson(Map<String, dynamic> json) {
    var props = <String, ThingProperty>{};
    if (json['properties'] != null) {
      json['properties'].forEach((key, val) {
        props[key] = ThingProperty.fromJson(val);
      });
    }

    var acts = <String, ThingAction>{};
    if (json['actions'] != null) {
      json['actions'].forEach((key, val) {
        acts[key] = ThingAction.fromJson(val);
      });
    }

    return ThingDescription(
      title: json['title'] ?? 'Unknown',
      deviceType: json['device_type'] ?? 'generic',
      properties: props,
      actions: acts,
    );
  }
}

class ThingProperty {
  final String type;
  final String description;
  final String? unit;
  final bool readOnly;

  ThingProperty({
    required this.type,
    this.description = '',
    this.unit,
    this.readOnly = false,
  });

  factory ThingProperty.fromJson(Map<String, dynamic> json) {
    return ThingProperty(
      type: json['type'] ?? 'string',
      description: json['description'] ?? '',
      unit: json['unit'],
      readOnly: json['readOnly'] ?? false,
    );
  }
}

class ThingAction {
  final String description;

  ThingAction({this.description = ''});

  factory ThingAction.fromJson(Map<String, dynamic> json) {
    return ThingAction(
      description: json['description'] ?? '',
    );
  }
}
