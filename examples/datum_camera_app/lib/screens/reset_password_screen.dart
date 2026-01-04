import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../providers/auth_provider.dart';

class ResetPasswordScreen extends StatefulWidget {
  final String? initialToken;

  const ResetPasswordScreen({super.key, this.initialToken});

  @override
  State<ResetPasswordScreen> createState() => _ResetPasswordScreenState();
}

class _ResetPasswordScreenState extends State<ResetPasswordScreen> {
  final _tokenController = TextEditingController();
  final _passwordController = TextEditingController();
  final _confirmPasswordController = TextEditingController();
  bool _isLoading = false;

  @override
  void initState() {
    super.initState();
    if (widget.initialToken != null) {
      _tokenController.text = widget.initialToken!;
    }
  }

  Future<void> _submit() async {
    if (_passwordController.text != _confirmPasswordController.text) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Passwords do not match')),
      );
      return;
    }

    if (_tokenController.text.isEmpty) {
        ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Token is missing')),
      );
      return;
    }

    setState(() => _isLoading = true);
    try {
      await Provider.of<AuthProvider>(context, listen: false)
          .resetPassword(_tokenController.text, _passwordController.text);
      
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('Password reset successfully. Please login.')),
        );
        Navigator.popUntil(context, (route) => route.isFirst); // Go to login
      }
    } catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('Error: ${e.toString()}')),
        );
      }
    } finally {
      if (mounted) setState(() => _isLoading = false);
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Reset Password')),
      body: SingleChildScrollView(
        padding: const EdgeInsets.all(16.0),
        child: Column(
          children: [
             // Usually token is hidden if passed via deep link, but visible for manual entry
             TextField(
              controller: _tokenController,
              decoration: const InputDecoration(
                labelText: 'Reset Token',
                border: OutlineInputBorder(),
                helperText: 'Usually filled automatically from email link',
              ),
            ),
            const SizedBox(height: 20),
            TextField(
              controller: _passwordController,
              decoration: const InputDecoration(
                labelText: 'New Password',
                border: OutlineInputBorder(),
              ),
              obscureText: true,
            ),
            const SizedBox(height: 20),
            TextField(
              controller: _confirmPasswordController,
              decoration: const InputDecoration(
                labelText: 'Confirm Password',
                border: OutlineInputBorder(),
              ),
              obscureText: true,
            ),
            const SizedBox(height: 20),
            _isLoading
                ? const CircularProgressIndicator()
                : ElevatedButton(
                    onPressed: _submit,
                    style: ElevatedButton.styleFrom(
                      minimumSize: const Size.fromHeight(50),
                      backgroundColor: Colors.cyan,
                      foregroundColor: Colors.white,
                    ),
                    child: const Text('Reset Password'),
                  ),
          ],
        ),
      ),
    );
  }
}
