import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';

/// Apple Watch (WatchConnectivity) + rezerva pro Wear OS Data Layer.
///
/// Inbound příkazy z hodinek: [ensureWatchInboundBinding] v `watch_command_binding.dart`.
/// Viz `context/WATCH_AND_LIVE_ACTIVITIES_PLAN.md`.
class WatchConnectivityService {
  WatchConnectivityService();

  static const _channel = MethodChannel('czechmate/watch');

  Future<bool> get isWatchCompanionSupported async {
    if (kIsWeb) return false;
    try {
      final v = await _channel.invokeMethod<bool>('isSupported');
      return v ?? false;
    } catch (_) {
      return false;
    }
  }

  Future<bool> get isPairedReachable async {
    try {
      final v = await _channel.invokeMethod<bool>('isReachable');
      return v ?? false;
    } catch (_) {
      return false;
    }
  }

  /// Odeslání příkazu na hodinky / příjem z hodinek — rozšíří se v nativní vrstvě.
  Future<void> sendGameMirror(Map<String, dynamic> payload) async {
    try {
      await _channel.invokeMethod<void>('mirrorGameState', payload);
    } catch (_) {}
  }
}
