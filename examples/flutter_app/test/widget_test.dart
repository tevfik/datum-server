import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:provider/provider.dart';
import 'package:datum_app/main.dart';
import 'package:datum_app/providers/auth_provider.dart';
import 'package:datum_app/providers/device_provider.dart';

void main() {
  testWidgets('Datum App loads login screen initially', (WidgetTester tester) async {
    await tester.pumpWidget(
      MultiProvider(
        providers: [
          ChangeNotifierProvider(create: (_) => AuthProvider()),
          ChangeNotifierProvider(create: (_) => DeviceProvider()),
        ],
        child: const DatumApp(),
      ),
    );

    expect(find.text('Datum IoT Login'), findsOneWidget);
    expect(find.text('Email'), findsOneWidget);
    expect(find.text('Password'), findsOneWidget);
  });
}
