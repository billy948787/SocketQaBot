import 'package:flutter/material.dart';
import '../services/tts_service.dart';

class TTSButton extends StatefulWidget {
  final String text;
  final TTSService ttsService;
  final Color? color;
  final double size;

  var isPlaying = false;

  TTSButton({
    Key? key,
    required this.text,
    required this.ttsService,
    this.color,
    this.size = 24.0,
  }) : super(key: key);

  @override
  State<TTSButton> createState() => _TTSButtonState();
}

class _TTSButtonState extends State<TTSButton> {
  late VoidCallback _listener;

  @override
  void initState() {
    super.initState();
    _listener = () {
      if (mounted) {
        setState(() {});
      }
    };
  }

  @override
  void dispose() {
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final isPlaying =
        widget.ttsService.isPlaying && widget.ttsService.listener == _listener;
    return IconButton(
      icon: Icon(
        isPlaying ? Icons.stop : Icons.volume_up,
        color: widget.color ?? Theme.of(context).primaryColor,
        size: widget.size,
      ),
      onPressed: () async {
        if (isPlaying) {
          await widget.ttsService.stop();
        } else {
          // 移除 "You: " 或 "AI: " 前綴
          String textToSpeak = widget.text;
          if (textToSpeak.startsWith('You: ')) {
            textToSpeak = textToSpeak.substring(5);
          } else if (textToSpeak.startsWith('AI: ')) {
            textToSpeak = textToSpeak.substring(4);
          }
          widget.ttsService.setListener(_listener);
          await widget.ttsService.speak(textToSpeak);
        }
      },
      tooltip: isPlaying ? '停止播放' : '播放語音',
    );
  }
}
