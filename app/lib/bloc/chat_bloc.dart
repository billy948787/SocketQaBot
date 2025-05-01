import 'dart:async'; // Import async

import 'package:bloc/bloc.dart';
import 'package:equatable/equatable.dart';
import 'package:qabot/services/chat_service.dart'; // Import ChatService
import 'package:flutter/foundation.dart'; // Import for kDebugMode
import 'dart:developer' as dev;

part 'chat_event.dart';
part 'chat_state.dart';

class ChatBloc extends Bloc<ChatEvent, ChatState> {
  final ChatService _chatService; // Inject ChatService
  StreamSubscription?
      _messageSubscription; // To listen to messages from service

  // Corrected initial state call
  ChatBloc(this._chatService) : super(const ChatInitial()) {
    // Accept ChatService in constructor
    on<ConnectWebSocketEvent>(_onConnectWebSocket);
    on<SendMessageEvent>(_onSendMessage);
    on<ReceiveMessageEvent>(_onReceiveMessage);
    on<WebSocketConnectedEvent>(_onWebSocketConnected);
    on<WebSocketConnectionFailedEvent>(_onWebSocketConnectionFailed);
    on<WebSocketDisconnectedEvent>(_onWebSocketDisconnected);
    on<SettingsLoadedEvent>(_onSettingsLoaded); // Add handler for SettingsLoadedEvent

    // Optionally, connect immediately when the BLoC is created
    // add(const ConnectWebSocketEvent());
  }

  // New event handler for SettingsLoadedEvent
  void _onSettingsLoaded(SettingsLoadedEvent event, Emitter<ChatState> emit) {
    // Update the state with the loaded settings, keeping existing messages
    emit(ChatInitial(state.statusMessage, event.apiKey)); // Update initial state with API key
    // If settings exist and we are in the initial state, try connecting
    if (event.apiKey.isNotEmpty && event.serverIp.isNotEmpty && event.serverPort != 0 && state is ChatInitial) {
       add(ConnectWebSocketEvent(host: event.serverIp, port: event.serverPort));
    }
  }


  void _onConnectWebSocket(
      ConnectWebSocketEvent event, Emitter<ChatState> emit) {
    if (state is ChatLoading || state is ChatConnected)
      return; // Avoid reconnecting if already connected/connecting

    // Pass apiKey to the new state
    emit(ChatLoading(
        state.messages, state.apiKey, 'Connecting...')); // Indicate connection attempt
    try {
      final messageStream = _chatService.connectAndListen(host: event.host, port: event.port);
      _messageSubscription?.cancel(); // Cancel previous subscription if any
      _messageSubscription = messageStream.listen(
        (message) {
          add(ReceiveMessageEvent(message)); // Add received message event
        },
        onError: (error) {
          add(WebSocketConnectionFailedEvent(error.toString()));
        },
        onDone: () {
          add(const WebSocketDisconnectedEvent());
        },
      );
      // Assuming connection is successful if no immediate error from connectAndListen stream setup
      // A more robust approach might involve a confirmation message from the server
      add(const WebSocketConnectedEvent());
    } catch (e) {
      add(WebSocketConnectionFailedEvent(
          'Failed to initiate connection: ${e.toString()}'));
    }
  }

  void _onWebSocketConnected(
      WebSocketConnectedEvent event, Emitter<ChatState> emit) {
    // Pass apiKey to the new state
    emit(ChatConnected(state.messages, state.apiKey)); // Move to Connected state
  }

  void _onWebSocketConnectionFailed(
      WebSocketConnectionFailedEvent event, Emitter<ChatState> emit) {
    _messageSubscription?.cancel();
    _messageSubscription = null;
    // Pass apiKey to the new state
    emit(ChatError(state.messages, state.apiKey, 'Connection Failed: ${event.error}'));
  }

  void _onWebSocketDisconnected(
      WebSocketDisconnectedEvent event, Emitter<ChatState> emit) {
    _messageSubscription?.cancel();
    _messageSubscription = null;
    // Decide if you want to go back to Initial or stay in an Error/Disconnected state
    // Corrected: Pass the status message correctly to ChatInitial
    // Pass apiKey to the new state
    emit(
        ChatInitial('Disconnected.', state.apiKey)); // Or a specific Disconnected state
  }

  Future<void> _onSendMessage(
      SendMessageEvent event, Emitter<ChatState> emit) async {
    // Ensure we are connected before sending
    if (state is! ChatConnected) {
      // Pass apiKey to the new state
      emit(ChatError(state.messages, state.apiKey, 'Not connected to the server.'));
      // Optionally try to reconnect here: add(const ConnectWebSocketEvent());
      return;
    }

    // Add user message immediately
    final userMessage = 'You: ${event.message}';
    final updatedMessages = List<String>.from(state.messages)..add(userMessage);
    // Keep the state as ChatConnected while waiting for response, but indicate thinking
    // Pass apiKey to the new state
    emit(ChatLoading(
        updatedMessages, state.apiKey, 'AI is thinking...')); // Show thinking state

    try {
      // Use the ChatService to send the message, passing the API key from the state
      await _chatService.sendMessage( // Add await
        message: event.message,
        apiKey: state.apiKey, // Pass API key from state
        // prompt: 'Your desired prompt', // Optional: customize prompt
        // modelName: 'your-model-name' // Optional: customize model
      );
      // Note: The response will arrive via the _messageSubscription
      // and trigger ReceiveMessageEvent. We don't add AI message here directly.
    } catch (e) {
      // If sending fails immediately (e.g., socket closed unexpectedly)
      final errorMessages = List<String>.from(
          updatedMessages) // Use updatedMessages which includes the user's message
        ..add('Error sending message: ${e.toString()}');
      // Pass apiKey to the new state
      emit(ChatError(errorMessages, state.apiKey, 'Failed to send message: ${e.toString()}'));
      // Handle potential disconnection due to error
      _chatService.closeConnection();
      add(const WebSocketDisconnectedEvent());
    }
  }

  void _onReceiveMessage(ReceiveMessageEvent event, Emitter<ChatState> emit) {
    final List<String> currentMessages = List<String>.from(state.messages);
    final String newMessageChunk = event.message;

    // Add debug print to check the received chunk
    if (kDebugMode) {
      dev.log('Received message chunk in Bloc: $newMessageChunk');
    }

    // Check if the last message is an AI message to append to
    if (currentMessages.isNotEmpty && currentMessages.last.startsWith('AI: ')) {
      // Append the new chunk to the last AI message
      currentMessages[currentMessages.length - 1] =
          currentMessages.last + newMessageChunk;
    } else {
      // Otherwise, start a new AI message entry
      // This handles the first chunk or if the previous message was from the user
      currentMessages.add('AI: $newMessageChunk');
    }

    // Add debug print to check the updated messages list
    if (kDebugMode) {
      dev.log('Updated messages list length: ${currentMessages.length}');
      dev.log('Updated messages: $currentMessages');
    }

    // Emit the updated state, staying in ChatConnected
    // Pass apiKey to the new state
    emit(ChatConnected(currentMessages, state.apiKey));
  }

  @override
  Future<void> close() {
    _messageSubscription?.cancel(); // Cancel subscription when BLoC closes
    _chatService.closeConnection(); // Close socket connection
    return super.close();
  }
}
