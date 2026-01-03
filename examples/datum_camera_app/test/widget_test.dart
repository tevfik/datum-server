import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:provider/provider.dart';
import 'package:datum_camera_app/main.dart';
import 'package:datum_camera_app/providers/auth_provider.dart';
import 'package:datum_camera_app/providers/device_provider.dart';
import 'package:datum_camera_app/screens/login_screen.dart';

void main() {
  testWidgets('App starts at Login Screen', (WidgetTester tester) async {
    // Build our app and trigger a frame.
    await tester.pumpWidget(
      MultiProvider(
        providers: [
          ChangeNotifierProvider(create: (_) => AuthProvider()),
          ChangeNotifierProvider(create: (_) => DeviceProvider()),
        ],
        child: const DatumApp(),
      ),
    );

    // Verify that we start on the Login Screen
    expect(find.byType(LoginScreen), findsOneWidget);
    expect(find.text('Datum IoT Login'), findsOneWidget);
    
    // Verify input fields exist
    expect(find.widgetWithText(TextField, 'Email'), findsOneWidget);
    expect(find.widgetWithText(TextField, 'Password'), findsOneWidget);
    
    // Verify login button exists
    expect(find.widgetWithText(ElevatedButton, 'Login'), findsOneWidget);
  });
}
