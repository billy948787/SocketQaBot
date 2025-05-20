import 'package:speech_to_text/speech_to_text.dart';
import 'package:flutter/foundation.dart';
import 'dart:developer' as dev;

class STTService {
  final SpeechToText _speechToText = SpeechToText();
  bool _isInitialized = false;
  bool _isListening = false;
  String _lastRecognizedWords = '';
  VoidCallback? _onStateChanged;

  Future<bool> initialize() async {
    if (!_isInitialized) {
      _isInitialized = await _speechToText.initialize(
        onError: (error) {
          if (kDebugMode) {
            dev.log('STT錯誤: $error');
          }
        },
        onStatus: (status) {
          if (kDebugMode) {
            dev.log('STT狀態: $status');
          }

          // 當語音辨識結束時
          if (status == 'done' || status == 'notListening') {
            _isListening = false;
            if (_onStateChanged != null) _onStateChanged!();
          }
        },
      );
    }
    return _isInitialized;
  }

  set onStateChanged(VoidCallback? callback) {
    _onStateChanged = callback;
  }

  Future<void> startListening(
      {required Function(String) onResult, Function? onFinished}) async {
    if (!_isInitialized) {
      bool initialized = await initialize();
      if (!initialized) {
        if (kDebugMode) {
          dev.log('無法初始化語音辨識');
        }
        return;
      }
    }

    if (!_speechToText.isAvailable) {
      if (kDebugMode) {
        dev.log('語音辨識不可用');
      }
      return;
    }

    _lastRecognizedWords = '';
    _isListening = true;
    if (_onStateChanged != null) _onStateChanged!();

    await _speechToText.listen(
      onResult: (result) {
        _lastRecognizedWords = result.recognizedWords;
        dev.log('辨識結果: $_lastRecognizedWords');
        onResult(_lastRecognizedWords);
      },
      localeId: 'zh_TW', // 繁體中文
      listenMode: ListenMode.confirmation,
      cancelOnError: true,
    );
  }

  Future<void> stopListening({Function(String)? onFinalResult}) async {
    _isListening = false;
    if (_onStateChanged != null) _onStateChanged!();
    await _speechToText.stop();

    if (onFinalResult != null) {
      onFinalResult(_lastRecognizedWords);
    }
  }

  bool get isListening => _isListening;
  bool get isAvailable => _speechToText.isAvailable;
  String get lastRecognizedWords => _lastRecognizedWords;

  void dispose() {
    _speechToText.cancel();
    _isListening = false;
  }
}
