import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:datum_camera_app/main.dart';
import 'package:datum_camera_app/screens/login_screen.dart';
import 'package:shared_preferences/shared_preferences.dart';

void main() {
  testWidgets('App starts at Login Screen', (WidgetTester tester) async {
    // Mock SharedPreferences
    SharedPreferences.setMockInitialValues({});
    
    // Mock FlutterSecureStorage
    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
        .setMockMethodCallHandler(
            const MethodChannel('plugins.it_nomads.com/flutter_secure_storage'),
            (MethodCall methodCall) async {
      return null;
    });

    // Build our app and trigger a frame.
    await tester.pumpWidget(
      const ProviderScope(
        child: DatumApp(),
      ),
    );

    // Initial frame should show Splash Screen (CircularProgressIndicator)
    expect(find.byType(CircularProgressIndicator), findsOneWidget);

    // Wait for async auth load to complete
    await tester.pumpAndSettle();

    // Verify that we are now on the Login Screen
    expect(find.byType(LoginScreen), findsOneWidget);
    expect(find.text('Datum IoT Login'), findsOneWidget);

    // Verify input fields exist
    expect(find.widgetWithText(TextField, 'Email'), findsOneWidget);
    expect(find.widgetWithText(TextField, 'Password'), findsOneWidget);

    // Verify login button exists
    expect(find.widgetWithText(ElevatedButton, 'Login'), findsOneWidget);
  });
}
