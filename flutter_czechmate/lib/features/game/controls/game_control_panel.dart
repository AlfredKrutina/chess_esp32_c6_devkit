import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../connection/board_session_notifier.dart';
import '../../connection/board_session_state.dart';
import '../state/game_ui_notifier.dart';

class GameControlPanel extends ConsumerWidget {
  const GameControlPanel({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final session = ref.watch(boardSessionNotifierProvider);
    final ui = ref.watch(gameUiNotifierProvider);
    final snap = session.snapshot;
    final sessionN = ref.read(boardSessionNotifierProvider.notifier);
    final uiN = ref.read(gameUiNotifierProvider.notifier);
    return Wrap(
      spacing: 8,
      runSpacing: 8,
      children: [
        FilledButton.tonal(
          onPressed: uiN.toggleFlip,
          child: Text(ui.boardFlipped ? 'Perspektiva: černá' : 'Perspektiva: bílá'),
        ),
        FilledButton.tonal(
          onPressed: uiN.toggleCoordinates,
          child: Text(ui.showCoordinates ? 'Souřadnice: zapnuto' : 'Souřadnice: vypnuto'),
        ),
        FilterChip(
          label: const Text('Sandbox'),
          selected: ui.sandboxMode,
          onSelected: (_) => uiN.toggleSandbox(snap),
        ),
        FilterChip(
          label: const Text('Tahy z aplikace'),
          selected: ui.remoteMovesEnabled,
          onSelected: (_) => uiN.toggleRemoteMoves(),
        ),
        if (session.transport == BoardTransport.wifi &&
            session.wifiBaseUrl != null) ...[
          FilledButton.tonal(
            onPressed: () => sessionN.postNewGame(),
            child: const Text('Nová hra'),
          ),
          FilledButton.tonal(
            onPressed: () => sessionN.postTimerPause(),
            child: const Text('Pause časomíry'),
          ),
          FilledButton.tonal(
            onPressed: () => sessionN.postTimerResume(),
            child: const Text('Resume časomíry'),
          ),
          FilledButton.tonal(
            onPressed: () => sessionN.refreshNow(),
            child: const Text('Obnovit'),
          ),
        ],
      ],
    );
  }
}
