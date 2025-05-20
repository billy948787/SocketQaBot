import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart';
import 'package:provider/provider.dart'; // Import Provider
import 'package:qabot/bloc/chat_bloc.dart';
import 'package:qabot/views/chat_view.dart';
import 'package:qabot/providers/theme_provider.dart'; // Import ThemeProvider
import 'package:qabot/services/chat_service.dart'; // Import ChatService
import 'package:qabot/services/chat_storage_service.dart'; // Import ChatStorageService

void main() {
  runApp(const MyApp());
}

class MyApp extends StatelessWidget {
  const MyApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MultiProvider(
      providers: [
        ChangeNotifierProvider(create: (_) => ThemeProvider()),
        BlocProvider(
          create: (context) => ChatBloc(ChatService(), ChatStorageService()),
        ),
      ],
      child: Consumer<ThemeProvider>(
        // Consume ThemeProvider
        builder: (context, themeProvider, child) {
          return MaterialApp(  
            title: 'Socket QA Bot',
            theme: ThemeData.light().copyWith(
              colorScheme: ColorScheme.fromSeed(seedColor: Colors.blueAccent),
              // Add other light theme customizations here if needed
            ),
            darkTheme: ThemeData.dark().copyWith(
              colorScheme: ColorScheme.fromSeed(
                  seedColor: Colors.blueAccent, brightness: Brightness.dark),
              // Add other dark theme customizations here if needed
            ),
            themeMode:
                themeProvider.themeMode, // Use themeMode from ThemeProvider
            home: const ChatView(),
          );
        },
      ),
    );
  }
}
