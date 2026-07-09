import 'package:flutter/material.dart';

import '../localization/context_l10n.dart';
import '../models/game_snapshot.dart';
import '../utils/matrix_guard_utils.dart';

/// Shown when firmware pauses play until physical board matches logic.
class MatrixGuardBanner extends StatelessWidget {
  const MatrixGuardBanner({super.key, required this.snapshot});

  final GameSnapshot snapshot;

  @override
  Widget build(BuildContext context) {
    final status = snapshot.status;
    if (status.matrixGuardActive != true) {
      return const SizedBox.shrink();
    }

    final l10n = context.l10n;
    final cs = Theme.of(context).colorScheme;
    final squares = matrixGuardSquaresLabel(
      liftedLow: status.matrixGuardLiftedLow,
      liftedHigh: status.matrixGuardLiftedHigh,
      droppedLow: status.matrixGuardDroppedLow,
      droppedHigh: status.matrixGuardDroppedHigh,
    );
    final resync = status.restoreState?.resyncRequired == true;
    final text = resync
        ? l10n.gameMatrixGuardResync(squares)
        : l10n.gameMatrixGuardPaused(squares);

    return Padding(
      padding: const EdgeInsets.fromLTRB(12, 0, 12, 8),
      child: Material(
        color: cs.errorContainer.withValues(alpha: 0.92),
        borderRadius: BorderRadius.circular(12),
        child: ListTile(
          dense: true,
          leading: Icon(Icons.pause_circle_outline, color: cs.onErrorContainer),
          title: Text(
            l10n.gameMatrixGuardTitle,
            style: TextStyle(
              color: cs.onErrorContainer,
              fontWeight: FontWeight.w600,
            ),
          ),
          subtitle: Text(
            text,
            style: TextStyle(color: cs.onErrorContainer),
          ),
        ),
      ),
    );
  }
}
