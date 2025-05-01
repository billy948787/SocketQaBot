part of 'chat_bloc.dart';

abstract class ChatEvent extends Equatable {
  const ChatEvent();

  @override
  List<Object> get props => [];
}

// Event for sending a message
class SendMessageEvent extends ChatEvent {
  final String message;
  final String apiKey; // Add apiKey field

  const SendMessageEvent(this.message, this.apiKey); // Update constructor

  @override
  List<Object> get props => [message, apiKey]; // Add apiKey to props
}

// Event for receiving a message (can be triggered internally by the BLoC)
class ReceiveMessageEvent extends ChatEvent {
  final String message;

  const ReceiveMessageEvent(this.message);

  @override
  List<Object> get props => [message];
}

// Event to initialize connection
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
  final String apiKey;
  final String serverIp;
  final int serverPort;

  const SettingsLoadedEvent({
    required this.apiKey,
    required this.serverIp,
    required this.serverPort,
  });

  @override
  List<Object> get props => [apiKey, serverIp, serverPort];
}
