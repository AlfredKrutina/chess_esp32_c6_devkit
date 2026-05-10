import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app_navigation.dart';
import '../../app_providers.dart';
import '../../core/localization/context_l10n.dart';
import '../../l10n/app_localizations.dart';
import '../../core/analysis/move_evaluation.dart';
import '../../core/analysis/move_quality_aggregator.dart';
import '../../core/models/game_snapshot.dart';
import '../../core/utils/fen_from_board.dart';
import '../../core/utils/opening_eco.dart';
import '../../core/utils/user_facing_error_message.dart';
import '../../core/widgets/animated_linear_busy_strip.dart';
import '../../core/widgets/app_modal_sheet.dart';
import '../../core/widgets/pressable_scale.dart';
import '../../core/widgets/network_status_banners.dart';
import '../connection/board_session_notifier.dart';
import '../game/board/chess_board_widget.dart';
import '../game/state/game_ui_notifier.dart';
import 'eval_line_chart.dart';
import 'move_eval_notifier.dart';

String _moveText(GameHistoryMove m) {
  final s = m.san?.trim();
  if (s != null && s.isNotEmpty) return s;
  if (m.from != null && m.to != null) return '${m.from}${m.to}';
  return '—';
}

Color _gradeColor(MoveGrade? g, ColorScheme cs) {
  switch (g) {
    case MoveGrade.best:
    case MoveGrade.good:
      return cs.primaryContainer == cs.surface
          ? Colors.green.shade700
          : cs.primary;
    case MoveGrade.inaccuracy:
      return Colors.orange.shade800;
    case MoveGrade.mistake:
      return cs.errorContainer;
    case MoveGrade.blunder:
      return cs.error;
    case MoveGrade.error:
    default:
      return cs.onSurface;
  }
}

MoveGrade? _gradeFor(List<MoveEvalHistoryEntry> entries, int globalIndex) {
  final ply = globalIndex + 1;
  for (final e in entries) {
    if (e.moveIndex1Based == ply) return e.grade;
  }
  return null;
}

Widget _buildQualityBlock(
  BuildContext context,
  AppLocalizations l10n,
  String title,
  MoveQualityWindowStats w,
  MoveQualityWindowStats b,
) {
  final theme = Theme.of(context);
  final cs = theme.colorScheme;

  Widget side(String label, MoveQualityWindowStats s) {
    final q = s.count == 0 ? null : s.averageQualityPercent?.round();
    final cp = s.count == 0 ? null : s.averageCpLoss?.round();
    return Expanded(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(
            label,
            style: theme.textTheme.labelMedium?.copyWith(
              color: cs.onSurfaceVariant,
              fontWeight: FontWeight.w600,
            ),
          ),
          const SizedBox(height: 6),
          if (s.count == 0)
            Text(
              '—',
              style: theme.textTheme.titleMedium
                  ?.copyWith(fontWeight: FontWeight.w800),
            )
          else ...[
            Text(
              '${q ?? '—'} %',
              style: theme.textTheme.titleLarge?.copyWith(
                fontWeight: FontWeight.w800,
                fontFeatures: const [FontFeature.tabularFigures()],
              ),
            ),
            const SizedBox(height: 2),
            if (cp != null)
              Text(
                l10n.analysisAvgLossCp('$cp').trim(),
                style: theme.textTheme.bodySmall?.copyWith(
                  color: cs.onSurfaceVariant,
                ),
              ),
            Text(
              l10n
                  .analysisQualitySummaryLine('', '', s.count)
                  .replaceFirst(RegExp(r'^\s*·\s*'), '')
                  .trim(),
              style: theme.textTheme.bodySmall?.copyWith(
                color: cs.onSurfaceVariant,
              ),
            ),
          ],
        ],
      ),
    );
  }

  return Padding(
    padding: const EdgeInsets.only(bottom: 12),
    child: Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(
          title,
          style: theme.textTheme.titleSmall?.copyWith(
            fontWeight: FontWeight.w700,
          ),
        ),
        const SizedBox(height: 8),
        Row(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            side(l10n.timerWhiteShort, w),
            side(l10n.timerBlackShort, b),
          ],
        ),
      ],
    ),
  );
}

/// Parita iOS `AnalysisView` — graf evaluace, kvalita tahů, historie, druhá varianta, vlastní FEN.
class AnalysisScreen extends ConsumerStatefulWidget {
  const AnalysisScreen({super.key});

  @override
  ConsumerState<AnalysisScreen> createState() => _AnalysisScreenState();
}

class _AnalysisScreenState extends ConsumerState<AnalysisScreen> {
  final _fenCtrl = TextEditingController();
  int _depth = 10;
  String _freeResult = '';
  String _secondaryLine = '';
  bool _busyFree = false;
  bool _busySecondary = false;
  String _freePreviewFen = '';
  String? _freeHighlightFrom;
  String? _freeHighlightTo;

  @override
  void dispose() {
    _fenCtrl.dispose();
    super.dispose();
  }

  void _syncFenFromSession(GameSnapshot? snap) {
    if (snap == null) return;
    if (_fenCtrl.text.isEmpty) {
      _fenCtrl.text = fenFromSnapshot(snap);
    }
  }

  Future<void> _confirmAndClearEvalData() async {
    final l10n = context.l10n;
    final cs = Theme.of(context).colorScheme;
    final ok = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: Text(l10n.analysisClearEvalConfirmTitle),
        content: Text(l10n.analysisClearEvalConfirmBody),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx, false),
            child: Text(l10n.commonCancel),
          ),
          FilledButton(
            style: FilledButton.styleFrom(
              backgroundColor: cs.error,
              foregroundColor: cs.onError,
            ),
            onPressed: () => Navigator.pop(ctx, true),
            child: Text(l10n.analysisClearEvalConfirmAction),
          ),
        ],
      ),
    );
    if (ok == true && mounted) {
      ref.read(moveEvalNotifierProvider.notifier).clearHistory();
    }
  }

  void _openSequential(BuildContext context, List<GameHistoryMove> moves,
      List<MoveEvalHistoryEntry> evals) {
    showAppModalBottomSheet<void>(
      context: context,
      isScrollControlled: true,
      showDragHandle: true,
      builder: (ctx) {
        final cs = Theme.of(ctx).colorScheme;
        final l10n = ctx.l10n;
        return DraggableScrollableSheet(
          expand: false,
          initialChildSize: 0.55,
          minChildSize: 0.35,
          maxChildSize: 0.92,
          builder: (_, scroll) => ListView(
            controller: scroll,
            padding: const EdgeInsets.all(16),
            children: [
              Text(l10n.analysisGameProgress,
                  style: Theme.of(ctx).textTheme.titleLarge),
              const SizedBox(height: 8),
              Text(
                l10n.analysisHalfMoveOrderHint,
                style: Theme.of(ctx).textTheme.bodySmall,
              ),
              const Divider(height: 24),
              for (var i = 0; i < moves.length; i++)
                ListTile(
                  dense: true,
                  leading: Text('${i + 1}.',
                      style: const TextStyle(
                          fontFeatures: [FontFeature.tabularFigures()])),
                  title: Text(
                    _moveText(moves[i]),
                    style: TextStyle(
                        color: _gradeColor(_gradeFor(evals, i), cs),
                        fontWeight: FontWeight.w500),
                  ),
                  subtitle: Text(
                      i % 2 == 0 ? l10n.timerWhiteShort : l10n.timerBlackShort),
                ),
            ],
          ),
        );
      },
    );
  }

  @override
  Widget build(BuildContext context) {
    ref.watch(moveEvalNotifierProvider);
    final session = ref.watch(boardSessionNotifierProvider);
    final snap = session.snapshot;
    final ui = ref.watch(gameUiNotifierProvider);
    final moveEval = ref.watch(moveEvalNotifierProvider);
    final cs = Theme.of(context).colorScheme;
    final l10n = context.l10n;
    final chartPalette =
        ref.read(prefsRepositoryProvider).resolvedChartPalette(cs);

    _syncFenFromSession(snap);

    final chartPoints = moveEval.entries
        .where((e) => e.evalWhitePawns != null)
        .map((e) => (
              moveIndex: e.moveIndex1Based,
              eval: e.evalWhitePawns!,
              grade: e.grade
            ))
        .toList();

    final summary = moveEval.entries.isEmpty
        ? null
        : MoveQualityAggregator.boardSummary(moveEval.entries);

    return Scaffold(
      appBar: AppBar(
        title: Text(l10n.analysisAppBarTitle),
        actions: [
          IconButton(
            tooltip: l10n.navPlay,
            icon: const Icon(Icons.grid_on_outlined),
            onPressed: () => ref.read(mainNavTabIndexProvider.notifier).state =
                AppMainTab.game,
          ),
        ],
      ),
      body: ListView(
        padding: const EdgeInsets.fromLTRB(12, 12, 12, 32),
        children: [
          const NetworkStatusBanners(),
          const SizedBox(height: 12),
          Card(
            child: ListTile(
              leading: Icon(Icons.psychology, color: cs.primary),
              title: Text(l10n.analysisStockfishSection),
              subtitle: Text(l10n.analysisIntroEvalHint),
            ),
          ),
          const SizedBox(height: 12),
          if (snap == null) ...[
            Card(
              child: Padding(
                padding: const EdgeInsets.all(16),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.stretch,
                  children: [
                    Text(l10n.analysisNoBoardPosition,
                        style: Theme.of(context).textTheme.titleMedium),
                    const SizedBox(height: 8),
                    Text(l10n.analysisNoBoardPositionHint),
                    const SizedBox(height: 12),
                    PressableScale(
                      child: FilledButton.tonal(
                        onPressed: () => ref
                            .read(mainNavTabIndexProvider.notifier)
                            .state = AppMainTab.game,
                        child: Text(l10n.analysisOpenPlay),
                      ),
                    ),
                  ],
                ),
              ),
            ),
          ] else ...[
            if (!ui.moveEvaluationEnabled)
              Card(
                color: cs.secondaryContainer,
                child: Padding(
                  padding: const EdgeInsets.all(12),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      ListTile(
                        contentPadding: EdgeInsets.zero,
                        leading: Icon(Icons.info_outline,
                            color: cs.onSecondaryContainer),
                        title: Text(l10n.analysisChartDisabledTitle,
                            style: TextStyle(color: cs.onSecondaryContainer)),
                        subtitle: Text(
                          l10n.analysisChartDisabledBoardSubtitle,
                          style: TextStyle(color: cs.onSecondaryContainer),
                        ),
                      ),
                      SwitchListTile(
                        contentPadding: EdgeInsets.zero,
                        title: Text(l10n.analysisEnableMoveEval),
                        value: ui.moveEvaluationEnabled,
                        onChanged: (v) => ref
                            .read(gameUiNotifierProvider.notifier)
                            .setMoveEvaluationEnabled(v),
                      ),
                    ],
                  ),
                ),
              ),
            if (!ui.moveEvaluationEnabled) const SizedBox(height: 12),
            Card(
              child: Padding(
                padding: const EdgeInsets.all(12),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.stretch,
                  children: [
                    Row(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Expanded(
                          child: Column(
                            crossAxisAlignment: CrossAxisAlignment.start,
                            children: [
                              Text(l10n.analysisGameOverview,
                                  style:
                                      Theme.of(context).textTheme.titleMedium),
                              Text(
                                l10n.analysisOverviewSubtitleEval,
                                style: const TextStyle(fontSize: 13),
                              ),
                            ],
                          ),
                        ),
                        if (chartPoints.isNotEmpty)
                          IconButton(
                            tooltip: l10n.analysisClearEvalData,
                            icon: Icon(
                              Icons.delete_outline,
                              color: cs.error.withValues(alpha: 0.85),
                            ),
                            onPressed: _confirmAndClearEvalData,
                          ),
                      ],
                    ),
                    const SizedBox(height: 8),
                    if (chartPoints.isEmpty)
                      Text(
                        ui.moveEvaluationEnabled
                            ? l10n.analysisChartFillHintLiveEvalOn
                            : l10n.analysisChartFillHintLiveEvalOff,
                        style: Theme.of(context).textTheme.bodySmall,
                      )
                    else ...[
                      EvalLineChart(
                        points: chartPoints,
                        accentColor: chartPalette.evalLine,
                      ),
                    ],
                    const SizedBox(height: 8),
                    Row(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Icon(Icons.grid_on, size: 20, color: cs.primary),
                        const SizedBox(width: 8),
                        Expanded(
                          child: Column(
                            crossAxisAlignment: CrossAxisAlignment.start,
                            children: [
                              Text(
                                  'Stav: ${snap.status.gameState} · Na tahu: ${snap.status.currentPlayer} · ${snap.history.moves.length} tahů'),
                              const SizedBox(height: 4),
                              Builder(builder: (ctx) {
                                final fenLine = fenFromSnapshot(snap);
                                final eco = OpeningEco.titleForFen(fenLine);
                                return Column(
                                  crossAxisAlignment: CrossAxisAlignment.start,
                                  children: [
                                    if (eco != null)
                                      Padding(
                                        padding:
                                            const EdgeInsets.only(bottom: 4),
                                        child: Text(eco,
                                            style: TextStyle(
                                                color: cs.primary,
                                                fontWeight: FontWeight.w600)),
                                      ),
                                    SelectableText(fenLine,
                                        style: const TextStyle(
                                            fontFamily: 'monospace',
                                            fontSize: 11)),
                                  ],
                                );
                              }),
                            ],
                          ),
                        ),
                      ],
                    ),
                  ],
                ),
              ),
            ),
            if (summary != null) ...[
              const SizedBox(height: 16),
              Card(
                child: Padding(
                  padding: const EdgeInsets.all(16),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        l10n.analysisMoveQuality,
                        style:
                            Theme.of(context).textTheme.titleMedium?.copyWith(
                                  fontWeight: FontWeight.w700,
                                ),
                      ),
                      const SizedBox(height: 6),
                      Text(
                        l10n.analysisMoveQualityBody,
                        style: Theme.of(context).textTheme.bodySmall?.copyWith(
                              color: cs.onSurfaceVariant,
                            ),
                      ),
                      const SizedBox(height: 12),
                      _buildQualityBlock(
                          context,
                          l10n,
                          l10n.analysisMoveQualitySideLast3,
                          summary.white.last3,
                          summary.black.last3),
                      _buildQualityBlock(
                          context,
                          l10n,
                          l10n.analysisMoveQualitySideLast10,
                          summary.white.last10,
                          summary.black.last10),
                      _buildQualityBlock(
                          context,
                          l10n,
                          l10n.analysisMoveQualityFullGame,
                          summary.white.fullGame,
                          summary.black.fullGame),
                    ],
                  ),
                ),
              ),
            ],
            if (moveEval.lastMessage != null) ...[
              const Divider(height: 24),
              ListTile(
                title: Text(l10n.analysisLastMoveEvalTitle),
                subtitle: Text(
                  moveEval.lastMessage!,
                  style: TextStyle(color: _gradeColor(moveEval.lastGrade, cs)),
                ),
              ),
            ],
            AnimatedLinearBusyStrip(busy: moveEval.lastBusy),
            const SizedBox(height: 16),
            Card(
              clipBehavior: Clip.antiAlias,
              child: InkWell(
                onTap: () => _openSequential(
                    context, snap.history.moves, moveEval.entries),
                child: Padding(
                  padding: const EdgeInsets.fromLTRB(12, 14, 12, 14),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        l10n.analysisMoveHistoryTitle,
                        style:
                            Theme.of(context).textTheme.titleMedium?.copyWith(
                                  fontWeight: FontWeight.w700,
                                ),
                      ),
                      const SizedBox(height: 4),
                      Text(
                        l10n.analysisMoveHistoryTapHint,
                        style: Theme.of(context).textTheme.bodySmall?.copyWith(
                              color: cs.onSurfaceVariant,
                            ),
                      ),
                      const SizedBox(height: 10),
                      SizedBox(
                        height: 48,
                        child: ListView.builder(
                          scrollDirection: Axis.horizontal,
                          padding: const EdgeInsets.only(bottom: 4),
                          itemCount: snap.history.moves.length,
                          itemBuilder: (ctx, i) {
                            final m = snap.history.moves[i];
                            final sideLabel = i % 2 == 0
                                ? l10n.timerWhiteShort
                                : l10n.timerBlackShort;
                            return Padding(
                              padding: const EdgeInsets.only(right: 14),
                              child: Column(
                                mainAxisAlignment: MainAxisAlignment.center,
                                crossAxisAlignment: CrossAxisAlignment.start,
                                children: [
                                  Text(
                                    '$sideLabel · ${i + 1}.',
                                    style: Theme.of(context)
                                        .textTheme
                                        .labelSmall
                                        ?.copyWith(color: cs.onSurfaceVariant),
                                  ),
                                  Text(
                                    _moveText(m),
                                    style: TextStyle(
                                      color: _gradeColor(
                                        _gradeFor(moveEval.entries, i),
                                        cs,
                                      ),
                                      fontWeight: FontWeight.w700,
                                      fontFeatures: const [
                                        FontFeature.tabularFigures(),
                                      ],
                                    ),
                                  ),
                                ],
                              ),
                            );
                          },
                        ),
                      ),
                    ],
                  ),
                ),
              ),
            ),
          ],
          const Divider(height: 32),
          Text(l10n.analysisSecondLineTitle,
              style: Theme.of(context).textTheme.titleMedium),
          Text(l10n.analysisSecondLineBody,
              style: const TextStyle(fontSize: 12)),
          const SizedBox(height: 8),
          if (_secondaryLine.isNotEmpty) Text(_secondaryLine),
          PressableScale(
            child: FilledButton.tonal(
              onPressed: _busySecondary || snap == null
                  ? null
                  : () async {
                      final fen = fenFromSnapshot(snap);
                      setState(() {
                        _busySecondary = true;
                        _secondaryLine = '';
                      });
                      try {
                        final d = ref.read(prefsRepositoryProvider).hintDepth;
                        final pair = await ref
                            .read(stockfishApiClientProvider)
                            .fetchPrimaryAndSecondary(fen, depth: d);
                        final p = pair.primary;
                        final s = pair.secondary;
                        setState(() {
                          final loc = AppLocalizations.of(context)!;
                          if (s != null) {
                            final suffix = p.evalPawns != null
                                ? loc.analysisSecondLineEvalApprox(
                                    p.evalPawns!.toStringAsFixed(2))
                                : '';
                            _secondaryLine = loc.analysisSecondLineDualMoves(
                              p.from,
                              p.to,
                              s.from,
                              s.to,
                              suffix,
                            );
                          } else {
                            _secondaryLine = loc.analysisSecondLineSameMove(
                              p.from,
                              p.to,
                            );
                          }
                        });
                      } catch (e) {
                        setState(
                          () =>
                              _secondaryLine = userFacingErrorSummary(l10n, e),
                        );
                      } finally {
                        if (mounted) setState(() => _busySecondary = false);
                      }
                    },
              child: Text(_busySecondary
                  ? l10n.analysisSecondLineComputing
                  : l10n.analysisSecondLineLoadButton),
            ),
          ),
          const Divider(height: 32),
          Text(l10n.analysisCustomPositionTitle,
              style: Theme.of(context).textTheme.titleMedium),
          Text(l10n.analysisCustomPositionBody,
              style: const TextStyle(fontSize: 12)),
          const SizedBox(height: 8),
          TextField(
            controller: _fenCtrl,
            maxLines: 3,
            decoration: InputDecoration(
              labelText: l10n.analysisFenFieldLabel,
              border: const OutlineInputBorder(),
            ),
          ),
          const SizedBox(height: 8),
          Text(l10n.analysisDepthLabel('$_depth')),
          Slider(
            min: 1,
            max: 18,
            divisions: 17,
            value: _depth.toDouble(),
            label: l10n.analysisDepthLabel('$_depth'),
            onChanged: (v) => setState(() => _depth = v.round()),
          ),
          Wrap(
            spacing: 12,
            runSpacing: 8,
            children: [
              PressableScale(
                child: FilledButton(
                  onPressed: _busyFree
                      ? null
                      : () async {
                          final fen = _fenCtrl.text.trim();
                          if (fen.isEmpty) return;
                          setState(() {
                            _busyFree = true;
                            _freeResult = l10n.analysisFreeAnalyzing;
                          });
                          try {
                            final mv = await ref
                                .read(stockfishApiClientProvider)
                                .fetchBestMove(fen, depth: _depth);
                            final ev = mv.evalPawns != null
                                ? l10n.analysisSecondLineEvalApprox(
                                    mv.evalPawns!.toStringAsFixed(2))
                                : '';
                            setState(() {
                              _freeResult = l10n.analysisBestMoveLine(
                                mv.from,
                                mv.to,
                                ev,
                                mv.text != null ? '\n${mv.text}' : '',
                              );
                              _freePreviewFen = fen;
                              final f = mv.from.trim();
                              final t = mv.to.trim();
                              _freeHighlightFrom = f.isNotEmpty ? f : null;
                              _freeHighlightTo = t.isNotEmpty ? t : null;
                            });
                          } catch (e) {
                            setState(() {
                              _freeResult = l10n.newGameErrorSnack(
                                userFacingErrorSummary(l10n, e),
                              );
                              _freePreviewFen = '';
                              _freeHighlightFrom = null;
                              _freeHighlightTo = null;
                            });
                          } finally {
                            if (mounted) setState(() => _busyFree = false);
                          }
                        },
                  child: Text(l10n.analysisAnalyzePosition),
                ),
              ),
              OutlinedButton(
                onPressed: () {
                  final t = _fenCtrl.text.trim();
                  if (t.isEmpty) return;
                  ref
                      .read(gameUiNotifierProvider.notifier)
                      .enterSandboxFromCustomFen(t);
                  ref.read(mainNavTabIndexProvider.notifier).state =
                      AppMainTab.game;
                  ScaffoldMessenger.of(context).showSnackBar(
                    SnackBar(content: Text(l10n.analysisSandboxFenSnack)),
                  );
                },
                child: Text(l10n.analysisPreviewInPlay),
              ),
            ],
          ),
          const SizedBox(height: 8),
          SelectableText(_freeResult, style: const TextStyle(fontSize: 14)),
          if (_freePreviewFen.isNotEmpty &&
              _freeHighlightFrom != null &&
              _freeHighlightTo != null) ...[
            const SizedBox(height: 12),
            Text(l10n.analysisPreviewMoveTitle,
                style: Theme.of(context).textTheme.titleSmall),
            const SizedBox(height: 8),
            LayoutBuilder(
              builder: (ctx, bc) {
                final side = bc.maxWidth.clamp(200.0, 360.0);
                return Center(
                  child: SizedBox(
                    width: side,
                    child: FenBoardPreview(
                      fen: _freePreviewFen,
                      highlightFrom: _freeHighlightFrom,
                      highlightTo: _freeHighlightTo,
                    ),
                  ),
                );
              },
            ),
          ],
        ],
      ),
    );
  }
}
