import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart';
import 'package:provider/provider.dart';
import 'package:qabot/bloc/chat_bloc.dart';
import 'package:flutter_secure_storage/flutter_secure_storage.dart';
import 'package:flutter_markdown/flutter_markdown.dart';
import 'dart:developer' as dev;
import 'package:qabot/providers/theme_provider.dart';
import 'package:qabot/views/settings_view.dart'; // Import the new SettingsView
import 'package:qabot/widgets/chat_drawer.dart'; // Import the ChatDrawer
import 'package:qabot/services/tts_service.dart'; // 引入 TTS 服務
import 'package:qabot/services/stt_service.dart'; // 引入 STT 服務
import 'package:qabot/widgets/tts_button.dart'; // 引入 TTS 按鈕小部件

class ChatView extends StatefulWidget {
  const ChatView({super.key});

  @override
  State<ChatView> createState() => _ChatViewState();
}

class _ChatViewState extends State<ChatView> {
  final TextEditingController _messageController = TextEditingController();
  // Remove _serverIpController and _serverPortController as they are managed in SettingsView

  final _storage =
      const FlutterSecureStorage(); // Keep for loading initial settings

  String _serverIp = '';
  int _serverPort = 0;
  int _contextLimit = 10;

  // 添加 TTS 和 STT 服務
  final TTSService _ttsService = TTSService();
  final STTService _sttService = STTService();
  bool _isListening = false; // 追蹤是否正在進行語音辨識

  final ScrollController _scrollController =
      ScrollController(); // For auto-scrolling

  @override
  void initState() {
    super.initState();
    _loadSettings();
    _sttService.initialize();
    _sttService.onStateChanged = () {
      if (mounted) {
        setState(() {
          _isListening = _sttService.isListening;
        });
      }
    };
  }

  Future<void> _loadSettings() async {
    // Load settings from secure storage
    _serverIp = await _storage.read(key: 'server_ip') ?? '';
    final serverPortString = await _storage.read(key: 'server_port');
    _serverPort = int.tryParse(serverPortString ?? '') ?? 0;
    final contextLimitString = await _storage.read(key: 'context_limit');
    _contextLimit = int.tryParse(contextLimitString ?? '') ?? 10;

    // No longer need to set controller text here as they are in SettingsView

    // Dispatch SettingsLoadedEvent after loading settings
    if (context.mounted) {
      context.read<ChatBloc>().add(SettingsLoadedEvent(
            serverIp: _serverIp,
            serverPort: _serverPort,
            // contextLimit is not part of SettingsLoadedEvent, it's used directly in _sendMessage
          ));
    }

    context
        .read<ChatBloc>()
        .add(ConnectWebSocketEvent(host: _serverIp, port: _serverPort));
  }

  void _connect() {
    // Connection logic is now handled by the Bloc when settings are loaded/saved
    // We can still keep this function if needed for a dedicated connect button,
    // but it should dispatch a ConnectWebSocketEvent with the current settings from the state.
    // For now, we'll rely on the SettingsLoadedEvent to trigger connection.
    // Use the local state variables for IP and Port
    if (_serverIp.isNotEmpty && _serverPort != 0) {
      context
          .read<ChatBloc>()
          .add(ConnectWebSocketEvent(host: _serverIp, port: _serverPort));
    } else {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Please enter and save Server IP, Port')),
      );
    }
  }

  // 切換語音辨識狀態，辨識結束時自動發送輸入文字
  void _toggleVoiceRecognition() async {
    if (_sttService.isListening) {
      await _sttService.stopListening(onFinalResult: (text) {
        _messageController.text = text;
        if (text.isNotEmpty) _sendMessage();
      });
    } else {
      await _sttService.startListening(
        onResult: (text) {
          setState(() {
            _messageController.text = text;
          });
        },
        onFinished: () {
          final text = _messageController.text;
          if (text.isNotEmpty) _sendMessage();
          setState(() {
            _isListening = false;
          });
        },
      );
    }
  }

  void _sendMessage() {
    if (_messageController.text.isNotEmpty) {
      // 如果正在語音辨識，先停止
      if (_sttService.isListening) {
        _sttService.stopListening();
      }

      // 使用 BlocProvider 來訪問 ChatBloc
      final chatBloc = BlocProvider.of<ChatBloc>(context);
      final currentState = chatBloc.state;

      // 確保有選中的聊天室
      if (currentState.currentChatId == null) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('請先選擇或創建一個聊天室')),
        );
        return;
      }

      List<String> historyMessages =
          List<String>.from(currentState.messages); // 創建歷史訊息的副本

      // 將 List<String> 轉換為 List<Map<String, String>>，供後端處理
      List<Map<String, String>> chatContext = [];
      for (int i = 0; i < historyMessages.length; i++) {
        final message = historyMessages[i];

        if (message.startsWith('You: ')) {
          chatContext.add({'user': message.substring(5)});
        } else if (message.startsWith('AI: ')) {
          chatContext.add({'model': message.substring(4)});
        }
        dev.log(
          chatContext.toString(),
        );
      }

      // 根據 contextLimit 限制歷史記錄的數量
      final limitedContext =
          _contextLimit > 0 && chatContext.length > _contextLimit
              ? chatContext.sublist(chatContext.length - _contextLimit)
              : chatContext;

      chatBloc.add(
        SendMessageEvent(
            chatId: currentState.currentChatId!, // 使用當前選中的聊天室 ID
            message: _messageController.text,
            context: limitedContext, // 傳遞格式化並限制的歷史記錄
            contextLimit: _contextLimit
            // prompt 和 modelName 將使用 SendMessageEvent 建構子中的預設值
            ),
      );
      _messageController.clear();
      _scrollToBottom(); // 發送後滾動到底部
    } else if (_messageController.text.isEmpty) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('請輸入訊息')),
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
      drawer: const ChatDrawer(),
      appBar: AppBar(
        // 只顯示聊天室名稱，不顯示狀態
        title: BlocBuilder<ChatBloc, ChatState>(
          builder: (context, state) {
            if (state.currentChatId != null && state.chatRooms.isNotEmpty) {
              // 如果有選中的聊天室，顯示其名稱
              final currentRoom = state.chatRooms.firstWhere(
                (room) => room.id == state.currentChatId,
                orElse: () => state.chatRooms.first,
              );

              return Text(currentRoom.name);
            } else {
              // 如果沒有選中的聊天室，顯示默認標題
              return const Text('AI Chat');
            }
          },
        ),
        actions: [
          // Add a theme toggle button
          IconButton(
            icon: Icon(
                Provider.of<ThemeProvider>(context).themeMode == ThemeMode.dark
                    ? Icons.light_mode
                    : Icons.dark_mode),
            tooltip: '切換主題',
            onPressed: () {
              Provider.of<ThemeProvider>(context, listen: false).toggleTheme();
            },
          ),
          // Add a settings button
          IconButton(
            icon: const Icon(Icons.settings),
            tooltip: '設定',
            onPressed: _navigateToSettings,
          ),
          // Add a clear chat history button
          BlocBuilder<ChatBloc, ChatState>(
            builder: (context, state) {
              if (state.currentChatId != null) {
                return IconButton(
                  icon: const Icon(Icons.delete_sweep),
                  tooltip: '清除對話歷史',
                  onPressed: () {
                    // Show a confirmation dialog before clearing
                    showDialog(
                      context: context,
                      builder: (BuildContext dialogContext) {
                        return AlertDialog(
                          title: const Text('確認清除'),
                          content: const Text('確定要清除當前聊天室的所有對話歷史嗎？'),
                          actions: <Widget>[
                            TextButton(
                              child: const Text('取消'),
                              onPressed: () {
                                Navigator.of(dialogContext).pop();
                              },
                            ),
                            TextButton(
                              child: const Text('清除'),
                              onPressed: () {
                                Navigator.of(dialogContext).pop();
                                context.read<ChatBloc>().add(
                                    ClearChatHistoryEvent(
                                        state.currentChatId!));
                              },
                            ),
                          ],
                        );
                      },
                    );
                  },
                );
              }
              return const SizedBox.shrink(); // 沒有選中聊天室時不顯示
            },
          ),
          // Add a reconnect button
          BlocBuilder<ChatBloc, ChatState>(
            builder: (context, state) {
              // Show reconnect button if not connected or loading
              if (state is! ChatConnected && state is! ChatLoading) {
                return IconButton(
                  icon: const Icon(Icons.refresh),
                  tooltip: '重新連接',
                  onPressed: (_serverIp.isNotEmpty && _serverPort != 0)
                      ? _connect
                      : null, // Disable if no IP, or Port
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
                if (state is ChatInitial) {
                  context.read<ChatBloc>().add(ConnectWebSocketEvent(
                      host: _serverIp, port: _serverPort));
                }
              },
              builder: (context, state) {
                // Handle initial state explicitly if needed
                if (state is ChatInitial &&
                    state.messages.isEmpty &&
                    (_serverIp.isEmpty || _serverPort == 0)) {
                  return const Center(
                      child: Text(
                          'Please enter and save Server IP, Port to connect.'));
                }
                if (state is ChatInitial &&
                    state.messages.isEmpty &&
                    _serverIp.isNotEmpty &&
                    _serverPort != 0) {
                  return Center(
                      child: Text(state.statusMessage.isNotEmpty
                          ? state.statusMessage
                          : 'Connecting...'));
                }
                if (state is ChatLoading && state.messages.isEmpty) {
                  return Center(
                      child: Column(
                    mainAxisAlignment: MainAxisAlignment.center,
                    children: [
                      const CircularProgressIndicator(),
                      const SizedBox(height: 10),
                      Text(state.statusMessage),
                    ],
                  ));
                }

                dev.log("now state : ${state.toString()}");

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
                      final displayedMessage = isUserMessage
                          ? message.substring(5)
                          : message
                              .substring(4); // Remove "You: " or "AI: " prefix

                      // Modern chat bubble styling
                      final ThemeData theme = Theme.of(context);
                      final bool isDarkMode =
                          theme.brightness == Brightness.dark;

                      final userBubbleColor = isDarkMode
                          ? theme.colorScheme.primaryContainer
                          : theme.colorScheme.primary;
                      final aiBubbleColor = isDarkMode
                          ? theme.colorScheme.surfaceVariant
                          : Colors.grey.shade200;
                      final userTextColor = isDarkMode
                          ? theme.colorScheme.onPrimaryContainer
                          : theme.colorScheme.onPrimary;
                      final aiTextColor = isDarkMode
                          ? theme.colorScheme.onSurfaceVariant
                          : Colors.black87;

                      return Column(
                        children: [
                          // 訊息氣泡
                          Align(
                            alignment: isUserMessage
                                ? Alignment.centerRight
                                : Alignment.centerLeft,
                            child: Container(
                              constraints: BoxConstraints(
                                  maxWidth: MediaQuery.of(context).size.width *
                                      0.75), // Slightly increased width
                              padding: const EdgeInsets.symmetric(
                                  vertical: 10.0,
                                  horizontal: 16.0), // Adjusted padding
                              margin: const EdgeInsets.only(
                                  top: 5.0,
                                  bottom: 0.0, // 移除底部邊距，讓按鈕貼近
                                  left: 8.0,
                                  right: 8.0), // Adjusted margin
                              decoration: BoxDecoration(
                                color: isUserMessage
                                    ? userBubbleColor
                                    : aiBubbleColor,
                                borderRadius: BorderRadius.only(
                                  topLeft: const Radius.circular(20.0),
                                  topRight: const Radius.circular(20.0),
                                  bottomLeft: isUserMessage
                                      ? const Radius.circular(20.0)
                                      : const Radius.circular(
                                          4.0), // Pointy corner for AI
                                  bottomRight: isUserMessage
                                      ? const Radius.circular(
                                          4.0) // Pointy corner for User
                                      : const Radius.circular(20.0),
                                ),
                                boxShadow: [
                                  BoxShadow(
                                    color: Colors.black.withOpacity(0.05),
                                    spreadRadius: 1,
                                    blurRadius: 3,
                                    offset: const Offset(0, 2),
                                  ),
                                ],
                              ),
                              child: isUserMessage
                                  ? Text(
                                      displayedMessage,
                                      style: TextStyle(
                                          color: userTextColor,
                                          fontSize: 16), // Slightly larger font
                                    )
                                  : MarkdownBody(
                                      data: displayedMessage,
                                      styleSheet: MarkdownStyleSheet.fromTheme(
                                              Theme.of(context))
                                          .copyWith(
                                        p: Theme.of(context)
                                            .textTheme
                                            .bodyMedium
                                            ?.copyWith(
                                                color: aiTextColor,
                                                fontSize:
                                                    16), // Slightly larger font
                                      ),
                                    ),
                            ),
                          ),
                          // TTS 按鈕獨立在訊息氣泡下方
                          Align(
                            alignment: isUserMessage
                                ? Alignment.centerRight
                                : Alignment.centerLeft,
                            child: Padding(
                              padding: EdgeInsets.only(
                                right: isUserMessage ? 14.0 : 0.0,
                                left: isUserMessage ? 0.0 : 14.0,
                                bottom: 2.0,
                              ),
                              child: TTSButton(
                                text: displayedMessage,
                                ttsService: _ttsService,
                                color: isUserMessage
                                    ? Theme.of(context).colorScheme.primary
                                    : Theme.of(context).colorScheme.secondary,
                                size: 16, // 較小的按鈕尺寸
                              ),
                            ),
                          ),
                        ],
                      );
                    }
                    return const SizedBox
                        .shrink(); // Should not happen if logic is correct
                  },
                );
              },
            ),
          ),
          const Divider(height: 1.0),
          Container(
            // Wrap with Container for background and padding
            decoration: BoxDecoration(
              color:
                  Theme.of(context).cardColor, // Use cardColor for background
              boxShadow: [
                BoxShadow(
                  color: Colors.black.withOpacity(0.05),
                  spreadRadius: 0,
                  blurRadius: 10,
                  offset: const Offset(0, -5), // Shadow upwards
                ),
              ],
            ),
            padding: const EdgeInsets.only(
                left: 16.0,
                right: 8.0,
                top: 12.0,
                bottom: 12.0), // Adjusted padding
            child: Row(
              children: [
                Expanded(
                  child: TextField(
                    controller: _messageController,
                    decoration: InputDecoration(
                      hintText: '輸入您的訊息...', // Updated hint text
                      border: OutlineInputBorder(
                        borderRadius:
                            BorderRadius.circular(30.0), // More rounded
                        borderSide: BorderSide.none,
                      ),
                      filled: true,
                      fillColor: Theme.of(context)
                          .scaffoldBackgroundColor, // Use scaffoldBackgroundColor or a light grey
                      contentPadding: const EdgeInsets.symmetric(
                          horizontal: 20.0, vertical: 14.0), // Adjusted padding
                      // 添加語音輸入按鈕
                      suffixIcon: IconButton(
                        icon: Icon(
                          _isListening ? Icons.close : Icons.mic,
                          color: _isListening
                              ? Theme.of(context).colorScheme.error
                              : Theme.of(context).colorScheme.onSurfaceVariant,
                        ),
                        onPressed: _toggleVoiceRecognition,
                        tooltip: _isListening ? '停止錄音' : '開始錄音',
                      ),
                    ),
                    // Disable input if not connected
                    onSubmitted: (_) => _sendMessage(),
                    style:
                        const TextStyle(fontSize: 16), // Consistent font size
                  ),
                ),
                const SizedBox(width: 8.0),
                BlocBuilder<ChatBloc, ChatState>(builder: (context, state) {
                  return FloatingActionButton(
                    onPressed: state is ChatConnected ? _sendMessage : null,
                    backgroundColor: Theme.of(context)
                        .colorScheme
                        .primary, // Use primary color
                    elevation: 2.0, // Add some elevation
                    mini: false, // Make it standard size
                    shape: RoundedRectangleBorder(
                        borderRadius:
                            BorderRadius.circular(28)), // Make it more circular
                    child: Icon(Icons.send,
                        color: Theme.of(context).colorScheme.onPrimary),
                  );
                }),
              ],
            ),
          ),
        ],
      ),
    );
  }

  Future<void> _navigateToSettings() async {
    final result = await Navigator.push(
      context,
      MaterialPageRoute(
        builder: (context) => SettingsView(
          initialServerIp: _serverIp,
          initialServerPort: _serverPort,
          initialContextLimit: _contextLimit,
        ),
      ),
    );

    if (result == true && mounted) {
      // If settings were saved in SettingsView, reload them in ChatView
      await _loadSettings(); // This will also re-dispatch SettingsLoadedEvent
    }
  }

  @override
  void dispose() {
    _messageController.dispose();
    _scrollController.dispose();
    _ttsService.dispose(); // 釋放 TTS 資源
    _sttService.onStateChanged = null;
    _sttService.dispose();
    super.dispose();
  }
}
