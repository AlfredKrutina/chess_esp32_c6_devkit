import 'dart:io';

import 'package:flutter/foundation.dart';
import 'package:home_widget/home_widget.dart';

/// Domovský widget (iOS / Android) — data přes `home_widget`; viz nativní `ChessBoardHomeWidget` / `ChessBoardHomeWidgetProvider`.
class BoardHomeWidgetSync {
  BoardHomeWidgetSync();

  /// Musí odpovídat App Group na iOS + entitlements u Runner a ChessLiveActivityExtension.
  static const appGroupId = 'group.com.example.flutterCzechmate.widget';

  static const _androidProvider =
      'com.example.flutter_czechmate.ChessBoardHomeWidgetProvider';

  static const _iosKind = 'ChessBoardHomeWidget';

  bool _initialized = false;
  DateTime? _lastPush;
  String? _lastSig;

  Future<void> ensureInitialized() async {
    if (kIsWeb || !(Platform.isIOS || Platform.isAndroid)) return;
    if (_initialized) return;
    if (Platform.isIOS) {
      await HomeWidget.setAppGroupId(appGroupId);
    }
    _initialized = true;
  }

  /// Stejný slovník jako [LiveActivityService.extensionPayload].
  Future<void> syncPayload(Map<String, dynamic> payload) async {
    if (kIsWeb || !(Platform.isIOS || Platform.isAndroid)) return;
    await ensureInitialized();

    final sig = _signature(payload);
    final now = DateTime.now();
    if (_lastSig == sig &&
        _lastPush != null &&
        now.difference(_lastPush!) < const Duration(milliseconds: 1200)) {
      return;
    }
    _lastSig = sig;
    _lastPush = now;

    final phase = payload['phase']?.toString() ?? 'no_game';

    await HomeWidget.saveWidgetData<String>('cz_phase', phase);
    await HomeWidget.saveWidgetData<String>(
      'cz_transport',
      payload['transport']?.toString() ?? '',
    );

    Future<void> saveOptInt(String key, dynamic v) async {
      if (v == null) {
        await HomeWidget.saveWidgetData<int>(key, null);
        return;
      }
      final n = v is int ? v : (v as num).toInt();
      await HomeWidget.saveWidgetData<int>(key, n);
    }

    Future<void> saveOptBool(String key, dynamic v) async {
      if (v == null) {
        await HomeWidget.saveWidgetData<bool>(key, null);
        return;
      }
      await HomeWidget.saveWidgetData<bool>(key, v == true);
    }

    if (phase == 'clock') {
      await saveOptInt('cz_white_ms', payload['whiteTimeMs']);
      await saveOptInt('cz_black_ms', payload['blackTimeMs']);
      await saveOptBool('cz_white_turn', payload['isWhiteTurn']);
      await saveOptBool('cz_timer_running', payload['timerRunning']);
      await saveOptBool('cz_game_paused', payload['gamePaused']);
      await saveOptInt('cz_total_moves', payload['totalMoves']);
      await HomeWidget.saveWidgetData<String>(
        'cz_tc_label',
        payload['timeControlLabel']?.toString() ?? '',
      );
      await saveOptBool('cz_game_finished', payload['gameFinished']);
    } else if (phase == 'no_timer') {
      await saveOptInt('cz_white_ms', null);
      await saveOptInt('cz_black_ms', null);
      await saveOptBool('cz_white_turn', null);
      await saveOptBool('cz_timer_running', null);
      await saveOptBool('cz_game_paused', payload['gamePaused']);
      await saveOptInt('cz_total_moves', payload['totalMoves']);
      await HomeWidget.saveWidgetData<String>('cz_tc_label', '');
      await saveOptBool('cz_game_finished', payload['gameFinished']);
    } else {
      await saveOptInt('cz_white_ms', null);
      await saveOptInt('cz_black_ms', null);
      await saveOptBool('cz_white_turn', null);
      await saveOptBool('cz_timer_running', null);
      await saveOptBool('cz_game_paused', null);
      await saveOptInt('cz_total_moves', null);
      await HomeWidget.saveWidgetData<String>('cz_tc_label', '');
      await saveOptBool('cz_game_finished', null);
    }

    await HomeWidget.updateWidget(
      qualifiedAndroidName: _androidProvider,
      iOSName: _iosKind,
    );
  }

  String _signature(Map<String, dynamic> m) =>
      m.entries.map((e) => '${e.key}=${e.value}').join('|');
}
