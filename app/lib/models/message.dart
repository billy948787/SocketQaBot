import 'package:equatable/equatable.dart';

enum MessageType {
  user,
  ai,
}

class Message extends Equatable {
  final String id;
  final MessageType type;
  final String content;
  final DateTime timestamp;

  const Message({
    required this.id,
    required this.type,
    required this.content,
    required this.timestamp,
  });

  @override
  List<Object> get props => [id, type, content, timestamp];

  // 將字串訊息轉換為 Message 物件
  factory Message.fromString(String messageString) {
    final timestamp = DateTime.now();
    final id = timestamp.millisecondsSinceEpoch.toString();
    
    if (messageString.startsWith('You: ')) {
      return Message(
        id: id,
        type: MessageType.user,
        content: messageString.substring(5), // 移除 "You: " 前綴
        timestamp: timestamp,
      );
    } else if (messageString.startsWith('AI: ')) {
      return Message(
        id: id,
        type: MessageType.ai,
        content: messageString.substring(4), // 移除 "AI: " 前綴
        timestamp: timestamp,
      );
    } else {
      // 預設為 AI 訊息（用於處理沒有前綴的舊資料）
      return Message(
        id: id,
        type: MessageType.ai,
        content: messageString,
        timestamp: timestamp,
      );
    }
  }

  // 將 Message 物件轉換為字串（用於向後相容）
  String toStringFormat() {
    switch (type) {
      case MessageType.user:
        return 'You: $content';
      case MessageType.ai:
        return 'AI: $content';
    }
  }

  // 創建用戶訊息
  factory Message.user({
    required String content,
    String? id,
    DateTime? timestamp,
  }) {
    final now = timestamp ?? DateTime.now();
    return Message(
      id: id ?? now.millisecondsSinceEpoch.toString(),
      type: MessageType.user,
      content: content,
      timestamp: now,
    );
  }

  // 創建 AI 訊息
  factory Message.ai({
    required String content,
    String? id,
    DateTime? timestamp,
  }) {
    final now = timestamp ?? DateTime.now();
    return Message(
      id: id ?? now.millisecondsSinceEpoch.toString(),
      type: MessageType.ai,
      content: content,
      timestamp: now,
    );
  }

  // 複製訊息並修改內容（用於重新生成）
  Message copyWith({
    String? id,
    MessageType? type,
    String? content,
    DateTime? timestamp,
  }) {
    return Message(
      id: id ?? this.id,
      type: type ?? this.type,
      content: content ?? this.content,
      timestamp: timestamp ?? this.timestamp,
    );
  }

  // 將 Message 轉換為 JSON
  Map<String, dynamic> toJson() {
    return {
      'id': id,
      'type': type.name,
      'content': content,
      'timestamp': timestamp.millisecondsSinceEpoch,
    };
  }

  // 從 JSON 創建 Message
  factory Message.fromJson(Map<String, dynamic> json) {
    return Message(
      id: json['id'] as String,
      type: MessageType.values.firstWhere(
        (e) => e.name == json['type'],
        orElse: () => MessageType.ai,
      ),
      content: json['content'] as String,
      timestamp: DateTime.fromMillisecondsSinceEpoch(json['timestamp'] as int),
    );
  }
}