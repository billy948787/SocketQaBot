import 'dart:async'; // Import async

import 'package:bloc/bloc.dart';
import 'package:equatable/equatable.dart';
import 'package:qabot/services/chat_service.dart'; // Import ChatService
import 'package:qabot/services/chat_storage_service.dart'; // Import ChatStorageService
import 'package:flutter/foundation.dart'; // Import for kDebugMode
import 'dart:developer' as dev;

part 'chat_event.dart';
part 'chat_state.dart';

class ChatBloc extends Bloc<ChatEvent, ChatState> {
  final ChatService _chatService;
  final ChatStorageService _chatStorageService;
  StreamSubscription? _messageSubscription;
  String? _activeChatId; // Keep track of the active chat ID

  ChatBloc(this._chatService, this._chatStorageService)
      : super(const ChatInitial([], [], null)) {
    // Initialize with empty messages, chatRooms, and null chatId
    on<ConnectWebSocketEvent>(_onConnectWebSocket);
    on<SendMessageEvent>(_onSendMessage);
    on<ReceiveMessageEvent>(_onReceiveMessage);
    on<WebSocketConnectedEvent>(_onWebSocketConnected);
    on<WebSocketConnectionFailedEvent>(_onWebSocketConnectionFailed);
    on<WebSocketDisconnectedEvent>(_onWebSocketDisconnected);
    on<SettingsLoadedEvent>(_onSettingsLoaded);
    on<ClearChatHistoryEvent>(_onClearChatHistory);
    on<SelectChatRoomEvent>(_onSelectChatRoom);
    on<LoadChatHistoryEvent>(_onLoadChatHistory); // Added
    on<CreateChatRoomEvent>(_onCreateChatRoom);
    on<DeleteChatRoomEvent>(_onDeleteChatRoom);
    on<RenameChatRoomEvent>(_onRenameChatRoom);
    on<LoadChatRoomsEvent>(_onLoadChatRooms); // Added
    on<ChatRoomsLoadedEvent>(_onChatRoomsLoaded); // Added

    // Initially load chat rooms and then select the first one or handle no rooms
    add(const LoadChatRoomsEvent());
  }

  Future<void> _onLoadChatRooms(
      LoadChatRoomsEvent event, Emitter<ChatState> emit) async {
    final rooms = await _chatStorageService.loadChatRooms();
    emit(ChatInitial(state.messages, rooms, state.currentChatId,
        'Loaded chat rooms')); // Keep existing messages and chatId for now
    add(ChatRoomsLoadedEvent(rooms)); // Dispatch event to handle selection
  }

  void _onChatRoomsLoaded(
      ChatRoomsLoadedEvent event, Emitter<ChatState> emit) async {
    if (event.chatRooms.isNotEmpty) {
      // If there's an active chat ID, try to keep it. Otherwise, select the first.
      final currentActiveChatId = _activeChatId;
      bool foundActive = false;
      if (currentActiveChatId != null) {
        foundActive =
            event.chatRooms.any((room) => room.id == currentActiveChatId);
      }

      if (foundActive && currentActiveChatId != null) {
        add(SelectChatRoomEvent(currentActiveChatId));
      } else {
        add(SelectChatRoomEvent(event.chatRooms.first.id));
      }
    } else {
      // No chat rooms, maybe create a default one or stay in a state indicating no rooms
      // For now, just update the state with empty rooms
      emit(ChatInitial(
          [], [], null, 'No chat rooms found. Create one to start.'));
      _activeChatId = null;
    }
  }

  Future<void> _onSelectChatRoom(
      SelectChatRoomEvent event, Emitter<ChatState> emit) async {
    _activeChatId = event.chatId;
    // Emit loading state for the new chat room
    emit(ChatLoading(
        [], state.chatRooms, _activeChatId, 'Loading chat history...'));
    add(LoadChatHistoryEvent(event.chatId));
  }

  Future<void> _onLoadChatHistory(
      LoadChatHistoryEvent event, Emitter<ChatState> emit) async {
    if (event.chatId != _activeChatId)
      return; // Ensure we are loading for the active chat

    final history = await _chatStorageService.loadChatHistory(event.chatId);
    // Determine the correct state based on WebSocket connection status
    if (state is ChatConnected || _chatService.isConnected) {
      // Assuming ChatService has an isConnected getter
      emit(ChatConnected(
          history, state.chatRooms, _activeChatId, 'Chat history loaded.'));
    } else if (state is ChatLoading && state.statusMessage == 'Connecting...') {
      // If it was in the middle of connecting, keep it loading but with new history
      emit(ChatLoading(
          history, state.chatRooms, _activeChatId, state.statusMessage));
    } else {
      emit(ChatInitial(
          history, state.chatRooms, _activeChatId, 'Chat history loaded.'));
    }
  }

  void _onSettingsLoaded(SettingsLoadedEvent event, Emitter<ChatState> emit) {
    if (event.serverIp.isNotEmpty && event.serverPort != 0) {
      bool shouldConnect = false;
      if (state is ChatInitial ||
          state.statusMessage == 'Disconnected.' ||
          (state is ChatError &&
              (state as ChatError).error.contains('Connection Failed'))) {
        shouldConnect = true;
      }
      // Also connect if settings change and we are already connected but to a different server (not implemented here, needs _chatService to expose host/port)
      // Or if we are connected but the target host/port in event differs from current.

      if (shouldConnect) {
        add(ConnectWebSocketEvent(
            host: event.serverIp, port: event.serverPort));
      }
    } else if (state.messages.isEmpty && _activeChatId == null) {
      emit(ChatInitial(
          [], state.chatRooms, null, 'Please configure server settings.'));
    }
  }

  Future<void> _onCreateChatRoom(
      CreateChatRoomEvent event, Emitter<ChatState> emit) async {
    final newRoom = await _chatStorageService.createChatRoom(name: event.name);
    final updatedRooms = List<ChatRoomInfo>.from(state.chatRooms)..add(newRoom);
    emit(ChatInitial(state.messages, updatedRooms, state.currentChatId,
        'Chat room created.')); // Or ChatConnected if already connected
    // Optionally, select the new chat room
    add(SelectChatRoomEvent(newRoom.id));
  }

  Future<void> _onDeleteChatRoom(
      DeleteChatRoomEvent event, Emitter<ChatState> emit) async {
    await _chatStorageService.deleteChatRoom(event.chatId);
    final updatedRooms =
        state.chatRooms.where((room) => room.id != event.chatId).toList();

    String? newActiveChatId = _activeChatId;
    List<String> messagesForNextState = [];

    if (_activeChatId == event.chatId) {
      // If the deleted room was active
      if (updatedRooms.isNotEmpty) {
        newActiveChatId = updatedRooms.first.id;
        messagesForNextState =
            await _chatStorageService.loadChatHistory(newActiveChatId);
      } else {
        newActiveChatId = null; // No rooms left
        messagesForNextState = [];
      }
      _activeChatId = newActiveChatId;
    } else {
      messagesForNextState = state
          .messages; // Keep current messages if a different room was deleted
    }

    // Determine the correct base state
    if (state is ChatConnected || _chatService.isConnected) {
      emit(ChatConnected(messagesForNextState, updatedRooms, newActiveChatId,
          'Chat room deleted.'));
    } else {
      emit(ChatInitial(messagesForNextState, updatedRooms, newActiveChatId,
          'Chat room deleted.'));
    }
    // If a new room was made active, explicitly select it to load its history (if not already loaded)
    if (_activeChatId == event.chatId &&
        newActiveChatId != null &&
        messagesForNextState.isEmpty) {
      add(LoadChatHistoryEvent(newActiveChatId));
    } else if (_activeChatId == event.chatId && newActiveChatId == null) {
      // If no rooms left, ensure UI reflects this (e.g. ChatInitial with no messages)
      emit(ChatInitial([], [], null, 'All chat rooms deleted.'));
    }
  }

  Future<void> _onRenameChatRoom(
      RenameChatRoomEvent event, Emitter<ChatState> emit) async {
    await _chatStorageService.renameChatRoom(event.chatId, event.newName);
    final updatedRooms = List<ChatRoomInfo>.from(state.chatRooms);
    final roomIndex =
        updatedRooms.indexWhere((room) => room.id == event.chatId);
    if (roomIndex != -1) {
      updatedRooms[roomIndex] =
          updatedRooms[roomIndex].copyWith(name: event.newName);
      if (state is ChatConnected || _chatService.isConnected) {
        emit(ChatConnected(state.messages, updatedRooms, state.currentChatId,
            'Chat room renamed.'));
      } else {
        emit(ChatInitial(state.messages, updatedRooms, state.currentChatId,
            'Chat room renamed.'));
      }
    }
  }

  Future<void> _onClearChatHistory(
      ClearChatHistoryEvent event, Emitter<ChatState> emit) async {
    if (event.chatId != _activeChatId)
      return; // Ensure clearing for the active chat

    await _chatStorageService.clearChatHistory(event.chatId);
    if (state is ChatConnected) {
      emit(ChatConnected(
          [], state.chatRooms, _activeChatId, 'Chat history cleared.'));
    } else {
      emit(ChatInitial(
          [], state.chatRooms, _activeChatId, 'Chat history cleared.'));
    }
  }

  void _onConnectWebSocket(
      ConnectWebSocketEvent event, Emitter<ChatState> emit) {
    // if (state is ChatLoading || state is ChatConnected) return; // This check might be too restrictive if settings change.
    // Consider allowing re-connection if host/port changes.
    // For now, we allow re-attempts.

    _chatService
        .closeConnection(); // Ensure any old connection is closed first.
    _messageSubscription?.cancel();

    emit(ChatLoading(
        state.messages, state.chatRooms, _activeChatId, 'Connecting...'));
    try {
      final messageStream =
          _chatService.connectAndListen(host: event.host, port: event.port);
      _messageSubscription = messageStream.listen(
        (message) {
          add(ReceiveMessageEvent(message));
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
      // For now, we optimistically add WebSocketConnectedEvent. ChatService should ideally confirm.
      // If connectAndListen throws, it's caught below. If stream setup is ok, we assume connected.
      // The actual "connected" state is confirmed by server messages or lack of immediate error.
      // Let's add the WebSocketConnectedEvent here, as _chatService.connectAndListen is synchronous in its setup.
      add(const WebSocketConnectedEvent()); // Moved here
    } catch (e) {
      add(WebSocketConnectionFailedEvent(
          'Failed to initiate connection: ${e.toString()}'));
    }
  }

  void _onWebSocketConnected(
      WebSocketConnectedEvent event, Emitter<ChatState> emit) {
    // When connected, messages should be for the _activeChatId
    // If _activeChatId is null (e.g. no rooms yet), messages should be empty.
    emit(ChatConnected(
        _activeChatId != null
            ? state.messages
            : [], // Use current messages if a chat is active
        state.chatRooms,
        _activeChatId));
  }

  void _onWebSocketConnectionFailed(
      WebSocketConnectionFailedEvent event, Emitter<ChatState> emit) {
    _messageSubscription?.cancel();
    _messageSubscription = null;
    emit(ChatError(state.messages, state.chatRooms, _activeChatId,
        'Connection Failed: ${event.error}'));
  }

  void _onWebSocketDisconnected(
      WebSocketDisconnectedEvent event, Emitter<ChatState> emit) {
    _messageSubscription?.cancel();
    _messageSubscription = null;
    emit(ChatInitial(
        state.messages, state.chatRooms, _activeChatId, 'Disconnected.'));
  }

  Future<void> _onSendMessage(
      SendMessageEvent event, Emitter<ChatState> emit) async {
    if (event.chatId != _activeChatId) return; // Message for a non-active chat
    if (state is! ChatConnected) {
      emit(ChatError(state.messages, state.chatRooms, _activeChatId,
          'Not connected to the server.'));
      return;
    }

    final userMessage = 'You: ${event.message}';
    final updatedMessages = List<String>.from(state.messages)..add(userMessage);
    emit(ChatLoading(
        updatedMessages, state.chatRooms, _activeChatId, 'AI is thinking...'));
    await _chatStorageService.saveChatHistory(
        _activeChatId!, updatedMessages); // Use _activeChatId!

    try {
      await _chatService.sendMessage(
        context: event.context,
        message: event.message,
        prompt: event.prompt,
        modelName: event.modelName,
        contextLimit: event.contextLimit,
      );
    } catch (e) {
      final errorMessages = List<String>.from(updatedMessages)
        ..add('Error sending message: ${e.toString()}');
      emit(ChatError(errorMessages, state.chatRooms, _activeChatId,
          'Failed to send message: ${e.toString()}'));
      _chatService.closeConnection();
      add(const WebSocketDisconnectedEvent());
    }
  }

  void _onReceiveMessage(ReceiveMessageEvent event, Emitter<ChatState> emit) {
    if (_activeChatId == null) return; // No active chat to receive message for

    final List<String> currentMessages = List<String>.from(state.messages);
    final String newMessageChunk = event.message;

    if (kDebugMode) {
      dev.log(
          'Received message chunk in Bloc for chat $_activeChatId: $newMessageChunk');
    }

    if (currentMessages.isNotEmpty && currentMessages.last.startsWith('AI: ')) {
      currentMessages[currentMessages.length - 1] =
          currentMessages.last + newMessageChunk;
    } else {
      currentMessages.add('AI: $newMessageChunk');
    }

    emit(ChatConnected(currentMessages, state.chatRooms, _activeChatId));
    _chatStorageService.saveChatHistory(
        _activeChatId!, currentMessages); // Use _activeChatId!
  }

  @override
  Future<void> close() {
    _messageSubscription?.cancel();
    _chatService.closeConnection();
    return super.close();
  }
}
