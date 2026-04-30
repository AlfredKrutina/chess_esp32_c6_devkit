import 'package:flutter/services.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../features/connection/board_session_notifier.dart';

const _watchChannel = MethodChannel('czechmate/watch');

bool _watchInboundBound = false;

/// Příkazy z Apple Watch (native → Flutter `onWatchCommand`).
void ensureWatchInboundBinding(WidgetRef ref) {
  if (_watchInboundBound) return;
  _watchInboundBound = true;
  _watchChannel.setMethodCallHandler((call) async {
    if (call.method != 'onWatchCommand') return;
    final args = call.arguments as Map<dynamic, dynamic>?;
    final cmd = args?['cmd'] as String? ?? '';
    final notifier = ref.read(boardSessionNotifierProvider.notifier);
    try {
      switch (cmd) {
        case 'pause':
          await notifier.postTimerPause();
          break;
        case 'resume':
          await notifier.postTimerResume();
          break;
      }
    } catch (_) {}
  });
}
