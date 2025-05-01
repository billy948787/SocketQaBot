part of 'chat_bloc.dart';

abstract class ChatState extends Equatable {
  final List<String> messages;
  final String
      statusMessage; // Optional status message (e.g., Connecting, Disconnected, AI is thinking...)
  final String apiKey; // Add apiKey field

  const ChatState(this.messages, this.apiKey, [this.statusMessage = '']);

  @override
  List<Object> get props => [messages, apiKey, statusMessage]; // Add apiKey to props
}

// Initial state
class ChatInitial extends ChatState {
  // Corrected constructor: Pass empty list for messages and optional status message
  const ChatInitial([String statusMessage = '', String apiKey = '']) // Add apiKey parameter
      : super(const [], apiKey, statusMessage); // Pass apiKey to super
}

// State when loading/connecting or waiting for AI response
class ChatLoading extends ChatState {
  const ChatLoading(super.messages, super.apiKey, [super.statusMessage = 'Loading...']); // Pass apiKey to super
}

// State when connection is established and ready
class ChatConnected extends ChatState {
  const ChatConnected(super.messages, super.apiKey, [super.statusMessage = 'Connected']); // Pass apiKey to super
}

// State for errors
class ChatError extends ChatState {
  final String error;

  const ChatError(super.messages, super.apiKey, this.error, [super.statusMessage = 'Error']); // Pass apiKey to super

  @override
  List<Object> get props => [messages, apiKey, error, statusMessage]; // Add apiKey to props
}
