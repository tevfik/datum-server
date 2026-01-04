import 'package:flutter/material.dart';
import 'package:package_info_plus/package_info_plus.dart';
import 'package:flutter_oss_licenses/flutter_oss_licenses.dart';

class AboutScreen extends StatefulWidget {
  const AboutScreen({super.key});

  @override
  State<AboutScreen> createState() => _AboutScreenState();
}

class _AboutScreenState extends State<AboutScreen> {
  PackageInfo _packageInfo = PackageInfo(
    appName: 'Unknown',
    packageName: 'Unknown',
    version: 'Unknown',
    buildNumber: 'Unknown',
  );

  @override
  void initState() {
    super.initState();
    _initPackageInfo();
  }

  Future<void> _initPackageInfo() async {
    final info = await PackageInfo.fromPlatform();
    setState(() {
      _packageInfo = info;
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('About')),
      body: ListView(
        children: [
          const SizedBox(height: 20),
          const Icon(Icons.info_outline, size: 80, color: Colors.blue),
          const SizedBox(height: 20),
          Center(
            child: Text(
              _packageInfo.appName,
              style: Theme.of(context).textTheme.headlineMedium,
            ),
          ),
          const SizedBox(height: 10),
          Center(
            child: Text(
              'Version: ${_packageInfo.version} (Build ${_packageInfo.buildNumber})',
              style: Theme.of(context).textTheme.titleMedium,
            ),
          ),
          const Divider(height: 40),
          ListTile(
            leading: const Icon(Icons.policy),
            title: const Text('Open Source Licenses'),
            subtitle: const Text('View licenses of third-party libraries'),
            trailing: const Icon(Icons.arrow_forward_ios),
            onTap: () {
              Navigator.of(context).push(MaterialPageRoute(
                builder: (_) => OssLicensesMenu(
                  appIcon:  const Icon(Icons.info_outline, size: 48, color: Colors.blue),
                ),
              ));
            },
          ),
          ListTile(
            leading: const Icon(Icons.privacy_tip),
            title: const Text('Privacy Policy'),
            subtitle: const Text('https://your-privacy-policy-url.com'),
            onTap: () {
              // TODO: Open URL
            },
          ),
          const Padding(
            padding: EdgeInsets.all(16.0),
            child: Text(
              '© 2024 Datum IoT Platform. All rights reserved.',
              textAlign: TextAlign.center,
              style: TextStyle(color: Colors.grey),
            ),
          ),
        ],
      ),
    );
  }
}
