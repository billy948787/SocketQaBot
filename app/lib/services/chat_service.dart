import 'dart:async';
import 'dart:convert';
import 'dart:io'; // Import dart:io for Socket
import 'package:flutter/foundation.dart'; // For kDebugMode
import 'dart:developer' as dev;

class ChatService {
  Socket? _socket;
  StreamController<String>? _messageStreamController;
  String _buffer = ''; // To accumulate raw data chunks
  bool _headersSkipped =
      false; // Flag to track if HTTP headers have been skipped
  String _currentPartialJson =
      ''; // Buffer for potentially incomplete JSON data lines

  // Store details of the current connection
  String _currentConnectedHost = '';
  int _currentConnectedPort = 0;
  bool _isSecureConnection = false;

  // Public getters for connection status and details
  bool get isConnected => _socket != null && _messageStreamController != null;
  String get currentHost => _currentConnectedHost;
  int get currentPort => _currentConnectedPort;
  bool get isSecureConnection => _isSecureConnection;

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

    // Reset current connection details
    _currentConnectedHost = '';
    _currentConnectedPort = 0;
    _isSecureConnection = false;
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
      // Also ensure connection details are reset if only stream was open
      if (_currentConnectedHost.isNotEmpty) {
        _handleDisconnect();
      }
    }
    // These are reset in _handleDisconnect, but good to be explicit if called when _socket is null
    _buffer = '';
    _currentPartialJson = '';
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

    Future<Socket> socketFuture;

    if (port == 443) {
      // Assume HTTPS for port 443
      _isSecureConnection = true;
      if (kDebugMode) {
        dev.log('Attempting SecureSocket connection to $host:$port');
      }
      // For SecureSocket, the 'host' parameter is used for SNI and certificate validation.
      // Default certificate validation will be used.
      socketFuture = SecureSocket.connect(host, port,
          timeout: const Duration(seconds: 10));
    } else {
      _isSecureConnection = false;
      if (kDebugMode) {
        dev.log('Attempting plain Socket connection to $host:$port');
      }
      socketFuture =
          Socket.connect(host, port, timeout: const Duration(seconds: 10));
    }

    socketFuture.then((socket) {
      if (kDebugMode) {
        dev.log(
            'Successfully connected to $host:$port ${(_isSecureConnection ? "(Secure)" : "(Plain)")}');
      }
      _socket = socket;
      _currentConnectedHost = host; // Store the actual host used
      _currentConnectedPort = port; // Store the actual port used

      _socket!.listen(
        (List<int> data) {
          final receivedString = utf8.decode(data);
          if (kDebugMode) {
            dev.log(
                'Received raw data chunk (visible whitespace): ${receivedString.replaceAll('\n', '\\n').replaceAll('\r', '\\r').replaceAll('\t', '\\t')}');
          }
          _buffer += receivedString;

          // --- Enhanced Parsing Logic ---

          // 1. Skip HTTP Headers if not already done
          if (!_headersSkipped) {
            int headerEndIndex = _buffer.indexOf('\r\n\r\n');
            if (headerEndIndex != -1) {
              // 解析 HTTP 狀態
              String headerSection = _buffer.substring(0, headerEndIndex);
              List<String> headerLines = headerSection.split('\r\n');
              if (headerLines.isNotEmpty) {
                final statusLine = headerLines[0];
                final match = RegExp(r'HTTP/\d+\.\d+\s+(\d+)\s*(.*)')
                    .firstMatch(statusLine);
                if (match != null) {
                  final statusCode = int.tryParse(match.group(1)!);
                  final reasonPhrase = match.group(2);
                  if (statusCode == null || statusCode != 200) {
                    _messageStreamController
                        ?.addError('HTTP error: $statusCode $reasonPhrase');
                    _handleDisconnect();
                    return;
                  }
                }
              }
              _buffer =
                  _buffer.substring(headerEndIndex + 4); // Skip headers + CRLF
              _headersSkipped = true;
              if (kDebugMode) {
                dev.log(
                    'Skipped HTTP headers. Remaining buffer start: "${_buffer.substring(0, (_buffer.length > 100 ? 100 : _buffer.length)).replaceAll('\n', '\\n').replaceAll('\r', '\\r').replaceAll('\t', '\\t')}..."');
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
            _buffer = _buffer
                .substring(lineEndIndex + 1); // Consume the line from buffer

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
              final jsonDataPart = line.substring('data:'.length).trim();
              _currentPartialJson += jsonDataPart;
              try {
                final jsonData = jsonDecode(_currentPartialJson);
                if (jsonData is Map && jsonData.containsKey('error')) {
                  final errMsg =
                      jsonData['error']?.toString() ?? 'Unknown error';
                  _messageStreamController?.addError(errMsg);
                } else if (jsonData is Map &&
                    jsonData.containsKey('candidates')) {
                  final candidates = jsonData['candidates'] as List?;
                  if (candidates != null && candidates.isNotEmpty) {
                    final content = candidates[0]['content'] as Map?;
                    final parts = content?['parts'] as List?;
                    final text = parts != null && parts.isNotEmpty
                        ? parts[0]['text'] as String?
                        : null;
                    if (text != null && text.isNotEmpty) {
                      _messageStreamController?.add(text);
                    }
                  }
                }
                // Reset partial JSON buffer after successful parse
                _currentPartialJson = '';
              } catch (e) {
                if (kDebugMode) {
                  // JSON 可能不完整，繼續累積
                  if (!e.toString().contains('Unterminated string') &&
                      !e.toString().contains('Unexpected character') &&
                      !e.toString().contains('Expected')) {
                    dev.log(
                        'JSON parse error: $e - buffer:"$_currentPartialJson"');
                  }
                }
              }
            } else {
              if (kDebugMode) {
                dev.log('Ignoring non-data line: "$line"');
              }
              // Reset buffer on unexpected lines
              if (_currentPartialJson.isNotEmpty) {
                if (kDebugMode) {
                  dev.log(
                      'Resetting partial JSON buffer due to non-data line.');
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
        dev.log('Failed to connect to $host:$port: $error');
      }
      _messageStreamController?.addError('Connection failed: $error');
      // Ensure full cleanup, including resetting connection state variables
      _handleDisconnect();
    });

    return _messageStreamController!.stream;
  }

  // Method to send a message over the established connection
  Future<void> sendMessage({
    // Change return type to Future<void>
    required List<Map<String, String>> context,
    required String message,
    int? contextLimit, // 新增參數，用於限制上下文訊息數量
    String prompt = '使用繁體中文回答',
    String modelName = 'gemini-2.0-flash',
  }) async {
    // Add async keyword
    if (_socket == null || _currentConnectedHost.isEmpty) {
      if (kDebugMode) {
        dev.log(
            'Socket not connected or host details missing. Cannot send message.');
      }
      throw Exception('Socket is not connected. Call connectAndListen first.');
    }

    String hostHeaderValue = _currentConnectedHost;
    // Append port to Host header if it's not the default for the scheme
    // HTTP default: 80, HTTPS default: 443
    if (!((_currentConnectedPort == 80 && !_isSecureConnection) ||
        (_currentConnectedPort == 443 && _isSecureConnection))) {
      hostHeaderValue = '$_currentConnectedHost:$_currentConnectedPort';
    }

    var contextToSend = [];

    int contextCount = 0;
    if (contextLimit == null) {
      contextLimit = 10; // Default context limit
    } else {
      contextLimit = contextLimit == 0 ? context.length : contextLimit;
    }

    for (int i = context.length - 1; i >= 0; i--) {
      if (contextCount >= contextLimit) {
        break;
      }
      contextToSend.add(context[i]);
      contextCount++;
    }

    contextToSend = contextToSend.reversed.toList();

    final Map<String, dynamic> bodyData = {
      'prompt': prompt,
      'context': contextToSend,
      'model_name': modelName,
      'message': message,
    };
    final String bodyJsonString = jsonEncode(bodyData);
    final List<int> bodyBytes = utf8
        .encode(bodyJsonString); // Encode to get byte length for Content-Length

    // Construct headers
    // Ensure the path (e.g., "/") is correct for your server endpoint
    final String requestHeader = 'POST / HTTP/1.1'
        '\r\nHost: $hostHeaderValue'
        '\r\nContent-Type: application/json'
        '\r\nContent-Length: ${bodyBytes.length}'
        '\r\nConnection: keep-alive'
        '\r\nAccept: application/json'
        '\r\nUser-Agent: Mozilla/1.0';
    final String headersString = requestHeader;
    // The full request includes headers, an empty line (CRLF), and then the body.
    final String fullRequest = '$headersString\r\n\r\n$bodyJsonString';

    if (kDebugMode) {
      dev.log('--- Start Request ---');
      dev.log(fullRequest);
      dev.log('--- End Request ---');
    }

    _socket!.write(fullRequest);
    await _socket!.flush(); // Ensure data is sent immediately
  }
}
