import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:dio/dio.dart';
import '../providers/device_provider.dart';

class ProvisioningWizard extends StatefulWidget {
  const ProvisioningWizard({super.key});

  @override
  State<ProvisioningWizard> createState() => _ProvisioningWizardState();
}

class _ProvisioningWizardState extends State<ProvisioningWizard> {
  final _ssidController = TextEditingController();
  final _passController = TextEditingController();
  final _userPassController = TextEditingController(); // Confirm user password
  String? _selectedSSID;
  List<dynamic> _networks = [];
  bool _scanning = false;
  
  // Device Info
  String? _deviceUID;
  String? _firmwareVersion;

  int _step = 0;
  bool _isLoading = false;
  String? _statusMessage;
  
  // Helper to connect to SoftAP endpoints with short timeout
  Dio _getDeviceDio() {
    final dio = Dio();
    dio.options.baseUrl = 'http://192.168.4.1';
    dio.options.connectTimeout = const Duration(seconds: 3);
    dio.options.sendTimeout = const Duration(seconds: 3);
    return dio; 
  }

  // Step 1: Discover Device
  Future<void> _discoverDevice() async {
    setState(() {
      _isLoading = true;
      _statusMessage = "Connecting to device...";
    });

    try {
      final response = await _getDeviceDio().get('/info');
      if (response.statusCode == 200) {
        final data = response.data;
        setState(() {
          _deviceUID = data['device_uid'];
          _firmwareVersion = data['firmware_version'];
          _isLoading = false;
           _statusMessage = null;
          _step++; // Move to next step
        });
        
        // Auto-scan for networks
        _scanNetworks();
      }
    } catch (e) {
      setState(() {
        _isLoading = false;
        _statusMessage = "Could not find device. Ensure you are connected to 'Datum-Camera-...' WiFi.";
      });
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

  // Step 3: Send Configuration (WiFi + User Creds)
  Future<void> _provisionDevice() async {
    setState(() {
      _isLoading = true;
      _statusMessage = "Sending configuration...";
    });
    
    // Get current user email from AuthProvider (assuming available via context or passed in)
    // For now we'll ask user to confirm password, and assume email is known? 
    // Ideally AuthProvider should expose currentUser.email
    // Let's assume user knows their email or we get it from local storage/auth state
    // For safety, let's inject Auth Service or similar. 
    // Simplification: We assume the user is logged in. Use a placeholder or fetch from storage if possible.
    // NOTE: In a real app, use: Provider.of<AuthProvider>(context, listen: false).user?.email
    // I will add an email field just to be safe if auth provider isn't handy here.
    
    // Actually, let's verify if we can get email.
    // Using hardcoded email from prompt isn't good. 
    // Let's add email field for re-verification security.
  }
  
  // Refined Step 3 Implementation
  Future<void> _completeProvisioning(String email) async {
     setState(() {
      _isLoading = true;
      _statusMessage = "Configuring device...";
    });

    try {
      final response = await _getDeviceDio().post(
        '/configure',
        data: FormData.fromMap({
          "wifi_ssid": _ssidController.text,
          "wifi_pass": _passController.text,
          "server_url": "https://datum.bezg.in",
          "user_email": email,
          "user_password": _userPassController.text,
        }),
      );

      if (response.statusCode == 200) {
          setState(() {
            _isLoading = false;
            _statusMessage = "Success! Device is restarting and will register itself.";
          });
          
          await Future.delayed(const Duration(seconds: 3));
          if (mounted) Navigator.pop(context);
      }
    } catch (e) {
       setState(() {
        _isLoading = false;
        _statusMessage = "Failed to configure device: $e";
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    // Attempt to get user email from provider if possible, else empty
    // final userEmail = Provider.of<AuthProvider>(context).user?.email ?? "";
    // Because I don't see AuthProvider file content, I'll add an Email field securely.

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
                  ElevatedButton(
                    onPressed: _discoverDevice,
                    child: const Text('I am Connected'),
                  ),
                  
                if (_step == 1)
                  ElevatedButton(
                     onPressed: () {
                       if (_ssidController.text.isEmpty) {
                         ScaffoldMessenger.of(context).showSnackBar(const SnackBar(content: Text("Select or enter WiFi SSID")));
                         return;
                       }
                       setState(() => _step++);
                     },
                     child: const Text('Next'),
                  ),
                  
                if (_step == 2) ...[
                   ElevatedButton(
                     onPressed: () {
                        if (_userPassController.text.isEmpty) {
                           ScaffoldMessenger.of(context).showSnackBar(const SnackBar(content: Text("Enter password to authorize device")));
                           return;
                        }
                        // Use email from controller if we added it, else use a default or ask
                        // Ideally we should have the email. I'll Add an email controller.
                        _completeProvisioning(_emailController.text);
                     },
                     style: ElevatedButton.styleFrom(backgroundColor: Colors.cyan, foregroundColor: Colors.white),
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
                   Text(_statusMessage!, style: const TextStyle(color: Colors.red)),
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
                 if (_scanning) const LinearProgressIndicator()
                 else if (_networks.isEmpty) 
                    TextButton.icon(icon: const Icon(Icons.refresh), label: const Text("Rescan"), onPressed: _scanNetworks),
                    
                 if (_networks.isNotEmpty)
                   Container(
                     height: 150,
                     decoration: BoxDecoration(border: Border.all(color: Colors.grey)),
                     child: ListView.builder(
                       itemCount: _networks.length,
                       itemBuilder: (ctx, i) {
                         final net = _networks[i];
                         return ListTile(
                           title: Text(net['ssid']),
                           trailing: Icon(Icons.wifi, size: 20),
                           dense: true,
                           onTap: () {
                              setState(() {
                                _ssidController.text = net['ssid'];
                                _selectedSSID = net['ssid'];
                              });
                           },
                           selected: _selectedSSID == net['ssid'],
                         );
                       }
                     ),
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
                   Text("Device ID: $_deviceUID", style: const TextStyle(fontWeight: FontWeight.bold)),
                   const SizedBox(height: 10),
                ],
                const Text('Enter your account credentials to register this device.'),
                 TextField(
                   controller: _emailController,
                   decoration: const InputDecoration(labelText: 'Your Email'),
                   keyboardType: TextInputType.emailAddress,
                 ),
                TextField(
                   controller: _userPassController,
                   decoration: const InputDecoration(labelText: 'Your Password'),
                   obscureText: true,
                 ),
                 
                 if (_statusMessage != null) ...[
                    const SizedBox(height: 10),
                    Text(_statusMessage!, style: TextStyle(color: _isLoading ? Colors.blue : Colors.green)),
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
  
  final _emailController = TextEditingController();
}
