import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';

import '../../features/connection/board_session_state.dart';

/// iOS Live Activity (ActivityKit) + Android ongoing notification (stejný kanál z herní obrazovky).
///
/// Detail platform: `context/WATCH_AND_LIVE_ACTIVITIES_PLAN.md`.
class LiveActivityService {
  LiveActivityService();

  static const _channel = MethodChannel('czechmate/live_activity');

  DateTime? _lastNativePush;
  String? _lastSignature;

  /// Rychlý dotaz z UI / nastavení.
  Future<bool> isIosLiveActivityFrameworkSupported() async {
    if (kIsWeb || defaultTargetPlatform != TargetPlatform.iOS) return false;
    try {
      final v = await _channel.invokeMethod<bool>('isSupported');
      return v ?? false;
    } catch (_) {
      return false;
    }
  }

  Future<bool> iosLiveActivitiesAllowedByUser() async {
    if (kIsWeb || defaultTargetPlatform != TargetPlatform.iOS) return false;
    try {
      final v = await _channel.invokeMethod<bool>('areActivitiesEnabled');
      return v ?? false;
    } catch (_) {
      return false;
    }
  }

  /// Při změně session pošle payload na iOS (až bude Widget Extension, objeví se na Lock Screen).
  Future<void> syncFromBoardSession(
    BoardSessionState session, {
    required bool enabled,
  }) async {
    if (!enabled) {
      await endAll();
      return;
    }

    final payload = _buildPayload(session);
    final sig = _signature(payload);

    final now = DateTime.now();
    if (_lastSignature == sig &&
        _lastNativePush != null &&
        now.difference(_lastNativePush!) < const Duration(milliseconds: 900)) {
      return;
    }
    _lastSignature = sig;
    _lastNativePush = now;

    try {
      await _channel.invokeMethod<void>('syncChessClock', payload);
    } on PlatformException catch (e) {
      if (kDebugMode) {
        debugPrint('[staging][live_activity] error: $e');
      }
    } catch (e) {
      if (kDebugMode) debugPrint('[staging][live_activity] error: $e');
    }
  }

  Future<void> endAll() async {
    _lastSignature = null;
    try {
      await _channel.invokeMethod<void>('endAll');
    } catch (_) {}
  }

  /// Android ongoing notifikace — Pauza / Pokračovat z shade (BroadcastReceiver).
  Future<String?> consumePendingAndroidClockAction() async {
    if (kIsWeb || defaultTargetPlatform != TargetPlatform.android) return null;
    try {
      final v =
          await _channel.invokeMethod<String>('consumePendingClockAction');
      return v;
    } catch (_) {
      return null;
    }
  }

  /// Stejná mapa jako pro Live Activity — vhodná i pro WatchConnectivity mirror.
  Map<String, dynamic> extensionPayload(BoardSessionState session) =>
      _buildPayload(session);

  Map<String, dynamic> _buildPayload(BoardSessionState s) {
    final snap = s.snapshot;
    if (snap == null) {
      return {
        'phase': 'no_game',
        'transport': s.transport.name,
      };
    }

    final clock = snap.clock ?? s.timer;
    final status = snap.status;

    if (clock == null) {
      return {
        'phase': 'no_timer',
        'transport': s.transport.name,
        'gamePaused': false,
        'totalMoves': snap.history.moves.length,
        'gameFinished': status.isGameFinished,
      };
    }

    return {
      'phase': 'clock',
      'transport': s.transport.name,
      'whiteTimeMs': clock.whiteTimeMs,
      'blackTimeMs': clock.blackTimeMs,
      'isWhiteTurn': clock.isWhiteTurn,
      'timerRunning': clock.timerRunning,
      'gamePaused': clock.gamePaused,
      'timeExpired': clock.timeExpired,
      'totalMoves': clock.totalMoves,
      'timeControlLabel': clock.config.name,
      'gameFinished': status.isGameFinished,
    };
  }

  String _signature(Map<String, dynamic> m) {
    return m.entries.map((e) => '${e.key}=${e.value}').join('|');
  }
}
