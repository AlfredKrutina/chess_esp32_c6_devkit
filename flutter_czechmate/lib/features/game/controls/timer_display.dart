import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../../core/models/board_timer_state.dart';
import '../../connection/board_session_notifier.dart';

class TimerDisplay extends ConsumerWidget {
  const TimerDisplay({super.key});

  static String _fmtMs(int ms) {
    final s = (ms / 1000).floor();
    final m = (s ~/ 60).toString().padLeft(2, '0');
    final r = (s % 60).toString().padLeft(2, '0');
    return '$m:$r';
  }

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final session = ref.watch(boardSessionNotifierProvider);
    BoardTimerState? t = session.timer;
    final c = session.snapshot?.clock;
    if (c != null) t = c;
    final snap = session.snapshot;
    final gameOver = snap != null &&
        (snap.status.gameEnd?.ended == true ||
            snap.status.gameState.toLowerCase() == 'finished');
    if (t == null || !t.isTimeControlEnabled) {
      return const Card(
        child: Padding(
          padding: EdgeInsets.all(12),
          child: Text('Časomíra: vypnutá nebo nedostupná'),
        ),
      );
    }
    final running = !gameOver && t.timerRunning;
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(12),
        child: Row(
          mainAxisAlignment: MainAxisAlignment.spaceBetween,
          children: [
            Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                const Text('Bílá', style: TextStyle(fontSize: 12)),
                Text(
                  _fmtMs(t.whiteTimeMs),
                  style: const TextStyle(
                    fontSize: 20,
                    fontFeatures: [FontFeature.tabularFigures()],
                  ),
                ),
              ],
            ),
            Column(
              children: [
                Icon(
                  (gameOver || t.gamePaused) ? Icons.pause_circle : Icons.timer,
                  size: 22,
                ),
                Text(
                  gameOver
                      ? 'konec partie'
                      : (running ? 'běží' : 'stojí'),
                  style: const TextStyle(fontSize: 11),
                ),
              ],
            ),
            Column(
              crossAxisAlignment: CrossAxisAlignment.end,
              children: [
                const Text('Černá', style: TextStyle(fontSize: 12)),
                Text(
                  _fmtMs(t.blackTimeMs),
                  style: const TextStyle(
                    fontSize: 20,
                    fontFeatures: [FontFeature.tabularFigures()],
                  ),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }
}
