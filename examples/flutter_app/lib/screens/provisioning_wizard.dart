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
  final _uidController = TextEditingController();
  int _step = 0;
  bool _isLoading = false;
  String? _statusMessage;

  // Step 1: Register on Server (Needs Internet)
  Future<void> _registerOnServer() async {
    setState(() {
      _isLoading = true;
      _statusMessage = "Registering device on cloud...";
    });

    try {
      await Provider.of<DeviceProvider>(context, listen: false).createProvisioningRequest(
        _uidController.text,
        "New Camera",
        _ssidController.text,
        "",
      );
      
      if (!mounted) return;

      setState(() {
        _isLoading = false;
        _step++;
        _statusMessage = null;
      });
      
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Device registered! Now connect to Camera WiFi.')),
      );
    } catch (e) {
      setState(() {
        _isLoading = false;
        _statusMessage = "Error: $e";
      });
    }
  }

  // Step 2: Configure Device (Needs SoftAP Connection)
  Future<void> _configureDevice() async {
    setState(() {
      _isLoading = true;
      _statusMessage = "Sending config to device...";
    });

    final dio = Dio();
    // Short timeout for local device
    dio.options.connectTimeout = const Duration(seconds: 5);
    dio.options.sendTimeout = const Duration(seconds: 5);

    try {
      // 192.168.4.1 is default ESP32 SoftAP IP
      final response = await dio.post(
        'http://192.168.4.1/provision',
        data: {
          "wifi_ssid": _ssidController.text,
          "wifi_pass": _passController.text,
          "server_url": "https://datum.bezg.in", // Hardcoded production or get from env
        },
        options: Options(contentType: Headers.jsonContentType),
      );

      if (response.statusCode == 200) {
          setState(() {
            _isLoading = false;
            _statusMessage = "Success! Device is restarting.";
          });
          
          await Future.delayed(const Duration(seconds: 2));
          if (mounted) Navigator.pop(context);
      } else {
        throw Exception("Device returned ${response.statusCode}");
      }

    } catch (e) {
       setState(() {
        _isLoading = false;
        _statusMessage = "Failed to connect to Camera. \nAre you connected to 'Datum-Cam-...' WiFi?";
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Add Device')),
      body: Stepper(
        type: StepperType.vertical,
        currentStep: _step,
        controlsBuilder: (context, details) {
           if (_isLoading) return const SizedBox.shrink(); // Hide buttons when loading

            return Padding(
            padding: const EdgeInsets.only(top: 20.0),
            child: Row(
              children: [
                ElevatedButton(
                  onPressed: details.onStepContinue,
                  style: ElevatedButton.styleFrom(backgroundColor: Colors.cyan, foregroundColor: Colors.white),
                  child: Text(_step == 2 ? 'Send Configuration' : 'Continue'),
                ),
                const SizedBox(width: 10),
                if (_step > 0)
                  TextButton(
                    onPressed: details.onStepCancel,
                    child: const Text('Back', style: TextStyle(color: Colors.white70)),
                  ),
              ],
            ),
          );
        },
        onStepContinue: () {
          if (_step == 0) {
             // Validate UID
             if (_uidController.text.length < 4) {
                ScaffoldMessenger.of(context).showSnackBar(const SnackBar(content: Text('Enter valid Device UID')));
                return;
             }
             _step++;
             setState((){});
          } else if (_step == 1) {
             // Register on server
             _registerOnServer();
          } else if (_step == 2) {
             // Configure Device
             _configureDevice();
          }
        },
        onStepCancel: () {
          if (_step > 0) setState(() => _step--);
        },
        steps: [
          // Step 0: UID
          Step(
            title: const Text('Device Info'),
            content: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                const Text('Enter the Device UID (MAC Address) found on the device label.'),
                const SizedBox(height: 10),
                TextField(
                  controller: _uidController,
                  decoration: const InputDecoration(
                    labelText: 'Device UID',
                    border: OutlineInputBorder(),
                    hintText: 'e.g. A0:B1:C2...',
                  ),
                ),
              ],
            ),
            isActive: _step >= 0,
          ),
          
          // Step 1: Credentials
          Step(
            title: const Text('WiFi Credentials'),
            content: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                const Text('Enter the WiFi credentials for the device to connect to.'),
                const SizedBox(height: 10),
                TextField(
                  controller: _ssidController,
                  decoration: const InputDecoration(labelText: 'WiFi SSID'),
                ),
                TextField(
                  controller: _passController,
                  decoration: const InputDecoration(labelText: 'WiFi Password'),
                  obscureText: true,
                ),
                 if (_isLoading) ...[
                  const SizedBox(height: 20),
                  const LinearProgressIndicator(),
                  Text(_statusMessage ?? ""),
                ]
              ],
            ),
            isActive: _step >= 1,
          ),

          // Step 2: Connect & Config
          Step(
            title: const Text('Connect & Configure'),
            content: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                const Text('1. Go to your Phone Settings -> WiFi'),
                const Text('2. Connect to network "Datum-Cam-..."'),
                const Text('3. Return here and press "Send Configuration"'),
                const SizedBox(height: 20),
                if (_statusMessage != null) 
                  Container(
                    padding: const EdgeInsets.all(8),
                    color: Colors.black12,
                    child: Text(_statusMessage!, style: const TextStyle(color: Colors.orangeAccent)),
                  ),
                  
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
