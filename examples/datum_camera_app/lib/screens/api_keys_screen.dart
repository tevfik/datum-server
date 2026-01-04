import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:provider/provider.dart';
import '../providers/auth_provider.dart';
import '../api_client.dart';

class ApiKeysScreen extends StatefulWidget {
  const ApiKeysScreen({super.key});

  @override
  State<ApiKeysScreen> createState() => _ApiKeysScreenState();
}

class _ApiKeysScreenState extends State<ApiKeysScreen> {
  final ApiClient _api = ApiClient();
  List<dynamic> _keys = [];
  bool _isLoading = true;

  @override
  void initState() {
    super.initState();
    final token = Provider.of<AuthProvider>(context, listen: false).token;
    if (token != null) {
      _api.setToken(token);
    }
    _loadKeys();
  }

  Future<void> _loadKeys() async {
    try {
      final keys = await _api.getApiKeys();
      setState(() {
        _keys = keys;
        _isLoading = false;
      });
    } catch (e) {
      if (mounted) {
        setState(() => _isLoading = false);
        ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text('Error: $e')));
      }
    }
  }

  Future<void> _createKey() async {
    final nameController = TextEditingController();
    await showDialog(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('New API Key'),
        content: TextField(
          controller: nameController,
          decoration: const InputDecoration(labelText: 'Key Name (e.g. Home Assistant)'),
        ),
        actions: [
          TextButton(onPressed: () => Navigator.pop(ctx), child: const Text('Cancel')),
          ElevatedButton(
            onPressed: () async {
              Navigator.pop(ctx);
              _submitCreateKey(nameController.text);
            },
            child: const Text('Create'),
          ),
        ],
      ),
    );
  }

  Future<void> _submitCreateKey(String name) async {
    if (name.isEmpty) return;
    setState(() => _isLoading = true);
    try {
      final newKeyData = await _api.createApiKey(name);
      await _loadKeys();
      
      if (mounted) {
        // Show the raw key
        showDialog(
          context: context,
          barrierDismissible: false,
          builder: (ctx) => AlertDialog(
            title: const Text('API Key Created'),
            content: Column(
              mainAxisSize: MainAxisSize.min,
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                const Text('Copy this key now. You won\'t be able to see it again!'),
                const SizedBox(height: 10),
                Container(
                  padding: const EdgeInsets.all(10),
                  decoration: BoxDecoration(color: Colors.grey.shade200, borderRadius: BorderRadius.circular(5)),
                  child: SelectableText(newKeyData['key'] ?? 'Error', style: const TextStyle(fontFamily: 'monospace', fontWeight: FontWeight.bold)),
                ),
              ],
            ),
            actions: [
              TextButton.icon(
                icon: const Icon(Icons.copy),
                label: const Text('Copy'),
                onPressed: () {
                   Clipboard.setData(ClipboardData(text: newKeyData['key']));
                   ScaffoldMessenger.of(ctx).showSnackBar(const SnackBar(content: Text('Copied to clipboard')));
                },
              ),
              ElevatedButton(onPressed: () => Navigator.pop(ctx), child: const Text('Done')),
            ],
          ),
        );
      }
    } catch (e) {
      if (mounted) ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text('Failed: $e')));
      setState(() => _isLoading = false);
    }
  }

  Future<void> _deleteKey(String id) async {
    final confirm = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Revoke Key?'),
        content: const Text('This action cannot be undone. Any application using this key will lose access.'),
        actions: [
          TextButton(onPressed: () => Navigator.pop(ctx, false), child: const Text('Cancel')),
          ElevatedButton(
            style: ElevatedButton.styleFrom(backgroundColor: Colors.red),
            onPressed: () => Navigator.pop(ctx, true),
            child: const Text('Revoke', style: TextStyle(color: Colors.white)),
          ),
        ],
      ),
    );

    if (confirm == true) {
      setState(() => _isLoading = true);
      try {
        await _api.deleteApiKey(id);
        await _loadKeys();
      } catch (e) {
        if (mounted) {
           ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text('Error: $e')));
           setState(() => _isLoading = false);
        }
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('API Keys')),
      floatingActionButton: FloatingActionButton(
        onPressed: _createKey,
        child: const Icon(Icons.add),
      ),
      body: _isLoading 
        ? const Center(child: CircularProgressIndicator()) 
        : _keys.isEmpty 
          ? const Center(child: Text("No API Keys found.", style: TextStyle(color: Colors.grey)))
          : ListView.separated(
              itemCount: _keys.length,
              separatorBuilder: (ctx, i) => const Divider(),
              itemBuilder: (ctx, i) {
                final k = _keys[i];
                return ListTile(
                  leading: const Icon(Icons.vpn_key, color: Colors.blueGrey),
                  title: Text(k['name'] ?? 'Unnamed Key', style: const TextStyle(fontWeight: FontWeight.bold)),
                  subtitle: Text("ID: ${k['id']}\nCreated: ${k['created_at']}"), // Masked key is usually not shown in list if not needed, but API returns it masked.
                  isThreeLine: true,
                  trailing: IconButton(
                    icon: const Icon(Icons.delete_outline, color: Colors.red),
                    onPressed: () => _deleteKey(k['id']),
                  ),
                );
              },
            ),
    );
  }
}
