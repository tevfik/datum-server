import 'package:shared_preferences/shared_preferences.dart';

void main() {
  testWidgets('App starts at Login Screen', (WidgetTester tester) async {
    // Mock SharedPreferences
    SharedPreferences.setMockInitialValues({});

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
