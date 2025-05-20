part of 'chat_bloc.dart';

abstract class ChatState extends Equatable {
  final String? currentChatId; // ID of the currently active chat room
  final List<String> messages; // Messages for the currentChatId
  final String statusMessage;
  final List<ChatRoomInfo> chatRooms; // List of all available chat rooms

  const ChatState(this.messages, this.chatRooms, this.currentChatId,
      [this.statusMessage = '']);

  @override
  List<Object?> get props =>
      [currentChatId, messages, statusMessage, chatRooms];
}

// Initial state
class ChatInitial extends ChatState {
  const ChatInitial(
      super.messages, // Messages for the (potentially) initially selected chat
      super.chatRooms, // Initially loaded chat rooms
      super.currentChatId, // Initially selected chat ID (can be null)
      [super.statusMessage = '']);
}

// State when loading/connecting or waiting for AI response
class ChatLoading extends ChatState {
  const ChatLoading(super.messages, super.chatRooms, super.currentChatId,
      [super.statusMessage = 'Loading...']);
}

// State when connection is established and ready for the current chat
class ChatConnected extends ChatState {
  const ChatConnected(super.messages, super.chatRooms, super.currentChatId,
      [super.statusMessage = 'Connected']);
}

// State for errors
class ChatError extends ChatState {
  final String error;

  const ChatError(
      super.messages, super.chatRooms, super.currentChatId, this.error,
      [super.statusMessage = 'Error']);

  @override
  List<Object?> get props =>
      [currentChatId, messages, error, statusMessage, chatRooms];
}
