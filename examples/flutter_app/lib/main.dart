import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'screens/login_screen.dart';
import 'screens/home_screen.dart';
import 'providers/auth_provider.dart';
import 'providers/device_provider.dart';

void main() {
  runApp(
    MultiProvider(
      providers: [
        ChangeNotifierProvider(create: (_) => AuthProvider()),
        ChangeNotifierProvider(create: (_) => DeviceProvider()),
      ],
      child: const DatumApp(),
    ),
  );
}

class DatumApp extends StatelessWidget {
  const DatumApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Datum IoT',
      theme: ThemeData.dark().copyWith(
        primaryColor: const Color(0xFF00BCD4),
        scaffoldBackgroundColor: const Color(0xFF1B1B1B),
        colorScheme: const ColorScheme.dark(
          primary: Color(0xFF00BCD4),
          secondary: Color(0xFF00BCD4),
        ),
        cardColor: const Color(0xFF2D2D2D),
      ),
      home: Consumer<AuthProvider>(
        builder: (context, auth, _) {
          return auth.isAuthenticated ? const HomeScreen() : const LoginScreen();
        },
      ),
    );
  }
}
