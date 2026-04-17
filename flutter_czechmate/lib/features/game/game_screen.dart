import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../connection/board_session_notifier.dart';
import '../connection/board_session_state.dart';
import 'board/chess_board_widget.dart';
import 'controls/game_control_panel.dart';
import 'controls/timer_display.dart';
import 'history/move_history_view.dart';
import 'state/game_ui_notifier.dart';

class GameScreen extends ConsumerWidget {
  const GameScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final session = ref.watch(boardSessionNotifierProvider);
    final ui = ref.watch(gameUiNotifierProvider);
    return Scaffold(
      appBar: AppBar(
        title: const Text('Hra'),
        actions: [
          TextButton(
            onPressed: () => ref.read(boardSessionNotifierProvider.notifier).disconnect(),
            child: const Text('Odpojit'),
          ),
        ],
      ),
      body: session.snapshot == null
          ? Center(
              child: Padding(
                padding: const EdgeInsets.all(24),
                child: Column(
                  mainAxisAlignment: MainAxisAlignment.center,
                  children: [
                    const Text('Zatím žádný snapshot.'),
                    const SizedBox(height: 12),
                    Text(
                      'Otevři záložku „Deska“ — Wi‑Fi, BLE nebo mock.',
                      textAlign: TextAlign.center,
                      style: Theme.of(context).textTheme.bodyMedium,
                    ),
                    if (session.lastError != null) ...[
                      const SizedBox(height: 16),
                      Text(
                        '${session.lastError}',
                        textAlign: TextAlign.center,
                        style: TextStyle(color: Theme.of(context).colorScheme.error),
                      ),
                    ],
                  ],
                ),
              ),
            )
          : LayoutBuilder(
              builder: (context, c) {
                final wide = c.maxWidth >= 840;
                const board = Padding(
                  padding: EdgeInsets.all(12),
                  child: Center(child: ChessBoardWidget()),
                );
                final side = Padding(
                  padding: const EdgeInsets.all(12),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.stretch,
                    children: [
                      _statusLine(session),
                      if (ui.sandboxMessage != null)
                        Text(
                          ui.sandboxMessage!,
                          style: TextStyle(color: Theme.of(context).colorScheme.error),
                        ),
                      const TimerDisplay(),
                      const SizedBox(height: 8),
                      const GameControlPanel(),
                      const SizedBox(height: 8),
                      const Text('Historie', style: TextStyle(fontWeight: FontWeight.w600)),
                      const MoveHistoryView(),
                      if (session.lastError != null) ...[
                        const SizedBox(height: 8),
                        Text(
                          '${session.lastError}',
                          style: TextStyle(color: Theme.of(context).colorScheme.error),
                        ),
                      ],
                    ],
                  ),
                );
                if (wide) {
                  return Row(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      const Expanded(flex: 3, child: board),
                      Expanded(
                        flex: 2,
                        child: SingleChildScrollView(child: side),
                      ),
                    ],
                  );
                }
                return SingleChildScrollView(
                  child: Column(children: [board, side]),
                );
              },
            ),
    );
  }

  Widget _statusLine(BoardSessionState s) {
    final t = switch (s.transport) {
      BoardTransport.none => 'Nepřipojeno',
      BoardTransport.mock => 'Mock deska',
      BoardTransport.wifi => 'Wi‑Fi: ${s.wifiBaseUrl ?? ''}',
      BoardTransport.ble => 'BLE: ${s.bleLabel ?? "deska"}',
    };
    final poll = s.pollingActive ? ' • poll' : '';
    final ws = s.webSocketActive ? ' • WS' : '';
    return Text('$t$poll$ws', style: const TextStyle(fontSize: 13));
  }
}
