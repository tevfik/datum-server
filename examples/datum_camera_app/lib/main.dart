import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'screens/login_screen.dart';
import 'screens/home_screen.dart';
import 'providers/auth_provider.dart';
import 'package:app_links/app_links.dart';
import 'screens/reset_password_screen.dart';

void main() {
  runApp(
    const ProviderScope(
      child: DatumApp(),
    ),
  );
}

class DatumApp extends StatefulWidget {
  const DatumApp({super.key});

  @override
  State<DatumApp> createState() => _DatumAppState();
}

class _DatumAppState extends State<DatumApp> {
  final _navigatorKey = GlobalKey<NavigatorState>();
  late AppLinks _appLinks;

  @override
  void initState() {
    super.initState();
    _initDeepLinks();
  }

  Future<void> _initDeepLinks() async {
    _appLinks = AppLinks();

    // Check initial link
    final uri = await _appLinks
        .getInitialLink(); // In v6+, this returns Uri? (or String? based on confusing docs, but we test Uri)
    if (uri != null) {
      _handleDeepLink(uri);
    }

    // Listen for future links
    _appLinks.uriLinkStream.listen((uri) {
      _handleDeepLink(uri);
    });
  }

  void _handleDeepLink(Uri uri) {
    // Expected link: https://datum.localhost/reset-password?token=...
    if (uri.path.contains('reset-password')) {
      final token = uri.queryParameters['token'];
      if (token != null) {
        _navigatorKey.currentState?.push(
          MaterialPageRoute(
            builder: (_) => ResetPasswordScreen(initialToken: token),
          ),
        );
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      navigatorKey: _navigatorKey,
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
      home: Consumer(
        builder: (context, ref, _) {
          final authState = ref.watch(authProvider);

          // Listen for auth changes to reset navigation stack on logout
          ref.listen<AsyncValue<String?>>(authProvider, (previous, next) {
            next.whenData((token) {
              if (token == null) {
                // User logged out - clear entire stack and go to login
                _navigatorKey.currentState?.popUntil((route) => route.isFirst);
                // Ensure we are at root which will be LoginScreen due to home param
              }
            });
          });

          return authState.when(
            data: (token) =>
                token != null ? const HomeScreen() : const LoginScreen(),
            loading: () => const Scaffold(
              backgroundColor: Color(0xFF1B1B1B),
              body:
                  Center(child: CircularProgressIndicator(color: Colors.cyan)),
            ),
            error: (err, stack) => const LoginScreen(),
          );
        },
      ),
    );
  }
}
