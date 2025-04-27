import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart'; // Import Bloc provider
import 'package:qabot/bloc/chat_bloc.dart'; // Import ChatBloc
import 'package:qabot/services/chat_service.dart'; // Import ChatService
import 'package:qabot/views/chat_view.dart'; // Import ChatView

void main() {
  // It's good practice to ensure Flutter bindings are initialized
  WidgetsFlutterBinding.ensureInitialized();

  // Create an instance of ChatService
  final chatService = ChatService();

  runApp(MyApp(chatService: chatService)); // Pass ChatService to MyApp
}

class MyApp extends StatelessWidget {
  final ChatService chatService; // Accept ChatService

  const MyApp({super.key, required this.chatService});

  @override
  Widget build(BuildContext context) {
    // Provide the ChatBloc to the widget tree
    return BlocProvider(
      // Use the injected chatService instance
      create: (context) => ChatBloc(chatService),
      // Don't automatically connect here, let ChatView handle it after loading API key
      // ..add(const ConnectWebSocketEvent()),
      child: MaterialApp(
        title: 'QABot',
        theme: ThemeData(
          primarySwatch: Colors.blue,
          visualDensity: VisualDensity.adaptivePlatformDensity,
        ),
        home: const ChatView(), // Set ChatView as the home screen
      ),
    );
  }
}
