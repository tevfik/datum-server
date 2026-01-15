import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:dio/dio.dart';
import 'package:wifi_iot/wifi_iot.dart';
import 'package:permission_handler/permission_handler.dart';
import '../providers/auth_provider.dart';

import '../utils/thing_description_registry.dart'; // Import Registry

class ProvisioningWizard extends ConsumerStatefulWidget {
  const ProvisioningWizard({super.key});

  @override
  ConsumerState<ProvisioningWizard> createState() => _ProvisioningWizardState();
}

class _ProvisioningWizardState extends ConsumerState<ProvisioningWizard> {
  final _ssidController = TextEditingController();
  final _passController = TextEditingController();
  final _nameController = TextEditingController();

  String? _selectedSSID;
  List<dynamic> _networks = [];
  bool _scanning = false;

  // Device Info
  String? _deviceUID;
  String? _firmwareVersion;
  String? _deviceType;

  int _step = 0;
  bool _isLoading = false;
  String? _statusMessage;

  @override
  void dispose() {
    _ssidController.dispose();
    _passController.dispose();
    _nameController.dispose();
    super.dispose();
  }

  // Helper to connect to SoftAP endpoints with short timeout
  Dio _getDeviceDio() {
    final dio = Dio();
    dio.options.baseUrl = 'http://192.168.4.1';
    dio.options.connectTimeout = const Duration(seconds: 3);
    dio.options.sendTimeout = const Duration(seconds: 3);
    return dio;
  }

  // Step 1: Discover Device (Auto-Connect)
  Future<void> _autoConnectAndDiscover() async {
    setState(() {
      _isLoading = true;
      _statusMessage = "Requesting permissions...";
    });

    // 1. Request Permissions
    Map<Permission, PermissionStatus> statuses = await [
      Permission.location,
      Permission.nearbyWifiDevices, // Android 13+
    ].request();

    if (statuses[Permission.location]!.isDenied ||
        (statuses[Permission.nearbyWifiDevices] != null &&
            statuses[Permission.nearbyWifiDevices]!.isDenied)) {
      setState(() {
        _isLoading = false;
        _statusMessage = "Permissions denied. Cannot scan for devices.";
      });
      return;
    }

    // 2. Scan for Datum Networks
    setState(() => _statusMessage = "Scanning for 'Datum-' devices...");

    try {
      // Load list
      // Note: On iOS this might be empty, so we might need a specific prefix connection if we knew it.
      // But for generic "Datum-XXXX", we scan.
      // ignore: deprecated_member_use
      List<WifiNetwork> networks = await WiFiForIoTPlugin.loadWifiList();

      var deviceNetworks = networks
          .where((n) => n.ssid != null && n.ssid!.startsWith("Datum-"))
          .toList();

      // Sort by signal strength (descending)
      deviceNetworks
          .sort((a, b) => (b.level ?? -100).compareTo(a.level ?? -100));

      if (deviceNetworks.isEmpty) {
        setState(() {
          _isLoading = false;
          _statusMessage =
              "No 'Datum-' devices found. Make sure device is in setup mode.";
        });
        return;
      }

      // 3. Connect to the strongest one
      var target = deviceNetworks.first;
      setState(() => _statusMessage = "Connecting to ${target.ssid}...");

      bool connected = await WiFiForIoTPlugin.connect(
        target.ssid!,
        password: null, // Open network as per firmware
        security: NetworkSecurity.NONE,
        joinOnce: true, // Important for iOS
        withInternet: false, // Don't expect internet
      );

      if (!connected) {
        setState(() {
          _isLoading = false;
          _statusMessage = "Failed to connect to ${target.ssid}.";
        });
        return;
      }

      // Force WiFi usage on Android (since no internet)
      await WiFiForIoTPlugin.forceWifiUsage(true);

      // 4. Verify Connection & Get Info
      setState(() => _statusMessage = "Connected! Reading device info...");
      // Small delay to let network stack settle
      await Future.delayed(const Duration(seconds: 2));

      await _discoverDevice(); // Call original discovery logic
    } catch (e) {
      if (mounted) {
        setState(() {
          _isLoading = false;
          _statusMessage = "Auto-connect error: $e. Try manual connection.";
        });
      }
    }
  }

  // Original Discovery Logic (now internal or manual fallback)
  Future<void> _discoverDevice() async {
    if (!mounted) return;
    setState(() {
      _isLoading = true;
      _statusMessage = "Reading device info from 192.168.4.1...";
    });

    try {
      final response = await _getDeviceDio().get('/info');
      if (response.statusCode == 200) {
        final data = response.data;
        setState(() {
          _deviceUID = data['device_uid'];
          _firmwareVersion = data['firmware_version'];
          _deviceType = data['device_type'];
          _isLoading = false;
          _statusMessage = null;
          _step++;
        });

        _scanNetworks();
      }
    } catch (e) {
      if (mounted) {
        setState(() {
          _isLoading = false;
          _statusMessage =
              "Could not reach device. Ensure you are connected to 'Datum-...' WiFi.";
        });
      }
    }
  }

  // Step 2: Scan Networks
  Future<void> _scanNetworks() async {
    setState(() {
      _scanning = true;
      _networks = [];
    });

    try {
      final response = await _getDeviceDio().get('/scan');
      if (response.statusCode == 200 && mounted) {
        setState(() {
          _networks = response.data;
          _scanning = false;
        });
      }
    } catch (e) {
      if (mounted) {
        setState(() {
          _scanning = false;
          // If scan fails, allow manual entry
          _networks = [];
        });
      }
    }
  }

  // Step 3 Implementation
  Future<void> _completeProvisioning() async {
    setState(() {
      _isLoading = true;
      _statusMessage = "Configuring device...";
    });

    // Get token from AuthProvider
    // Note: In real app, make sure getter exists or use public field if comfortable.
    // Based on previous file read, _token is private but there is `get isAuthenticated`.
    // I need to update AuthProvider to expose token or hack it via reflection?
    // Wait, the file read of `auth_provider.dart` showed `String? _token;` and NO public getter for token string, only `isAuthenticated`.
    // I MUST ADD A GETTER TO AuthProvider FIRST.
    // Assuming I will do that in next step or assuming I can modify it now.
    // I will use `Provider.of<AuthProvider>(context, listen: false).token` and fix AuthProvider immediately after this tool call.

    final token = ref.read(authProvider).value;

    if (token == null) {
      setState(() {
        _isLoading = false;
        _statusMessage = "Error: User not authenticated.";
      });
      return;
    }

    try {
      final defaultName = ThingDescriptionRegistry.get(_deviceType ?? '').title;
      final response = await _getDeviceDio().post(
        '/configure',
        data: FormData.fromMap({
          "wifi_ssid": _ssidController.text,
          "wifi_pass": _passController.text,
          "device_name": _nameController.text.isNotEmpty
              ? _nameController.text
              : defaultName,
          "server_url": "http://datum.bezg.in:8000",
          "user_token": token,
        }),
      );

      if (response.statusCode == 200) {
        if (mounted) {
          setState(() {
            _isLoading = false;
            _statusMessage =
                "Success! Device is restarting and will register itself.";
          });
        }

        await Future.delayed(const Duration(seconds: 3));
        if (mounted) Navigator.pop(context, true);
      }
    } catch (e) {
      if (mounted) {
        setState(() {
          _isLoading = false;
          _statusMessage = "Failed to configure device: $e";
        });
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Add New Device')),
      body: Stepper(
        type: StepperType.vertical,
        currentStep: _step,
        controlsBuilder: (context, details) {
          if (_isLoading) return const SizedBox.shrink();

          return Padding(
            padding: const EdgeInsets.only(top: 20.0),
            child: Row(
              children: [
                if (_step == 0)
                  Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      ElevatedButton.icon(
                        icon: const Icon(Icons.wifi_find),
                        label: const Text('Auto Connect & Setup'),
                        onPressed: _autoConnectAndDiscover,
                        style: ElevatedButton.styleFrom(
                            backgroundColor: Colors.blueAccent,
                            foregroundColor: Colors.white),
                      ),
                      const SizedBox(height: 8),
                      TextButton(
                        onPressed: _discoverDevice,
                        child: const Text('I connected manually',
                            style: TextStyle(color: Colors.grey)),
                      ),
                    ],
                  ),
                if (_step == 1)
                  ElevatedButton(
                    onPressed: () {
                      if (_ssidController.text.isEmpty) {
                        ScaffoldMessenger.of(context).showSnackBar(
                            const SnackBar(
                                content: Text("Select or enter WiFi SSID")));
                        return;
                      }
                      setState(() => _step++);
                    },
                    child: const Text('Next'),
                  ),
                if (_step == 2) ...[
                  ElevatedButton(
                    onPressed: () {
                      _completeProvisioning();
                    },
                    style: ElevatedButton.styleFrom(
                        backgroundColor: Colors.cyan,
                        foregroundColor: Colors.white),
                    child: const Text('Provision Device'),
                  ),
                ],
                const SizedBox(width: 10),
                if (_step > 0)
                  TextButton(
                    onPressed: () => setState(() => _step--),
                    child: const Text('Back'),
                  ),
              ],
            ),
          );
        },
        steps: [
          // Step 0: Connect
          Step(
            title: const Text('Connect to Device'),
            content: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                const Text('1. Open WiFi Settings'),
                const Text('2. Connect to "Datum-Camera-XXXX"'),
                const Text('3. Return here and press "I am Connected"'),
                const SizedBox(height: 10),
                if (_statusMessage != null)
                  Text(_statusMessage!,
                      style: const TextStyle(color: Colors.red)),
              ],
            ),
            isActive: _step >= 0,
            state: _step > 0 ? StepState.complete : StepState.indexed,
          ),

          // Step 1: Network Selection
          Step(
            title: const Text('Select WiFi Network'),
            content: Column(
              children: [
                if (_scanning)
                  const LinearProgressIndicator()
                else if (_networks.isEmpty)
                  TextButton.icon(
                      icon: const Icon(Icons.refresh),
                      label: const Text("Rescan"),
                      onPressed: _scanNetworks),
                if (_networks.isNotEmpty)
                  Container(
                    height: 150,
                    decoration:
                        BoxDecoration(border: Border.all(color: Colors.grey)),
                    child: ListView.builder(
                        itemCount: _networks.length,
                        itemBuilder: (ctx, i) {
                          final net = _networks[i];
                          return ListTile(
                            title: Text(net['ssid']),
                            trailing: const Icon(Icons.wifi, size: 20),
                            dense: true,
                            onTap: () {
                              setState(() {
                                _ssidController.text = net['ssid'];
                                _selectedSSID = net['ssid'];
                              });
                            },
                            selected: _selectedSSID == net['ssid'],
                          );
                        }),
                  ),
                const SizedBox(height: 10),
                TextField(
                  controller: _ssidController,
                  decoration: const InputDecoration(labelText: 'SSID'),
                ),
                TextField(
                  controller: _passController,
                  decoration: const InputDecoration(labelText: 'WiFi Password'),
                  obscureText: true,
                ),
              ],
            ),
            isActive: _step >= 1,
            state: _step > 1 ? StepState.complete : StepState.indexed,
          ),

          // Step 2: Confirmation
          Step(
            title: const Text('Authorize Device'),
            content: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                if (_deviceUID != null) ...[
                  Text(
                      "Found: ${ThingDescriptionRegistry.get(_deviceType ?? '').title}",
                      style: const TextStyle(
                          fontWeight: FontWeight.bold, fontSize: 16)),
                  Text("ID: $_deviceUID\nFW: ${_firmwareVersion ?? 'Unknown'}",
                      style: const TextStyle(color: Colors.grey)),
                  const SizedBox(height: 10),
                ],
                TextField(
                  controller: _nameController,
                  decoration: InputDecoration(
                      labelText: 'Device Name',
                      hintText: ThingDescriptionRegistry.get(_deviceType ?? '')
                          .title, // Dynamic Hint
                      border: const OutlineInputBorder()),
                ),
                const SizedBox(height: 10),
                const Text(
                    'Device will be linked to your account automatically.'),
                if (_statusMessage != null) ...[
                  const SizedBox(height: 10),
                  Text(_statusMessage!,
                      style: TextStyle(
                          color: _isLoading ? Colors.blue : Colors.green)),
                ],
                if (_isLoading) const LinearProgressIndicator(),
              ],
            ),
            isActive: _step >= 2,
          ),
        ],
      ),
    );
  }
}
