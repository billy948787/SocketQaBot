import 'package:flutter_tts/flutter_tts.dart';
import 'package:flutter/foundation.dart';
import 'dart:developer' as dev;

class TTSService {
  final FlutterTts _flutterTts = FlutterTts();
  bool _isPlaying = false;
  String? _currentText;
  VoidCallback? _listener;

  TTSService() {
    _initTTS();
  }

  void setListener(VoidCallback listener) {
    final oldListener = _listener;
    _listener = listener;

    // 如果有舊的 listener 重新呼叫他更新gui
    if (oldListener != null) {
      oldListener();
    }
  }

  VoidCallback? get listener => _listener;

  Future<void> _initTTS() async {
    await _flutterTts.setLanguage('zh-TW'); // 設定為繁體中文
    await _flutterTts.setSpeechRate(0.5); // 較慢的語速，範圍通常是 0.0-1.0
    await _flutterTts.setVolume(1.0); // 最大音量
    await _flutterTts.setPitch(1.0); // 正常音調

    _flutterTts.setCompletionHandler(() {
      _isPlaying = false;
      _currentText = null;
      if (_listener != null) {
        _listener!();
      }

      if (kDebugMode) {
        dev.log('TTS播放完成');
      }
    });

    _flutterTts.setErrorHandler((error) {
      _isPlaying = false;
      _currentText = null;

      if (_listener != null) {
        _listener!();
      }

      if (kDebugMode) {
        dev.log('TTS錯誤: $error');
      }
    });
  }

  Future<void> speak(String text) async {
    if (_isPlaying) {
      // 如果當前正在播放，先停止
      await stop();
    }

    _currentText = text;
    _isPlaying = true;
    if (_listener != null) {
      _listener!();
    }
    await _flutterTts.speak(text);
  }

  Future<void> stop() async {
    _isPlaying = false;
    if (_listener != null) {
      _listener!();
    }
    await _flutterTts.stop();
  }

  bool get isPlaying => _isPlaying;

  String? get currentText => _currentText;

  void dispose() {
    _flutterTts.stop();
    _flutterTts.awaitSpeakCompletion(false);
  }
}
