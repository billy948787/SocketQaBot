part of 'chat_bloc.dart';

abstract class ChatState extends Equatable {
  final List<String> messages;
  final String
      statusMessage; // Optional status message (e.g., Connecting, Disconnected, AI is thinking...)

  const ChatState(this.messages, [this.statusMessage = '']);

  @override
  List<Object> get props => [messages, statusMessage];
}

// Initial state
class ChatInitial extends ChatState {
  // Corrected constructor: Pass empty list for messages and optional status message
  const ChatInitial([String statusMessage = ''])
      : super(const [], statusMessage);
}

// State when loading/connecting or waiting for AI response
class ChatLoading extends ChatState {
  const ChatLoading(super.messages, [super.statusMessage = 'Loading...']);
}

// State when connection is established and ready
class ChatConnected extends ChatState {
  const ChatConnected(super.messages, [super.statusMessage = 'Connected']);
}

// State for errors
class ChatError extends ChatState {
  final String error;

  const ChatError(super.messages, this.error, [super.statusMessage = 'Error']);

  @override
  List<Object> get props => [messages, error, statusMessage];
}
