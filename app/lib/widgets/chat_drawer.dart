import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart';
import 'package:qabot/bloc/chat_bloc.dart';
import 'package:qabot/services/chat_storage_service.dart';

class ChatDrawer extends StatefulWidget {
  const ChatDrawer({super.key});

  @override
  State<ChatDrawer> createState() => _ChatDrawerState();
}

class _ChatDrawerState extends State<ChatDrawer> {
  final TextEditingController _newChatNameController = TextEditingController();
  final TextEditingController _renameChatController = TextEditingController();

  @override
  void dispose() {
    _newChatNameController.dispose();
    _renameChatController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Drawer(
      child: BlocBuilder<ChatBloc, ChatState>(
        builder: (context, state) {
          return Column(
            children: [
              // Drawer header with title and create chat button
              DrawerHeader(
                decoration: BoxDecoration(
                  color: Theme.of(context).primaryColor,
                ),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Row(
                      mainAxisAlignment: MainAxisAlignment.spaceBetween,
                      children: [
                        const Text(
                          '聊天室列表',
                          style: TextStyle(
                            color: Colors.white,
                            fontSize: 24,
                          ),
                        ),
                        IconButton(
                          icon: const Icon(Icons.add, color: Colors.white),
                          onPressed: () => _showCreateChatDialog(context),
                          tooltip: '建立新聊天室',
                        ),
                      ],
                    ),
                    const Spacer(),
                    Text(
                      '${state.chatRooms.length} 聊天室',
                      style: const TextStyle(color: Colors.white70),
                    ),
                  ],
                ),
              ),
              // Chat rooms list
              Expanded(
                child: state.chatRooms.isEmpty
                    ? const Center(
                        child: Text('沒有聊天室，請點擊右上角的 + 新增'),
                      )
                    : ListView.builder(
                        itemCount: state.chatRooms.length,
                        itemBuilder: (context, index) {
                          final ChatRoomInfo chatRoom = state.chatRooms[index];
                          final bool isSelected =
                              state.currentChatId == chatRoom.id;

                          return ListTile(
                            title: Text(
                              chatRoom.name,
                              style: TextStyle(
                                fontWeight: isSelected
                                    ? FontWeight.bold
                                    : FontWeight.normal,
                              ),
                            ),
                            subtitle: Text(
                              '建立於 ${_formatDate(chatRoom.createdAt)}',
                              style: TextStyle(
                                fontSize: 12,
                                color: Theme.of(context)
                                    .textTheme
                                    .bodySmall
                                    ?.color,
                              ),
                            ),
                            leading: CircleAvatar(
                              backgroundColor: isSelected
                                  ? Theme.of(context).primaryColor
                                  : Theme.of(context).disabledColor,
                              child: Text(
                                chatRoom.name.substring(0, 1).toUpperCase(),
                                style: TextStyle(
                                  color: isSelected
                                      ? Theme.of(context)
                                          .primaryTextTheme
                                          .bodyLarge
                                          ?.color
                                      : Colors.white,
                                ),
                              ),
                            ),
                            selected: isSelected,
                            onTap: () {
                              // Select this chat room
                              if (!isSelected) {
                                context.read<ChatBloc>().add(
                                      SelectChatRoomEvent(chatRoom.id),
                                    );
                                // Close drawer on mobile
                                if (Scaffold.of(context).isDrawerOpen) {
                                  Navigator.of(context).pop();
                                }
                              }
                            },
                            trailing: PopupMenuButton<String>(
                              icon: const Icon(Icons.more_vert),
                              onSelected: (value) {
                                if (value == 'rename') {
                                  _showRenameChatDialog(context, chatRoom);
                                } else if (value == 'delete') {
                                  _showDeleteChatConfirmDialog(
                                      context, chatRoom);
                                } else if (value == 'clear') {
                                  _showClearChatHistoryConfirmDialog(
                                      context, chatRoom);
                                }
                              },
                              itemBuilder: (BuildContext context) => [
                                const PopupMenuItem<String>(
                                  value: 'rename',
                                  child: Text('重命名'),
                                ),
                                const PopupMenuItem<String>(
                                  value: 'clear',
                                  child: Text('清除聊天記錄'),
                                ),
                                const PopupMenuItem<String>(
                                  value: 'delete',
                                  child: Text('刪除聊天室'),
                                ),
                              ],
                            ),
                          );
                        },
                      ),
              ),
            ],
          );
        },
      ),
    );
  }

  String _formatDate(DateTime date) {
    return '${date.year}/${date.month}/${date.day}';
  }

  // Dialog to create a new chat room
  Future<void> _showCreateChatDialog(BuildContext context) async {
    _newChatNameController.clear();
    return showDialog<void>(
      context: context,
      builder: (BuildContext dialogContext) {
        return AlertDialog(
          title: const Text('建立新聊天室'),
          content: TextField(
            controller: _newChatNameController,
            autofocus: true,
            decoration: const InputDecoration(
              labelText: '聊天室名稱',
              hintText: '輸入聊天室名稱',
            ),
          ),
          actions: <Widget>[
            TextButton(
              child: const Text('取消'),
              onPressed: () {
                Navigator.of(dialogContext).pop();
              },
            ),
            TextButton(
              child: const Text('建立'),
              onPressed: () {
                if (_newChatNameController.text.trim().isNotEmpty) {
                  context.read<ChatBloc>().add(
                        CreateChatRoomEvent(
                            name: _newChatNameController.text.trim()),
                      );
                  Navigator.of(dialogContext).pop();
                }
              },
            ),
          ],
        );
      },
    );
  }

  // Dialog to rename an existing chat room
  Future<void> _showRenameChatDialog(
      BuildContext context, ChatRoomInfo chatRoom) async {
    _renameChatController.text = chatRoom.name;
    return showDialog<void>(
      context: context,
      builder: (BuildContext dialogContext) {
        return AlertDialog(
          title: const Text('重命名聊天室'),
          content: TextField(
            controller: _renameChatController,
            autofocus: true,
            decoration: const InputDecoration(
              labelText: '新名稱',
              hintText: '輸入新的聊天室名稱',
            ),
          ),
          actions: <Widget>[
            TextButton(
              child: const Text('取消'),
              onPressed: () {
                Navigator.of(dialogContext).pop();
              },
            ),
            TextButton(
              child: const Text('更新'),
              onPressed: () {
                if (_renameChatController.text.trim().isNotEmpty) {
                  context.read<ChatBloc>().add(
                        RenameChatRoomEvent(
                          chatRoom.id,
                          _renameChatController.text.trim(),
                        ),
                      );
                  Navigator.of(dialogContext).pop();
                }
              },
            ),
          ],
        );
      },
    );
  }

  // Dialog to confirm deletion of a chat room
  Future<void> _showDeleteChatConfirmDialog(
      BuildContext context, ChatRoomInfo chatRoom) async {
    return showDialog<void>(
      context: context,
      builder: (BuildContext dialogContext) {
        return AlertDialog(
          title: const Text('刪除聊天室'),
          content: Text('確定要刪除 "${chatRoom.name}" 聊天室嗎？這個操作無法撤銷。'),
          actions: <Widget>[
            TextButton(
              child: const Text('取消'),
              onPressed: () {
                Navigator.of(dialogContext).pop();
              },
            ),
            TextButton(
              child: const Text('刪除'),
              onPressed: () {
                context.read<ChatBloc>().add(DeleteChatRoomEvent(chatRoom.id));
                Navigator.of(dialogContext).pop();
              },
            ),
          ],
        );
      },
    );
  }

  // Dialog to confirm clearing chat history
  Future<void> _showClearChatHistoryConfirmDialog(
      BuildContext context, ChatRoomInfo chatRoom) async {
    return showDialog<void>(
      context: context,
      builder: (BuildContext dialogContext) {
        return AlertDialog(
          title: const Text('清除聊天記錄'),
          content: Text('確定要清除 "${chatRoom.name}" 聊天室的所有對話記錄嗎？這個操作無法撤銷。'),
          actions: <Widget>[
            TextButton(
              child: const Text('取消'),
              onPressed: () {
                Navigator.of(dialogContext).pop();
              },
            ),
            TextButton(
              child: const Text('清除'),
              onPressed: () {
                context
                    .read<ChatBloc>()
                    .add(ClearChatHistoryEvent(chatRoom.id));
                Navigator.of(dialogContext).pop();
              },
            ),
          ],
        );
      },
    );
  }
}
