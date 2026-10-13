import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../features/connection/board_session_notifier.dart';
import '../../features/game/game_screen.dart'
    show ConnectionTier, connectionTierOf;

const _watchChannel = MethodChannel('czechmate/watch');

bool _watchInboundBound = false;

/// Commands from Apple Watch (native → Flutter `onWatchCommand`).
///
/// Returns `true` only when the board accepted the command, so the watch can
/// tell the player whether the tap did anything.
void ensureWatchInboundBinding(WidgetRef ref) {
  if (_watchInboundBound) return;
  _watchInboundBound = true;
  _watchChannel.setMethodCallHandler((call) async {
    if (call.method != 'onWatchCommand') return false;
    final args = call.arguments as Map<dynamic, dynamic>?;
    final cmd = args?['cmd'] as String? ?? '';
    final session = ref.read(boardSessionNotifierProvider);
    final timer = session.snapshot?.clock ?? session.timer;
    final canToggleClock = connectionTierOf(session) == ConnectionTier.live &&
        timer != null &&
        timer.isTimeControlEnabled &&
        session.snapshot?.status.isGameFinished != true &&
        !timer.timeExpired;
    if (!canToggleClock) {
      if (kDebugMode) {
        debugPrint('[staging][watch] $cmd refused: clock not toggleable');
      }
      return false;
    }
    final notifier = ref.read(boardSessionNotifierProvider.notifier);
    try {
      switch (cmd) {
        case 'pause':
          if (!timer.gamePaused) await notifier.postTimerPause();
          return true;
        case 'resume':
          if (timer.gamePaused) await notifier.postTimerResume();
          return true;
      }
    } catch (e) {
      if (kDebugMode) debugPrint('[staging][watch] $cmd failed: $e');
    }
    return false;
  });
}
