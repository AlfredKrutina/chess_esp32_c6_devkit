import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../../core/localization/context_l10n.dart';
import '../../../core/models/game_snapshot.dart';
import '../../connection/board_session_notifier.dart';
import '../state/game_ui_notifier.dart';

class MoveHistoryView extends ConsumerWidget {
  const MoveHistoryView({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final l10n = context.l10n;
    final ui = ref.watch(gameUiNotifierProvider);
    final session = ref.watch(boardSessionNotifierProvider);
    final snap = session.snapshot;
    final raw = snap?.history.moves;
    final moves =
        raw == null ? <GameHistoryMove>[] : List<GameHistoryMove>.of(raw);
    if (moves.isEmpty) {
      return Card(
        child: Padding(
          padding: const EdgeInsets.all(16),
          child: Text(l10n.moveHistoryEmpty),
        ),
      );
    }
    final notifier = ref.read(gameUiNotifierProvider.notifier);
    return Card(
      child: ConstrainedBox(
        constraints: const BoxConstraints(maxHeight: 220),
        child: ListView.builder(
          padding: const EdgeInsets.all(8),
          itemCount: moves.length + 1,
          itemBuilder: (context, listIndex) {
            if (listIndex == 0) {
              return ListTile(
                dense: true,
                leading: Icon(
                  Icons.radio_button_checked,
                  size: 18,
                  color: Theme.of(context).colorScheme.primary,
                ),
                title: Text(l10n.moveHistoryCurrentPosition),
                selected: !ui.sandboxMode && ui.historyReviewMoveIndex == null,
                enabled: !ui.sandboxMode,
                onTap: ui.sandboxMode
                    ? null
                    : () => notifier.setHistoryReviewIndex(null, snap),
              );
            }
            final chronologicalIndex = moves.length - listIndex;
            final m = moves[chronologicalIndex];
            final label = m.san ?? '${m.from ?? '?'}→${m.to ?? '?'}';
            final moveNo = chronologicalIndex + 1;
            return ListTile(
              dense: true,
              enabled: !ui.sandboxMode,
              selected: !ui.sandboxMode &&
                  ui.historyReviewMoveIndex == chronologicalIndex,
              title: Text('$moveNo. $label'),
              subtitle: m.piece != null
                  ? Text(l10n.moveHistoryPieceSubtitle(m.piece!))
                  : null,
              onTap: ui.sandboxMode
                  ? null
                  : () => notifier.setHistoryReviewIndex(
                        chronologicalIndex,
                        snap,
                      ),
            );
          },
        ),
      ),
    );
  }
}
