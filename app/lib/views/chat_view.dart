import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart'; // Import Bloc
import 'package:qabot/bloc/chat_bloc.dart'; // Import ChatBloc, State, Event
import 'package:shared_preferences/shared_preferences.dart';
import 'package:flutter_secure_storage/flutter_secure_storage.dart'; // Import flutter_secure_storage
import 'package:flutter_markdown/flutter_markdown.dart'; // Import flutter_markdown

class ChatView extends StatefulWidget {
  const ChatView({super.key});

  @override
  State<ChatView> createState() => _ChatViewState();
}

class _ChatViewState extends State<ChatView> {
  final TextEditingController _messageController = TextEditingController();
  final TextEditingController _apiKeyController = TextEditingController();
  final TextEditingController _serverIpController = TextEditingController();
  final TextEditingController _serverPortController = TextEditingController();

  final _storage = const FlutterSecureStorage(); // Initialize FlutterSecureStorage

  String _apiKey = '';
  String _serverIp = '';
  int _serverPort = 0;

  final ScrollController _scrollController =
      ScrollController(); // For auto-scrolling

  @override
  void initState() {
    super.initState();
    _loadSettings();
  }

  Future<void> _loadSettings() async {
    // Load settings from secure storage
    _apiKey = await _storage.read(key: 'api_key') ?? '';
    _serverIp = await _storage.read(key: 'server_ip') ?? '';
    final serverPortString = await _storage.read(key: 'server_port');
    _serverPort = int.tryParse(serverPortString ?? '') ?? 0;

    setState(() {
      _apiKeyController.text = _apiKey;
      _serverIpController.text = _serverIp;
      _serverPortController.text = _serverPort == 0 ? '' : _serverPort.toString();
    });

    // Dispatch SettingsLoadedEvent after loading settings
    context.read<ChatBloc>().add(SettingsLoadedEvent(
      apiKey: _apiKey,
      serverIp: _serverIp,
      serverPort: _serverPort,
    ));

    // The connection logic is now handled within the ChatBloc's SettingsLoadedEvent handler
  }

  Future<void> _saveSettings() async {
    final newApiKey = _apiKeyController.text;
    final newServerIp = _serverIpController.text;
    final newServerPort = int.tryParse(_serverPortController.text) ?? 0;

    // Save settings to secure storage
    await _storage.write(key: 'api_key', value: newApiKey);
    await _storage.write(key: 'server_ip', value: newServerIp);
    await _storage.write(key: 'server_port', value: newServerPort.toString()); // Secure storage stores strings

    setState(() {
      _apiKey = newApiKey;
      _serverIp = newServerIp;
      _serverPort = newServerPort;
    });

    ScaffoldMessenger.of(context).showSnackBar(
      const SnackBar(content: Text('Settings Saved!')),
    );

    // Dispatch SettingsLoadedEvent after saving settings
    context.read<ChatBloc>().add(SettingsLoadedEvent(
      apiKey: newApiKey,
      serverIp: newServerIp,
      serverPort: newServerPort,
    ));

    // The connection logic is now handled within the ChatBloc's SettingsLoadedEvent handler
  }

  void _connect() {
     // Connection logic is now handled by the Bloc when settings are loaded/saved
     // We can still keep this function if needed for a dedicated connect button,
     // but it should dispatch a ConnectWebSocketEvent with the current settings from the state.
     // For now, we'll rely on the SettingsLoadedEvent to trigger connection.
     // Connection logic is now handled by the Bloc when settings are loaded/saved
     // We can still keep this function if needed for a dedicated connect button,
     // but it should dispatch a ConnectWebSocketEvent with the current settings from the state.
     // For now, we'll rely on the SettingsLoadedEvent to trigger connection.
     // Use the local state variables for IP and Port
     if (_serverIp.isNotEmpty && _serverPort != 0 && _apiKey.isNotEmpty) {
        context.read<ChatBloc>().add(ConnectWebSocketEvent(host: _serverIp, port: _serverPort));
     } else {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('Please enter and save Server IP, Port, and API Key.')),
        );
     }
  }

  void _sendMessage() {
    // Get API key from the Bloc state before sending
    final currentApiKey = context.read<ChatBloc>().state.apiKey;

    if (_messageController.text.isNotEmpty && currentApiKey.isNotEmpty) {
      // Send message via Bloc, passing the API key from the state
      context.read<ChatBloc>().add(
            SendMessageEvent(_messageController.text, currentApiKey), // Pass API key from state
          );
      _messageController.clear();
      _scrollToBottom(); // Scroll after sending
    } else if (currentApiKey.isEmpty) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(
            content: Text('Please enter and save your API Key first!')),
      );
    } else if (_messageController.text.isEmpty) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Please enter a message.')),
      );
    }
  }

  // Helper method to scroll to the bottom of the list
  void _scrollToBottom() {
    // Use addPostFrameCallback to scroll after the frame is built
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (_scrollController.hasClients) {
        _scrollController.animateTo(
          _scrollController.position.maxScrollExtent,
          duration: const Duration(milliseconds: 300),
          curve: Curves.easeOut,
        );
      }
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        // Display connection status in AppBar
        title: BlocBuilder<ChatBloc, ChatState>(
          builder: (context, state) {
            String status = 'AI Chat';
            if (state is ChatLoading) status = state.statusMessage;
            if (state is ChatConnected) status = 'AI Chat (Connected)';
            if (state is ChatError) status = 'AI Chat (Error)';
            if (state is ChatInitial && state.statusMessage.isNotEmpty)
              status = 'AI Chat (${state.statusMessage})';
            if (state is ChatInitial && _apiKey.isEmpty)
              status = 'AI Chat (Enter API Key)';
            return Text(status);
          },
        ),
        actions: [
          // Add a settings button
          IconButton(
            icon: const Icon(Icons.settings),
            tooltip: 'Settings',
            onPressed: _showSettingsDialog,
          ),
          // Add a reconnect button
          BlocBuilder<ChatBloc, ChatState>(
            builder: (context, state) {
              // Show reconnect button if not connected or loading
              if (state is! ChatConnected && state is! ChatLoading) {
                return IconButton(
                  icon: const Icon(Icons.refresh),
                  tooltip: 'Reconnect',
                  onPressed: (_apiKey.isNotEmpty && _serverIp.isNotEmpty && _serverPort != 0)
                      ? _connect
                      : null, // Disable if no API key, IP, or Port
                );
              }
              return const SizedBox.shrink(); // Hide if connected/loading
            },
          ),
        ],
      ),
      body: Column(
        children: [
          // Use BlocBuilder to display messages from the state
          Expanded(
            child: BlocConsumer<ChatBloc, ChatState>(
              // Use BlocConsumer to listen for state changes for scrolling
              listener: (context, state) {
                // Scroll to bottom whenever messages change (in ChatConnected or ChatError state)
                if (state is ChatConnected || state is ChatError) {
                  _scrollToBottom();
                }
                // Show error messages from the state
                if (state is ChatError) {
                  // Avoid showing snackbar for connection errors if already shown in AppBar
                  if (!state.error.contains("Connection Failed") &&
                      !state.error.contains("Not connected")) {
                    ScaffoldMessenger.of(context).showSnackBar(
                      SnackBar(
                          content: Text(state.error),
                          backgroundColor: Colors.red),
                    );
                  }
                }
              },
              builder: (context, state) {
                // Handle initial state explicitly if needed
                if (state is ChatInitial &&
                    state.messages.isEmpty &&
                    (_apiKey.isEmpty || _serverIp.isEmpty || _serverPort == 0)) {
                  return const Center(
                      child: Text(
                          'Please enter and save Server IP, Port, and API Key to connect.'));
                }
                if (state is ChatInitial &&
                    state.messages.isEmpty &&
                    _apiKey.isNotEmpty && _serverIp.isNotEmpty && _serverPort != 0) {
                  return Center(
                      child: Text(state.statusMessage.isNotEmpty
                          ? state.statusMessage
                          : 'Press reconnect button.'));
                }
                if (state is ChatLoading && state.messages.isEmpty) {
                  return Center(
                      child: Column(
                    mainAxisAlignment: MainAxisAlignment.center,
                    children: [
                      CircularProgressIndicator(),
                      SizedBox(height: 10),
                      Text(state.statusMessage),
                    ],
                  ));
                }

                // Display messages
                return ListView.builder(
                  controller: _scrollController, // Attach scroll controller
                  itemCount: state.messages.length +
                      (state is ChatLoading &&
                              state.statusMessage == 'AI is thinking...'
                          ? 1
                          : 0), // Adjust count for thinking indicator
                  itemBuilder: (context, index) {
                    // Show thinking indicator at the end if ChatLoading and thinking
                    if (state is ChatLoading &&
                        state.statusMessage == 'AI is thinking...' &&
                        index == state.messages.length) {
                      return const Padding(
                        padding: EdgeInsets.symmetric(vertical: 8.0),
                        child: Center(child: Text("AI is thinking...")),
                      );
                    }
                    // Ensure index is within bounds for messages
                    if (index < state.messages.length) {
                      final message = state.messages[index];
                      final isUserMessage = message.startsWith('You: ');
                      final displayedMessage = isUserMessage ? message.substring(5) : message.substring(4); // Remove "You: " or "AI: " prefix

                      return Align(
                        alignment: isUserMessage ? Alignment.centerRight : Alignment.centerLeft,
                        child: Container(
                          constraints: BoxConstraints(
                              maxWidth: MediaQuery.of(context).size.width * 0.7), // Limit bubble width
                          padding: const EdgeInsets.symmetric(
                              vertical: 10.0, horizontal: 15.0),
                          margin: const EdgeInsets.symmetric(
                              vertical: 4.0, horizontal: 8.0),
                          decoration: BoxDecoration(
                            color: isUserMessage ? Colors.blueAccent : Colors.grey[300],
                            borderRadius: BorderRadius.only(
                              topLeft: const Radius.circular(15.0),
                              topRight: const Radius.circular(15.0),
                              bottomLeft: isUserMessage
                                  ? const Radius.circular(15.0)
                                  : const Radius.circular(0.0),
                              bottomRight: isUserMessage
                                  ? const Radius.circular(0.0)
                                  : const Radius.circular(15.0),
                            ),
                          ),
                          child: isUserMessage
                              ? Text( // User messages remain as Text
                                  displayedMessage,
                                  style: TextStyle(
                                      color: isUserMessage ? Colors.white : Colors.black87),
                                )
                              : MarkdownBody( // AI messages use MarkdownBody
                                  data: displayedMessage,
                                  styleSheet: MarkdownStyleSheet.fromTheme(Theme.of(context)).copyWith(
                                    p: Theme.of(context).textTheme.bodyMedium?.copyWith(color: Colors.black87),
                                  ),
                                ),
                        ),
                      );
                    }
                    return const SizedBox
                        .shrink(); // Should not happen if logic is correct
                  },
                );
              },
            ),
          ),
          const Divider(height: 1.0), // Add a visual separator
          Padding(
            padding: const EdgeInsets.all(8.0),
            child: Row(
              children: [
                Expanded(
                  child: TextField(
                    controller: _messageController,
                    decoration: InputDecoration(
                      hintText: 'Enter your message...',
                      border: OutlineInputBorder(
                        borderRadius: BorderRadius.circular(25.0),
                        borderSide: BorderSide.none,
                      ),
                      filled: true,
                      fillColor: Colors.grey[200],
                      contentPadding: const EdgeInsets.symmetric(horizontal: 20.0, vertical: 10.0),
                    ),
                    // Disable input if not connected
                    enabled: context.watch<ChatBloc>().state is ChatConnected,
                    onSubmitted: (_) => _sendMessage(),
                  ),
                ),
                const SizedBox(width: 8.0), // Add spacing
                // Disable send button if not connected
                BlocBuilder<ChatBloc, ChatState>(builder: (context, state) {
                  return FloatingActionButton( // Use FloatingActionButton for send button
                    onPressed: state is ChatConnected
                        ? _sendMessage
                        : null, // Enable only when connected
                    child: const Icon(Icons.send),
                    mini: true, // Make it smaller
                  );
                }),
              ],
            ),
          ),
        ],
      ),
    );
  }

  void _showSettingsDialog() {
    showDialog(
      context: context,
      builder: (BuildContext context) {
        return AlertDialog(
          title: const Text('Settings'),
          content: SingleChildScrollView(
            child: ListBody(
              children: <Widget>[
                TextField(
                  controller: _serverIpController,
                  decoration: const InputDecoration(
                    hintText: 'Enter Server IP...',
                    labelText: 'Server IP',
                  ),
                  keyboardType: TextInputType.numberWithOptions(decimal: true),
                ),
                const SizedBox(height: 10),
                TextField(
                  controller: _serverPortController,
                  decoration: const InputDecoration(
                    hintText: 'Enter Server Port...',
                    labelText: 'Server Port',
                  ),
                  keyboardType: TextInputType.number,
                ),
                const SizedBox(height: 10),
                TextField(
                  controller: _apiKeyController,
                  decoration: const InputDecoration(
                    hintText: 'Enter your API Key...',
                    labelText: 'API Key',
                  ),
                  obscureText: true,
                ),
              ],
            ),
          ),
          actions: <Widget>[
            TextButton(
              child: const Text('Cancel'),
              onPressed: () {
                // Reset controllers if cancelled
                _apiKeyController.text = _apiKey;
                _serverIpController.text = _serverIp;
                _serverPortController.text = _serverPort == 0 ? '' : _serverPort.toString();
                Navigator.of(context).pop();
              },
            ),
            TextButton(
              child: const Text('Save'),
              onPressed: () {
                _saveSettings();
                Navigator.of(context).pop();
              },
            ),
          ],
        );
      },
    );
  }

  @override
  void dispose() {
    _messageController.dispose();
    _apiKeyController.dispose();
    _serverIpController.dispose();
    _serverPortController.dispose();
    _scrollController.dispose();
    super.dispose();
  }
}
