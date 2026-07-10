import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../features/connection/board_session_notifier.dart';
import '../../features/connection/board_session_state.dart';
import '../localization/context_l10n.dart';
import '../models/game_snapshot.dart';
import '../utils/board_action_feedback.dart';
import '../utils/matrix_guard_utils.dart';
import 'pressable_scale.dart';

/// Shown when firmware pauses play until physical board matches logic.
class MatrixGuardBanner extends ConsumerStatefulWidget {
  const MatrixGuardBanner({super.key, required this.snapshot});

  final GameSnapshot snapshot;

  @override
  ConsumerState<MatrixGuardBanner> createState() => _MatrixGuardBannerState();
}

class _MatrixGuardBannerState extends ConsumerState<MatrixGuardBanner> {
  bool _clearBusy = false;

  Future<void> _forceClear() async {
    if (_clearBusy) return;
    setState(() => _clearBusy = true);
    try {
      await runBoardCommandWithSnackBar(
        context,
        ref.read(boardSessionNotifierProvider.notifier).postGuardClear,
        successMessage: context.l10n.gameMatrixGuardForceClearSnack,
      );
    } finally {
      if (mounted) setState(() => _clearBusy = false);
    }
  }

  @override
  Widget build(BuildContext context) {
    final status = widget.snapshot.status;
    if (status.matrixGuardActive != true) {
      return const SizedBox.shrink();
    }

    final l10n = context.l10n;
    final theme = Theme.of(context);
    final cs = theme.colorScheme;
    final session = ref.watch(boardSessionNotifierProvider);
    final canForceClear = session.transport == BoardTransport.wifi ||
        session.transport == BoardTransport.ble;
    final squares = matrixGuardSquaresLabel(
      liftedLow: status.matrixGuardLiftedLow,
      liftedHigh: status.matrixGuardLiftedHigh,
      droppedLow: status.matrixGuardDroppedLow,
      droppedHigh: status.matrixGuardDroppedHigh,
    );
    final squareSuffix = squares == '—' ? '' : ' ($squares)';
    final resync = status.restoreState?.resyncRequired == true;
    final body = resync
        ? l10n.gameMatrixGuardResync(squareSuffix)
        : l10n.gameMatrixGuardPaused(squareSuffix);
    final onSurface = cs.onErrorContainer;

    return Padding(
      padding: const EdgeInsets.fromLTRB(12, 0, 12, 8),
      child: Material(
        color: cs.errorContainer.withValues(alpha: 0.88),
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(14),
          side: BorderSide(color: cs.error.withValues(alpha: 0.35)),
        ),
        child: Padding(
          padding: const EdgeInsets.fromLTRB(14, 12, 14, 12),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: [
              Row(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  DecoratedBox(
                    decoration: BoxDecoration(
                      color: cs.error.withValues(alpha: 0.18),
                      shape: BoxShape.circle,
                    ),
                    child: Padding(
                      padding: const EdgeInsets.all(8),
                      child: Icon(
                        Icons.grid_on_outlined,
                        size: 20,
                        color: onSurface,
                      ),
                    ),
                  ),
                  const SizedBox(width: 12),
                  Expanded(
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text(
                          l10n.gameMatrixGuardTitle,
                          style: theme.textTheme.titleSmall?.copyWith(
                            color: onSurface,
                            fontWeight: FontWeight.w700,
                          ),
                        ),
                        const SizedBox(height: 4),
                        Text(
                          body,
                          style: theme.textTheme.bodySmall?.copyWith(
                            color: onSurface.withValues(alpha: 0.92),
                            height: 1.4,
                          ),
                        ),
                      ],
                    ),
                  ),
                ],
              ),
              if (canForceClear) ...[
                const SizedBox(height: 12),
                Align(
                  alignment: Alignment.centerRight,
                  child: PressableScale(
                    child: OutlinedButton.icon(
                      onPressed: _clearBusy ? null : _forceClear,
                      style: OutlinedButton.styleFrom(
                        foregroundColor: onSurface,
                        side: BorderSide(
                          color: onSurface.withValues(alpha: 0.35),
                        ),
                        padding: const EdgeInsets.symmetric(
                          horizontal: 14,
                          vertical: 10,
                        ),
                        visualDensity: VisualDensity.compact,
                        shape: RoundedRectangleBorder(
                          borderRadius: BorderRadius.circular(10),
                        ),
                      ),
                      icon: _clearBusy
                          ? SizedBox(
                              width: 16,
                              height: 16,
                              child: CircularProgressIndicator(
                                strokeWidth: 2,
                                color: onSurface,
                              ),
                            )
                          : Icon(
                              Icons.play_arrow_rounded,
                              size: 18,
                              color: onSurface,
                            ),
                      label: Text(l10n.gameMatrixGuardForceClear),
                    ),
                  ),
                ),
                const SizedBox(height: 2),
                Text(
                  l10n.gameMatrixGuardForceClearHint,
                  textAlign: TextAlign.end,
                  style: theme.textTheme.labelSmall?.copyWith(
                    color: onSurface.withValues(alpha: 0.72),
                  ),
                ),
              ],
            ],
          ),
        ),
      ),
    );
  }
}
