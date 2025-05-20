import 'package:flutter/material.dart';
import 'package:flutter_secure_storage/flutter_secure_storage.dart';

class SettingsView extends StatefulWidget {
  final String initialServerIp;
  final int initialServerPort;
  final int initialContextLimit;

  const SettingsView({
    super.key,
    required this.initialServerIp,
    required this.initialServerPort,
    required this.initialContextLimit,
  });

  @override
  State<SettingsView> createState() => _SettingsViewState();
}

class _SettingsViewState extends State<SettingsView> {
  late TextEditingController _serverIpController;
  late TextEditingController _serverPortController;
  late int _contextLimit;

  final _storage = const FlutterSecureStorage();
  final _formKey = GlobalKey<FormState>();

  @override
  void initState() {
    super.initState();
    _serverIpController = TextEditingController(text: widget.initialServerIp);
    _serverPortController = TextEditingController(
        text: widget.initialServerPort == 0
            ? ''
            : widget.initialServerPort.toString());
    _contextLimit = widget.initialContextLimit;
  }

  Future<void> _saveSettings() async {
    if (_formKey.currentState!.validate()) {
      _formKey.currentState!.save(); // Important to call save on Form

      final newServerIp = _serverIpController.text;
      final newServerPort = int.tryParse(_serverPortController.text) ?? 0;

      await _storage.write(key: 'server_ip', value: newServerIp);
      await _storage.write(key: 'server_port', value: newServerPort.toString());
      await _storage.write(
          key: 'context_limit', value: _contextLimit.toString());

      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('Settings Saved!')),
        );
        Navigator.pop(
            context, true); // Return true to indicate settings were saved
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Settings'),
      ),
      body: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Form(
          key: _formKey,
          child: ListView(
            children: <Widget>[
              TextFormField(
                controller: _serverIpController,
                decoration: const InputDecoration(
                  hintText: 'Enter Server IP',
                  labelText: 'Server IP',
                  border: OutlineInputBorder(),
                ),
                keyboardType:
                    const TextInputType.numberWithOptions(decimal: true),
              ),
              const SizedBox(height: 20),
              TextFormField(
                controller: _serverPortController,
                decoration: const InputDecoration(
                  hintText: 'Enter Server Port',
                  labelText: 'Server Port',
                  border: OutlineInputBorder(),
                ),
                keyboardType: TextInputType.number,
                validator: (value) {
                  if (value == null || value.isEmpty) {
                    return 'Server Port cannot be empty';
                  }
                  final port = int.tryParse(value);
                  if (port == null || port <= 0 || port > 65535) {
                    return 'Enter a valid port number (1-65535)';
                  }
                  return null;
                },
              ),
              const SizedBox(height: 20),
              DropdownButtonFormField<int>(
                decoration: const InputDecoration(
                  labelText: 'Context Message Limit',
                  border: OutlineInputBorder(),
                ),
                value: _contextLimit,
                items: const [
                  DropdownMenuItem(value: 5, child: Text('Recent 5')),
                  DropdownMenuItem(
                      value: 10, child: Text('Recent 10 (Default)')),
                  DropdownMenuItem(value: 20, child: Text('Recent 20')),
                  DropdownMenuItem(
                      value: 0, child: Text('All Messages (No Limit)')),
                ],
                onChanged: (value) {
                  if (value != null) {
                    setState(() {
                      _contextLimit = value;
                    });
                  }
                },
                validator: (value) {
                  if (value == null) {
                    return 'Please select a context limit';
                  }
                  return null;
                },
              ),
              const SizedBox(height: 30),
              ElevatedButton(
                onPressed: _saveSettings,
                style: ElevatedButton.styleFrom(
                  padding: const EdgeInsets.symmetric(vertical: 16.0),
                ),
                child: const Text('Save Settings'),
              ),
            ],
          ),
        ),
      ),
    );
  }

  @override
  void dispose() {
    _serverIpController.dispose();
    _serverPortController.dispose();
    super.dispose();
  }
}
