import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../../core/models/game_snapshot.dart';
import '../../connection/board_session_notifier.dart';

class MoveHistoryView extends ConsumerWidget {
  const MoveHistoryView({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final raw =
        ref.watch(boardSessionNotifierProvider).snapshot?.history.moves;
    final moves = raw == null ? <GameHistoryMove>[] : List<GameHistoryMove>.of(raw);
    if (moves.isEmpty) {
      return const Card(
        child: Padding(
          padding: EdgeInsets.all(16),
          child: Text('Historie tahů zatím prázdná'),
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
              title: Text('${i + 1}. $label'),
              subtitle: m.piece != null ? Text('figura: ${m.piece}') : null,
            );
          },
        ),
      ),
    );
  }
}
