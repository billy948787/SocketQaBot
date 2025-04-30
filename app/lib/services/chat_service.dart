import 'dart:async';
import 'dart:convert';
import 'dart:io'; // Import dart:io for Socket
import 'package:flutter/foundation.dart'; // For kDebugMode
import 'dart:developer' as dev;

class ChatService {
  // TODO: Replace with your actual middleware server IP/hostname and port
  final String _serverHost = '10.201.14.97'; // Example: '192.168.1.100' or 'your-server.com'
  final int _serverPort = 38763; // Example: 8080

  Socket? _socket;
  StreamController<String>? _messageStreamController;
  String _buffer = ''; // To accumulate raw data chunks
  bool _headersSkipped = false; // Flag to track if HTTP headers have been skipped
  String _currentPartialJson = ''; // Buffer for potentially incomplete JSON data lines

  // Helper method to handle disconnection cleanup
  void _handleDisconnect() {
    if (kDebugMode) {
      dev.log('Handling disconnection.');
    }
    _socket?.destroy();
    _socket = null;
    _messageStreamController?.close();
    _messageStreamController = null;
    _buffer = '';
    _headersSkipped = false; // Reset flag
    _currentPartialJson = ''; // Reset partial JSON buffer
  }

  // Method to close the Socket connection
  void closeConnection() {
    if (_socket != null) {
      if (kDebugMode) {
        dev.log('Closing socket connection.');
      }
      _handleDisconnect(); // Use helper to ensure proper cleanup
    } else {
      // Ensure stream controller is closed even if socket was never connected or already null
      _messageStreamController?.close();
      _messageStreamController = null;
    }
    _buffer = '';
    _currentPartialJson = ''; // Reset partial JSON buffer on close
  }

  // Method to establish connection and return the stream for responses
  Stream<String> connectAndListen({required String host, required int port}) {
    // Close existing connection if any
    closeConnection();

    _messageStreamController = StreamController<String>.broadcast();
    _buffer = '';
    _currentPartialJson = ''; // Reset partial JSON buffer
    _headersSkipped = false; // Reset flag on new connection attempt

    if (kDebugMode) {
      dev.log('Attempting to connect to $host:$port...');
    }

    Socket.connect(host, port,
            timeout: const Duration(seconds: 10))
        .then((socket) {
      if (kDebugMode) {
        dev.log(
            'Connected to: ${socket.remoteAddress.address}:${socket.remotePort}');
      }
      _socket = socket;

      _socket!.listen(
        (List<int> data) {
          final receivedString = utf8.decode(data);
          if (kDebugMode) {
            dev.log('Received raw data chunk (visible whitespace): ${receivedString.replaceAll('\n', '\\n').replaceAll('\r', '\\r').replaceAll('\t', '\\t')}');
          }
          _buffer += receivedString;

          // --- Enhanced Parsing Logic ---

          // 1. Skip HTTP Headers if not already done
          if (!_headersSkipped) {
            int headerEndIndex = _buffer.indexOf('\r\n\r\n');
            if (headerEndIndex != -1) {
              _buffer = _buffer.substring(headerEndIndex + 4); // Skip headers + \r\n\r\n
              _headersSkipped = true;
              if (kDebugMode) {
                dev.log('Skipped HTTP headers. Remaining buffer start: "${_buffer.substring(0, (_buffer.length > 100 ? 100 : _buffer.length)).replaceAll('\n', '\\n').replaceAll('\r', '\\r').replaceAll('\t', '\\t')}..."');
              }
            } else {
              // Headers not fully received yet, wait for more data
              return;
            }
          }

          // 2. Process buffer line by line after headers are skipped
          // This handles chunked encoding implicitly by processing lines as they become complete
          while (_headersSkipped) {
            int lineEndIndex = _buffer.indexOf('\n');
            if (lineEndIndex == -1) {
              // No complete line found, wait for more data
              break;
            }

            // Extract the line (including potential \r)
            String line = _buffer.substring(0, lineEndIndex + 1);
            _buffer = _buffer.substring(lineEndIndex + 1); // Consume the line from buffer

            line = line.trim(); // Trim whitespace (\n, \r, spaces)

            if (kDebugMode) {
              // dev.log('Processing line from buffer: "${line.replaceAll('\n', '\\n').replaceAll('\r', '\\r').replaceAll('\t', '\\t')}"');
            }

            // Ignore empty lines and chunk size lines (heuristic)
            if (line.isEmpty || RegExp(r'^[0-9a-fA-F]+$').hasMatch(line)) {
              if (kDebugMode && line.isNotEmpty) {
                 dev.log('Ignoring chunk size line: "$line"');
              }
              continue;
            }

            // Process 'data:' lines
            if (line.startsWith('data:')) {
              String jsonDataPart = line.substring('data:'.length).trim();
              if (kDebugMode) {
                dev.log('Found data line content: "$jsonDataPart"');
              }

              // Append to partial JSON buffer
              _currentPartialJson += jsonDataPart;

              // Attempt to parse the accumulated JSON
              try {
                var jsonData = jsonDecode(_currentPartialJson);
                // If parsing succeeds, we have a complete JSON object
                if (kDebugMode) {
                  dev.log('Successfully parsed JSON: $_currentPartialJson');
                }

                // Extract text from Gemini structure
                if (jsonData is Map && jsonData.containsKey('candidates')) {
                  var candidates = jsonData['candidates'] as List?;
                  if (candidates != null && candidates.isNotEmpty) {
                    var content = candidates[0]['content'] as Map?;
                    if (content != null && content.containsKey('parts')) {
                      var parts = content['parts'] as List?;
                      if (parts != null && parts.isNotEmpty) {
                        var text = parts[0]['text'] as String?;
                        if (text != null && text.isNotEmpty) {
                          if (kDebugMode) {
                            dev.log('Extracted text: "$text"');
                          }
                          // Add extracted text to the stream
                          _messageStreamController?.add(text);
                        }
                      }
                    }
                  }
                }
                // Reset partial JSON buffer after successful parse
                _currentPartialJson = '';

              } catch (e) {
                // If JSON parsing fails, it might be incomplete. Keep accumulating.
                if (kDebugMode) {
                  // Only dev.log error if it's not an Unterminated string or similar incomplete errors
                  if (!e.toString().contains('Unterminated string') && !e.toString().contains('Unexpected character') && !e.toString().contains('Expected') ) {
                     dev.log('JSON parse error (likely incomplete): $e - Current partial JSON: "$_currentPartialJson"');
                  } else {
                     dev.log('JSON likely incomplete, continuing accumulation...');
                  }
                }
              }
            } else {
               if (kDebugMode) {
                 dev.log('Ignoring non-data line: "$line"');
               }
               // If we encounter a non-data line after starting to accumulate JSON,
               // it might indicate an error or unexpected format. Reset partial buffer.
               if (_currentPartialJson.isNotEmpty) {
                  if (kDebugMode) {
                     dev.log('Resetting partial JSON buffer due to non-data line.');
                  }
                 _currentPartialJson = '';
               }
            }
          }
          // --- End Enhanced Parsing Logic ---
        },
        onError: (error) {
          if (kDebugMode) {
            dev.log('Socket error: $error');
          }
          _messageStreamController?.addError('Socket error: $error');
          _handleDisconnect();
        },
        onDone: () {
          if (kDebugMode) {
            dev.log('Socket connection closed by server.');
          }
          _handleDisconnect();
        },
        cancelOnError: true,
      );

      // Handle potential connection errors during the connect phase
      // This catchError is for errors during the Socket.connect future
    }).catchError((error) {
      if (kDebugMode) {
        dev.log('Failed to connect: $error');
      }
      _messageStreamController?.addError('Connection failed: $error');
      _messageStreamController?.close(); // Close stream if connection fails
      _socket = null; // Ensure socket is null
      // Re-throw or handle as needed
      // throw Exception('Failed to connect to socket: $e');
    });

    return _messageStreamController!.stream;
  }

  // Method to send a message over the established connection
  void sendMessage({
    required String apiKey,
    required String message,
    String prompt = '使用繁體中文回答',
    String modelName = 'gemini-2.0-flash',
  }) {
    if (_socket == null ||
        _messageStreamController == null ||
        _messageStreamController!.isClosed) {
      if (kDebugMode) {
        dev.log('Socket not connected or stream closed. Cannot send message.');
      }
      throw Exception('Socket is not connected. Call connectAndListen first.');
    }

    final header = 'POST / HTTP/1.1\r\n'
        'Host: $_serverHost:$_serverPort\r\n'
        'Connection: keep-alive\r\n'
        'Accept: application/json\r\n'
        'Accept-Encoding: gzip, deflate, br\r\n'
        'Accept-Language: zh-TW,zh;q=0.9,en;q=0.8\r\n'
        'User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/\r\n';

    final contentLength = 'Content-Length: ${message.length}\r\n';
    const endOfHeaders = '\r\n';

    // Construct the payload

    final bodyJson = jsonEncode({
      'prompt': prompt,
      'model_name': modelName,
      'message': message,
      'api_key': apiKey,
    });

    final payload = '$header$contentLength$endOfHeaders$bodyJson';

    if (kDebugMode) {
      dev.log('Sending message via Socket: $payload');
    }
    // Add a newline delimiter, assuming the server expects it. Adjust if needed.
    _socket!.write('$payload\n');
    _socket!.flush(); // Ensure data is sent immediately
  }
}
