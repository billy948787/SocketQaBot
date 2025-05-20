import 'dart:convert';
import 'dart:io';
import 'package:path_provider/path_provider.dart';
import 'package:flutter/foundation.dart';
import 'package:uuid/uuid.dart'; // For generating unique chat IDs

class ChatStorageService {
  static const String _chatHistoryDir = 'chat_histories';
  static const String _chatRoomsInfoFile = 'chat_rooms.json';
  final Uuid _uuid = const Uuid();

  Future<String> _getChatHistoryDirectoryPath() async {
    final directory = await getApplicationDocumentsDirectory();
    final chatDir = Directory('${directory.path}/$_chatHistoryDir');
    if (!await chatDir.exists()) {
      await chatDir.create(recursive: true);
    }
    return chatDir.path;
  }

  Future<String> _getChatRoomFilePath(String chatId) async {
    final dirPath = await _getChatHistoryDirectoryPath();
    return '$dirPath/$chatId.json';
  }

  Future<String> _getChatRoomsInfoFilePath() async {
    final directory = await getApplicationDocumentsDirectory();
    return '${directory.path}/$_chatRoomsInfoFile';
  }

  Future<List<ChatRoomInfo>> loadChatRooms() async {
    try {
      final filePath = await _getChatRoomsInfoFilePath();
      final file = File(filePath);
      if (await file.exists()) {
        final content = await file.readAsString();
        final List<dynamic> jsonData = jsonDecode(content);
        return jsonData.map((data) => ChatRoomInfo.fromJson(data)).toList();
      }
    } catch (e) {
      if (kDebugMode) {
        print('Error loading chat rooms: $e');
      }
    }
    return [];
  }

  Future<void> _saveChatRooms(List<ChatRoomInfo> chatRooms) async {
    try {
      final filePath = await _getChatRoomsInfoFilePath();
      final file = File(filePath);
      final jsonData = jsonEncode(chatRooms.map((cr) => cr.toJson()).toList());
      await file.writeAsString(jsonData);
    } catch (e) {
      if (kDebugMode) {
        print('Error saving chat rooms: $e');
      }
    }
  }

  Future<ChatRoomInfo> createChatRoom({String? name}) async {
    final chatRooms = await loadChatRooms();
    final newChatId = _uuid.v4();
    final newRoomName = name ?? 'Chat ${chatRooms.length + 1}';
    final newChatRoom = ChatRoomInfo(
        id: newChatId, name: newRoomName, createdAt: DateTime.now());
    chatRooms.add(newChatRoom);
    await _saveChatRooms(chatRooms);
    // Create an empty history file for the new chat room
    await saveChatHistory(newChatId, []);
    return newChatRoom;
  }

  Future<void> deleteChatRoom(String chatId) async {
    var chatRooms = await loadChatRooms();
    chatRooms.removeWhere((room) => room.id == chatId);
    await _saveChatRooms(chatRooms);

    try {
      final filePath = await _getChatRoomFilePath(chatId);
      final file = File(filePath);
      if (await file.exists()) {
        await file.delete();
      }
    } catch (e) {
      if (kDebugMode) {
        print('Error deleting chat history file for $chatId: $e');
      }
    }
  }

  Future<void> renameChatRoom(String chatId, String newName) async {
    var chatRooms = await loadChatRooms();
    final roomIndex = chatRooms.indexWhere((room) => room.id == chatId);
    if (roomIndex != -1) {
      chatRooms[roomIndex] = chatRooms[roomIndex].copyWith(name: newName);
      await _saveChatRooms(chatRooms);
    }
  }

  Future<List<String>> loadChatHistory(String chatId) async {
    try {
      final filePath = await _getChatRoomFilePath(chatId);
      final file = File(filePath);
      if (await file.exists()) {
        final content = await file.readAsString();
        final List<dynamic> jsonData = jsonDecode(content);
        return jsonData.cast<String>();
      }
    } catch (e) {
      if (kDebugMode) {
        print('Error loading chat history for $chatId: $e');
      }
    }
    return [];
  }

  Future<void> saveChatHistory(String chatId, List<String> messages) async {
    try {
      final filePath = await _getChatRoomFilePath(chatId);
      final file = File(filePath);
      final jsonData = jsonEncode(messages);
      await file.writeAsString(jsonData);
    } catch (e) {
      if (kDebugMode) {
        print('Error saving chat history for $chatId: $e');
      }
    }
  }

  Future<void> clearChatHistory(String chatId) async {
    try {
      final filePath = await _getChatRoomFilePath(chatId);
      final file = File(filePath);
      if (await file.exists()) {
        // Instead of deleting, just write an empty list to clear history
        await file.writeAsString(jsonEncode([]));
      }
    } catch (e) {
      if (kDebugMode) {
        print('Error clearing chat history for $chatId: $e');
      }
    }
  }
}

class ChatRoomInfo {
  final String id;
  final String name;
  final DateTime createdAt;
  DateTime? lastModifiedAt; // Optional: to track last message time

  ChatRoomInfo({
    required this.id,
    required this.name,
    required this.createdAt,
    this.lastModifiedAt,
  });

  ChatRoomInfo copyWith({
    String? id,
    String? name,
    DateTime? createdAt,
    DateTime? lastModifiedAt,
  }) {
    return ChatRoomInfo(
      id: id ?? this.id,
      name: name ?? this.name,
      createdAt: createdAt ?? this.createdAt,
      lastModifiedAt: lastModifiedAt ?? this.lastModifiedAt,
    );
  }

  Map<String, dynamic> toJson() {
    return {
      'id': id,
      'name': name,
      'createdAt': createdAt.toIso8601String(),
      'lastModifiedAt': lastModifiedAt?.toIso8601String(),
    };
  }

  factory ChatRoomInfo.fromJson(Map<String, dynamic> json) {
    return ChatRoomInfo(
      id: json['id'] as String,
      name: json['name'] as String,
      createdAt: DateTime.parse(json['createdAt'] as String),
      lastModifiedAt: json['lastModifiedAt'] != null
          ? DateTime.parse(json['lastModifiedAt'] as String)
          : null,
    );
  }
}
