part of 'chat_bloc.dart';

abstract class ChatEvent extends Equatable {
  const ChatEvent();

  @override
  List<Object?> get props => []; // Allow null in props for optional fields
}

// Event to select a chat room
class SelectChatRoomEvent extends ChatEvent {
  final String chatId;

  const SelectChatRoomEvent(this.chatId);

  @override
  List<Object> get props => [chatId];
}

// Event to load chat history for a specific room
class LoadChatHistoryEvent extends ChatEvent {
  final String chatId;

  const LoadChatHistoryEvent(this.chatId);

  @override
  List<Object> get props => [chatId];
}

// Event for sending a message
class SendMessageEvent extends ChatEvent {
  final String chatId; // Added chatId
  final String message;
  final List<Map<String, String>> context;
  final String prompt;
  final String modelName;
  final int contextLimit;

  const SendMessageEvent({
    required this.chatId, // Added chatId
    required this.message,
    required this.context,
    this.prompt = '使用繁體中文回答',
    this.modelName = 'gemini-2.0-flash',
    this.contextLimit = 10,
  });

  @override
  List<Object> get props =>
      [chatId, message, prompt, modelName, contextLimit, context];
}

// Event for receiving a message (can be triggered internally by the BLoC)
// Remains unchanged as it pertains to the currently active connection/chat
class ReceiveMessageEvent extends ChatEvent {
  final String message;

  const ReceiveMessageEvent(this.message);

  @override
  List<Object> get props => [message];
}

// Event to initialize connection (remains largely the same, connection is global for now)
class ConnectWebSocketEvent extends ChatEvent {
  final String host;
  final int port;

  const ConnectWebSocketEvent({required this.host, required this.port});

  @override
  List<Object> get props => [host, port];
}

// Event when connection is established (internal)
class WebSocketConnectedEvent extends ChatEvent {
  const WebSocketConnectedEvent();
}

// Event when connection fails (internal)
class WebSocketConnectionFailedEvent extends ChatEvent {
  final String error;
  const WebSocketConnectionFailedEvent(this.error);
  @override
  List<Object> get props => [error];
}

// Event when connection is closed (internal)
class WebSocketDisconnectedEvent extends ChatEvent {
  const WebSocketDisconnectedEvent();
}

// Event when settings are loaded from storage
class SettingsLoadedEvent extends ChatEvent {
  final String serverIp;
  final int serverPort;
  // Removed contextLimit as it's now part of SendMessageEvent or per-chat settings if needed later

  const SettingsLoadedEvent({
    required this.serverIp,
    required this.serverPort,
  });

  @override
  List<Object> get props => [serverIp, serverPort];
}

// Event to clear chat history for a specific room
class ClearChatHistoryEvent extends ChatEvent {
  final String chatId; // Added chatId
  const ClearChatHistoryEvent(this.chatId);

  @override
  List<Object> get props => [chatId];
}

// Event to create a new chat room
class CreateChatRoomEvent extends ChatEvent {
  final String? name; // Optional name for the new chat room

  const CreateChatRoomEvent({this.name});

  @override
  List<Object?> get props => [name];
}

// Event to delete a chat room
class DeleteChatRoomEvent extends ChatEvent {
  final String chatId;

  const DeleteChatRoomEvent(this.chatId);

  @override
  List<Object> get props => [chatId];
}

// Event to rename a chat room
class RenameChatRoomEvent extends ChatEvent {
  final String chatId;
  final String newName;

  const RenameChatRoomEvent(this.chatId, this.newName);

  @override
  List<Object> get props => [chatId, newName];
}

// Event when the list of chat rooms has been loaded
class ChatRoomsLoadedEvent extends ChatEvent {
  final List<ChatRoomInfo> chatRooms;

  const ChatRoomsLoadedEvent(this.chatRooms);

  @override
  List<Object> get props => [chatRooms];
}

// Event to trigger loading of all chat rooms
class LoadChatRoomsEvent extends ChatEvent {
  const LoadChatRoomsEvent();
}
