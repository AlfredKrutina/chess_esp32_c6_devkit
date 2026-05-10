import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app_navigation.dart';
import '../../app_providers.dart';
import '../../core/analysis/move_evaluation.dart';
import '../../core/models/game_snapshot.dart';
import '../../core/widgets/glass_snackbar.dart';
import '../../l10n/app_localizations.dart';
import '../game/state/game_ui_notifier.dart';

String moveText(GameHistoryMove m) {
  final s = m.san?.trim();
  if (s != null && s.isNotEmpty) return s;
  if (m.from != null && m.to != null) return '${m.from}${m.to}';
  return '—';
}

MoveGrade? gradeForEntry(List<MoveEvalHistoryEntry> entries, int globalIndex) {
  final ply = globalIndex + 1;
  for (final e in entries) {
    if (e.moveIndex1Based == ply) return e.grade;
  }
  return null;
}

Color gradeTint(MoveGrade? g, ColorScheme cs) {
  switch (g) {
    case MoveGrade.best:
    case MoveGrade.good:
      return Colors.green.withValues(alpha: 0.12);
    case MoveGrade.inaccuracy:
      return Colors.orange.withValues(alpha: 0.14);
    case MoveGrade.mistake:
      return cs.errorContainer.withValues(alpha: 0.35);
    case MoveGrade.blunder:
      return cs.error.withValues(alpha: 0.18);
    case MoveGrade.error:
    default:
      return Colors.transparent;
  }
}

/// Classic scoresheet: move number, White, Black — tap jumps to Play with board replay.
class AnalysisScoresheetTable extends ConsumerWidget {
  const AnalysisScoresheetTable({
    super.key,
    required this.moves,
    required this.evals,
    required this.snap,
    required this.l10n,
  });

  final List<GameHistoryMove> moves;
  final List<MoveEvalHistoryEntry> evals;
  final GameSnapshot snap;
  final AppLocalizations l10n;

  void _jump(BuildContext context, WidgetRef ref, int historyIndex) {
    ref.read(mainNavTabIndexProvider.notifier).state = AppMainTab.game;
    ref
        .read(gameUiNotifierProvider.notifier)
        .setHistoryReviewIndex(historyIndex, snap);
    if (!context.mounted) return;
    showAppSnackBar(
      context,
      l10n.analysisChartJumpedToMove(
        historyIndex + 1,
      ),
    );
  }

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final cs = Theme.of(context).colorScheme;
    final tt = Theme.of(context).textTheme;
    final n = moves.length;
    if (n == 0) {
      return Text(
        l10n.analysisScoresheetEmpty,
        style: tt.bodySmall?.copyWith(color: cs.onSurfaceVariant),
      );
    }
    final rows = (n + 1) ~/ 2;

    Widget cell(String text, MoveGrade? g, VoidCallback onTap) {
      final tint = gradeTint(g, cs);
      return InkWell(
        onTap: onTap,
        child: Container(
          width: double.infinity,
          padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 10),
          decoration: BoxDecoration(color: tint),
          child: Text(
            text,
            style: tt.bodyMedium?.copyWith(fontWeight: FontWeight.w600),
          ),
        ),
      );
    }

    return LayoutBuilder(
      builder: (context, bc) {
        return Scrollbar(
          thumbVisibility: bc.maxWidth < 340,
          child: SingleChildScrollView(
            scrollDirection: Axis.horizontal,
            child: ConstrainedBox(
              constraints:
                  BoxConstraints(minWidth: bc.maxWidth.clamp(280.0, 560)),
              child: Table(
                border: TableBorder.all(
                    color: cs.outlineVariant.withValues(alpha: 0.45)),
                columnWidths: const {
                  0: FixedColumnWidth(36),
                  1: FlexColumnWidth(2),
                  2: FlexColumnWidth(2),
                },
                children: [
                  TableRow(
                    decoration:
                        BoxDecoration(color: cs.surfaceContainerHighest),
                    children: [
                      Padding(
                        padding: const EdgeInsets.symmetric(vertical: 8),
                        child: Text(
                          '#',
                          textAlign: TextAlign.center,
                          style: tt.labelSmall
                              ?.copyWith(fontWeight: FontWeight.w800),
                        ),
                      ),
                      Padding(
                        padding: const EdgeInsets.symmetric(
                            horizontal: 8, vertical: 8),
                        child: Text(
                          l10n.timerWhiteShort,
                          style: tt.labelSmall
                              ?.copyWith(fontWeight: FontWeight.w800),
                        ),
                      ),
                      Padding(
                        padding: const EdgeInsets.symmetric(
                            horizontal: 8, vertical: 8),
                        child: Text(
                          l10n.timerBlackShort,
                          style: tt.labelSmall
                              ?.copyWith(fontWeight: FontWeight.w800),
                        ),
                      ),
                    ],
                  ),
                  for (var r = 0; r < rows; r++) ...[
                    TableRow(
                      children: [
                        Padding(
                          padding: const EdgeInsets.symmetric(vertical: 10),
                          child: Text(
                            '${r + 1}',
                            textAlign: TextAlign.center,
                            style: tt.labelMedium?.copyWith(
                              color: cs.onSurfaceVariant,
                              fontFeatures: const [
                                FontFeature.tabularFigures()
                              ],
                            ),
                          ),
                        ),
                        Builder(
                          builder: (ctx) {
                            final wi = 2 * r;
                            if (wi >= n) {
                              return const SizedBox(height: 44);
                            }
                            return cell(
                              moveText(moves[wi]),
                              gradeForEntry(evals, wi),
                              () => _jump(ctx, ref, wi),
                            );
                          },
                        ),
                        Builder(
                          builder: (ctx) {
                            final bi = 2 * r + 1;
                            if (bi >= n) {
                              return Container(
                                width: double.infinity,
                                padding: const EdgeInsets.symmetric(
                                  horizontal: 8,
                                  vertical: 10,
                                ),
                                child: Text(
                                  '—',
                                  style: tt.bodyMedium?.copyWith(
                                    color: cs.onSurfaceVariant,
                                  ),
                                ),
                              );
                            }
                            return cell(
                              moveText(moves[bi]),
                              gradeForEntry(evals, bi),
                              () => _jump(ctx, ref, bi),
                            );
                          },
                        ),
                      ],
                    ),
                  ],
                ],
              ),
            ),
          ),
        );
      },
    );
  }
}
