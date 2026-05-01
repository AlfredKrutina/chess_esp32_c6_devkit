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
    final raw =
        ref.watch(boardSessionNotifierProvider).snapshot?.history.moves;
    final moves = raw == null ? <GameHistoryMove>[] : List<GameHistoryMove>.of(raw);
    if (moves.isEmpty) {
      return Card(
        child: Padding(
          padding: const EdgeInsets.all(16),
          child: Text(l10n.moveHistoryEmpty),
        ),
      );
    }
    return Card(
      child: ConstrainedBox(
        constraints: const BoxConstraints(maxHeight: 220),
        child: ListView.builder(
          padding: const EdgeInsets.all(8),
          itemCount: moves.length,
          itemBuilder: (context, i) {
            final m = moves[i];
            final label = m.san ?? '${m.from ?? '?'}→${m.to ?? '?'}';
            return ListTile(
              dense: true,
              enabled: !ui.sandboxMode,
              selected:
                  !ui.sandboxMode && ui.historyReviewMoveIndex == i,
              title: Text('${i + 1}. $label'),
              subtitle: m.piece != null
                  ? Text(l10n.moveHistoryPieceSubtitle(m.piece!))
                  : null,
              onTap: ui.sandboxMode
                  ? null
                  : () {
                      ref.read(gameUiNotifierProvider.notifier).setHistoryReviewIndex(
                            i,
                            ref.read(boardSessionNotifierProvider).snapshot,
                          );
                    },
            );
          },
        ),
      ),
    );
  }
}
