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
  // Remove the local _messages list, we'll use the Bloc state
  // final List<String> _messages = [];
  String _apiKey = '';
  final ScrollController _scrollController =
      ScrollController(); // For auto-scrolling

  @override
  void initState() {
    super.initState();
    _loadApiKey();
    // Optionally connect here if not done in main.dart's BlocProvider
    // context.read<ChatBloc>().add(const ConnectWebSocketEvent());
  }

  Future<void> _loadApiKey() async {
    final prefs = await SharedPreferences.getInstance();
    setState(() {
      _apiKey = prefs.getString('api_key') ?? '';
      _apiKeyController.text = _apiKey;
    });
    // If API key exists, maybe trigger connection if not already connected
    if (_apiKey.isNotEmpty && context.read<ChatBloc>().state is ChatInitial) {
      context.read<ChatBloc>().add(const ConnectWebSocketEvent());
    }
  }

  Future<void> _saveApiKey() async {
    final prefs = await SharedPreferences.getInstance();
    final newApiKey = _apiKeyController.text;
    await prefs.setString('api_key', newApiKey);
    setState(() {
      _apiKey = newApiKey;
    });
    ScaffoldMessenger.of(context).showSnackBar(
      const SnackBar(content: Text('API Key Saved!')),
    );
    // If API key is saved and we are not connected, try connecting
    if (_apiKey.isNotEmpty &&
        context.read<ChatBloc>().state is! ChatConnected &&
        context.read<ChatBloc>().state is! ChatLoading) {
      context.read<ChatBloc>().add(const ConnectWebSocketEvent());
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
                  onPressed: _apiKey.isNotEmpty
                      ? () => context
                          .read<ChatBloc>()
                          .add(const ConnectWebSocketEvent())
                      : null, // Disable if no API key
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
            child: Row(
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
                  tooltip: 'Save API Key',
                  onPressed: _saveApiKey,
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
                    _apiKey.isEmpty) {
                  return const Center(
                      child: Text(
                          'Please enter and save your API Key to connect.'));
                }
                if (state is ChatInitial &&
                    state.messages.isEmpty &&
                    _apiKey.isNotEmpty) {
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
                      return const ListTile(
                          title: Center(child: Text("AI is thinking...")));
                    }
                    // Ensure index is within bounds for messages
                    if (index < state.messages.length) {
                      return ListTile(
                        title: Text(state.messages[index]),
                      );
                    }
                    return const SizedBox
                        .shrink(); // Should not happen if logic is correct
                  },
                );
              },
            ),
          ),
          Padding(
            padding: const EdgeInsets.all(8.0),
            child: Row(
              children: [
                Expanded(
                  child: TextField(
                    controller: _messageController,
                    decoration: const InputDecoration(
                      hintText: 'Enter your message...',
                    ),
                    // Disable input if not connected
                    enabled: context.watch<ChatBloc>().state is ChatConnected,
                    onSubmitted: (_) => _sendMessage(),
                  ),
                ),
                // Disable send button if not connected
                BlocBuilder<ChatBloc, ChatState>(builder: (context, state) {
                  return IconButton(
                    icon: const Icon(Icons.send),
                    onPressed: state is ChatConnected
                        ? _sendMessage
                        : null, // Enable only when connected
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
    _scrollController.dispose(); // Dispose scroll controller
    super.dispose();
  }
}
