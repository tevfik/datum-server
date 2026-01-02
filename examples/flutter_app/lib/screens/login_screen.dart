import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../providers/auth_provider.dart';
import 'register_screen.dart';
import 'debug_screen.dart';

import 'package:shared_preferences/shared_preferences.dart';

class LoginScreen extends StatefulWidget {
  const LoginScreen({super.key});

  @override
  State<LoginScreen> createState() => _LoginScreenState();
}


class _LoginScreenState extends State<LoginScreen> {
  final _emailController = TextEditingController();
  final _passwordController = TextEditingController();
  bool _isLoading = false;
  bool _rememberMe = false;

  @override
  void initState() {
    super.initState();
    _loadCredentials();
  }

  Future<void> _loadCredentials() async {
    final prefs = await SharedPreferences.getInstance();
    final remember = prefs.getBool('remember_me') ?? false;
    if (remember) {
      if (mounted) {
        setState(() {
          _rememberMe = true;
          _emailController.text = prefs.getString('saved_email') ?? '';
          _passwordController.text = prefs.getString('saved_pass') ?? '';
        });
      }
    }
  }

  Future<void> _login() async {
    setState(() => _isLoading = true);
    final success = await Provider.of<AuthProvider>(context, listen: false)
        .login(_emailController.text, _passwordController.text);
    setState(() => _isLoading = false);

    if (success) {
      // Handle Remember Me
      final prefs = await SharedPreferences.getInstance();
      if (_rememberMe) {
         await prefs.setBool('remember_me', true);
         await prefs.setString('saved_email', _emailController.text);
         await prefs.setString('saved_pass', _passwordController.text);
      } else {
         await prefs.remove('remember_me');
         await prefs.remove('saved_email');
         await prefs.remove('saved_pass');
      }
    } else if (mounted) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Login failed. Check credentials.')),
      );
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Datum IoT Login'),
        actions: [
          IconButton(
            icon: const Icon(Icons.bug_report),
            onPressed: () {
              Navigator.push(
                context,
                MaterialPageRoute(builder: (_) => const DebugScreen()),
              );
            },
          ),
        ],
      ),
      body: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Column(
          children: [
            const Icon(Icons.cloud_circle, size: 80, color: Colors.cyan),
            const SizedBox(height: 20),
            TextField(
              controller: _emailController,
              decoration: const InputDecoration(labelText: 'Email', border: OutlineInputBorder()),
            ),
            const SizedBox(height: 10),
            TextField(
              controller: _passwordController,
              decoration: const InputDecoration(labelText: 'Password', border: OutlineInputBorder()),
              obscureText: true,
            ),
            Row(
               children: [
                 Checkbox(
                   value: _rememberMe,
                   onChanged: (v) => setState(() => _rememberMe = v ?? false),
                 ),
                 const Text("Remember Me"),
               ],
            ),
            const SizedBox(height: 10),
            _isLoading
                ? const CircularProgressIndicator()
                : Column(
                    children: [
                      ElevatedButton(
                        style: ElevatedButton.styleFrom(
                          minimumSize: const Size.fromHeight(50),
                          backgroundColor: Colors.cyan,
                          foregroundColor: Colors.white,
                        ),
                        onPressed: _login,
                        child: const Text('Login'),
                      ),
                      const SizedBox(height: 10),
                      TextButton(
                        onPressed: () {
                          Navigator.push(
                            context,
                            MaterialPageRoute(builder: (_) => const RegisterScreen()),
                          );
                        },
                        child: const Text('Create Account', style: TextStyle(color: Colors.cyan)),
                      ),
                    ],
                  ),
          ],
        ),
      ),
    );
  }
}
