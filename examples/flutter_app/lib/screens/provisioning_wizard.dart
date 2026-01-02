import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../providers/device_provider.dart';

class ProvisioningWizard extends StatefulWidget {
  const ProvisioningWizard({super.key});

  @override
  @override
  State<ProvisioningWizard> createState() => _ProvisioningWizardState();
}

class _ProvisioningWizardState extends State<ProvisioningWizard> {
  final _ssidController = TextEditingController();
  final _passController = TextEditingController();
  final _uidController = TextEditingController();
  int _step = 0;
  bool _isScanning = false;

  void _scanQRCode() async {
    // Mock QR Scan
    setState(() => _isScanning = true);
    await Future.delayed(const Duration(seconds: 1));
    setState(() {
      _uidController.text = "AABBCCDDEEFF";
      _isScanning = false;
      _step = 1;
    });
  }

  void _connectAndProvision() async {
    // Mock provisioning flow
    // 1. Connect to SoftAP (handled by wifi_iot)
    // 2. Post credentials to 192.168.4.1/provision
    // 3. Register on server
    
    // Simulating server registration
    await Provider.of<DeviceProvider>(context, listen: false).createProvisioningRequest(
      _uidController.text,
      "New Device",
      _ssidController.text,
      _passController.text,
    );
    
    if (mounted) {
      Navigator.pop(context);
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Add Device')),
      body: Stepper(
        type: StepperType.vertical,
        controlsBuilder: (context, details) {
          return Padding(
            padding: const EdgeInsets.only(top: 20.0),
            child: Row(
              children: [
                ElevatedButton(
                  onPressed: details.onStepContinue,
                  style: ElevatedButton.styleFrom(backgroundColor: Colors.cyan, foregroundColor: Colors.white),
                  child: const Text('Continue'),
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
        currentStep: _step,
        onStepContinue: () {
          if (_step < 2) {
            setState(() => _step++);
          } else {
            _connectAndProvision();
          }
        },
        onStepCancel: () {
          if (_step > 0) {
            setState(() => _step--);
          }
        },
        steps: [
          Step(
            title: const Text('Scan QR Code'),
            content: Column(
              children: [
                const Text('Scan the QR code on the back of the device.'),
                ElevatedButton(
                  onPressed: _scanQRCode,
                  child: Text(_isScanning ? 'Scanning...' : 'Scan QR'),
                ),
              ],
            ),
          ),
          Step(
            title: const Text('WiFi Credentials'),
            content: Column(
              children: [
                TextField(
                  controller: _ssidController,
                  decoration: const InputDecoration(labelText: 'WiFi SSID'),
                ),
                TextField(
                  controller: _passController,
                  decoration: const InputDecoration(labelText: 'WiFi Password'),
                  obscureText: true,
                ),
              ],
            ),
          ),
          const Step(
            title: Text('Finish'),
            content: Text('Connect to device and upload credentials?'),
          ),
        ],
      ),
    );
  }
}
