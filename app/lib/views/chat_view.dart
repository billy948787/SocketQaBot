import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart'; // Import Bloc
import 'package:qabot/bloc/chat_bloc.dart'; // Import ChatBloc, State, Event
import 'package:shared_preferences/shared_preferences.dart';

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
    final prefs = await SharedPreferences.getInstance();
    setState(() {
      _apiKey = prefs.getString('api_key') ?? '';
      _serverIp = prefs.getString('server_ip') ?? '';
      _serverPort = prefs.getInt('server_port') ?? 0;

      _apiKeyController.text = _apiKey;
      _serverIpController.text = _serverIp;
      _serverPortController.text = _serverPort == 0 ? '' : _serverPort.toString();
    });

    // If settings exist, maybe trigger connection if not already connected
    if (_apiKey.isNotEmpty && _serverIp.isNotEmpty && _serverPort != 0 && context.read<ChatBloc>().state is ChatInitial) {
      _connect();
    }
  }

  Future<void> _saveSettings() async {
    final prefs = await SharedPreferences.getInstance();
    final newApiKey = _apiKeyController.text;
    final newServerIp = _serverIpController.text;
    final newServerPort = int.tryParse(_serverPortController.text) ?? 0;

    await prefs.setString('api_key', newApiKey);
    await prefs.setString('server_ip', newServerIp);
    await prefs.setInt('server_port', newServerPort);

    setState(() {
      _apiKey = newApiKey;
      _serverIp = newServerIp;
      _serverPort = newServerPort;
    });

    ScaffoldMessenger.of(context).showSnackBar(
      const SnackBar(content: Text('Settings Saved!')),
    );

    // If settings are saved and we are not connected, try connecting
    if (_apiKey.isNotEmpty && _serverIp.isNotEmpty && _serverPort != 0 &&
        context.read<ChatBloc>().state is! ChatConnected &&
        context.read<ChatBloc>().state is! ChatLoading) {
      _connect();
    }
  }

  void _connect() {
     if (_serverIp.isNotEmpty && _serverPort != 0 && _apiKey.isNotEmpty) {
       context.read<ChatBloc>().add(ConnectWebSocketEvent(host: _serverIp, port: _serverPort));
     } else if (_serverIp.isEmpty || _serverPort == 0) {
       ScaffoldMessenger.of(context).showSnackBar(
         const SnackBar(content: Text('Please enter and save Server IP and Port.')),
       );
     } else if (_apiKey.isEmpty) {
        ScaffoldMessenger.of(context).showSnackBar(
         const SnackBar(content: Text('Please enter and save your API Key.')),
       );
     }
  }

  void _sendMessage() {
    if (_messageController.text.isNotEmpty && _apiKey.isNotEmpty) {
      // Send message via Bloc
      context.read<ChatBloc>().add(
            SendMessageEvent(_messageController.text, _apiKey),
          );
      _messageController.clear();
      _scrollToBottom(); // Scroll after sending
    } else if (_apiKey.isEmpty) {
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
          Padding(
            padding: const EdgeInsets.symmetric(horizontal: 8.0, vertical: 4.0),
            child: Column( // Use Column for multiple input fields
              children: [
                Row(
                  children: [
                    Expanded(
                      child: TextField(
                        controller: _serverIpController,
                        decoration: const InputDecoration(
                          hintText: 'Enter Server IP...',
                          labelText: 'Server IP',
                        ),
                        keyboardType: TextInputType.numberWithOptions(decimal: true), // Allow numbers and dots
                      ),
                    ),
                    SizedBox(width: 8.0), // Add spacing
                    Expanded(
                      child: TextField(
                        controller: _serverPortController,
                        decoration: const InputDecoration(
                          hintText: 'Enter Server Port...',
                          labelText: 'Server Port',
                        ),
                        keyboardType: TextInputType.number, // Allow only numbers
                      ),
                    ),
                  ],
                ),
                SizedBox(height: 8.0), // Add spacing
                Row(
                  children: [
                    Expanded(
                      child: TextField(
                        controller: _apiKeyController,
                        decoration: const InputDecoration(
                          hintText: 'Enter your API Key...',
                          labelText: 'API Key',
                        ),
                        obscureText: true,
                      ),
                    ),
                    IconButton(
                      icon: const Icon(Icons.save),
                      tooltip: 'Save Settings', // Changed tooltip
                      onPressed: _saveSettings, // Changed onPressed
                    ),
                  ],
                ),
              ],
            ),
          ),
          const Divider(),
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
                          padding: const EdgeInsets.all(10.0),
                          margin: const EdgeInsets.symmetric(vertical: 4.0, horizontal: 8.0),
                          decoration: BoxDecoration(
                            color: isUserMessage ? Colors.blue[100] : Colors.grey[300],
                            borderRadius: BorderRadius.circular(15.0),
                          ),
                          child: Text(
                            displayedMessage,
                            style: TextStyle(color: isUserMessage ? Colors.black87 : Colors.black87),
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

  @override
  void dispose() {
    _messageController.dispose();
    _apiKeyController.dispose();
    _serverIpController.dispose(); // Dispose new controller
    _serverPortController.dispose(); // Dispose new controller
    _scrollController.dispose(); // Dispose scroll controller
    super.dispose();
  }
}
