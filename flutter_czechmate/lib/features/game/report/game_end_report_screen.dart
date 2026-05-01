import 'dart:convert';
import 'dart:io';
import 'dart:math' as math;
import 'dart:typed_data';
import 'dart:ui' as ui;

import 'package:flutter/material.dart';
import 'package:flutter/rendering.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:path_provider/path_provider.dart';
import 'package:share_plus/share_plus.dart';

import '../../../core/models/game_end_report_model.dart';
import '../../../l10n/app_localizations.dart';
import '../../../core/utils/fen_from_board.dart';
import '../../../core/utils/game_end_report_timing.dart';
import '../../../core/utils/opening_eco.dart';
import '../../../core/utils/pgn_export.dart';
import '../../../core/localization/context_l10n.dart';
import '../../../core/widgets/glass_snackbar.dart';
import '../../analysis/eval_line_chart.dart';
import '../../analysis/move_eval_notifier.dart';
import '../../connection/board_session_notifier.dart';
import 'timing_charts.dart';

class GameEndReportScreen extends ConsumerStatefulWidget {
  const GameEndReportScreen({super.key});

  @override
  ConsumerState<GameEndReportScreen> createState() => _GameEndReportScreenState();
}

class _GameEndReportScreenState extends ConsumerState<GameEndReportScreen> {
  final GlobalKey _shareBoundaryKey = GlobalKey();
  bool _isExporting = false;

  String _formatDuration(AppLocalizations l10n, int sec) {
    if (sec <= 0) return l10n.durationDash;
    final m = sec ~/ 60;
    final s = sec % 60;
    if (m >= 60) {
      final h = m ~/ 60;
      final mm = m % 60;
      return l10n.hoursMinutesSeconds(h, mm, s);
    }
    if (m > 0) {
      return l10n.minutesSeconds(m, s.toString().padLeft(2, '0'));
    }
    return l10n.secondsShort(s);
  }

  String _formatAvgSec(AppLocalizations l10n, double v) {
    if (v <= 0) return l10n.durationDash;
    return '${v.toStringAsFixed(1)}s';
  }

  String _localizedGameEndReason(AppLocalizations l10n, GameEndReportModel model) {
    switch (model.reason) {
      case 'Unknown result':
        return l10n.endReasonUnknown;
      case 'Game over':
        return l10n.endReasonGameOver;
      case 'Checkmate':
        return l10n.endReasonCheckmate;
      case 'Stalemate':
        return l10n.endReasonStalemate;
      case 'Finished':
        return l10n.endReasonFinished;
      default:
        return model.reason;
    }
  }

  Future<void> _sharePgn() async {
    final snap = ref.read(boardSessionNotifierProvider).snapshot;
    if (snap == null) return;
    final l10n = AppLocalizations.of(context)!;
    final pgn = buildPgnFromSnapshot(
      snap,
      eventHeader: l10n.pgnEventHeader,
      openingPrefix: l10n.pgnOpeningPrefix,
    );
    final bytes = Uint8List.fromList(utf8.encode(pgn));
    if (!mounted) return;
    final box = context.findRenderObject() as RenderBox?;
    final rect = box != null ? box.localToGlobal(Offset.zero) & box.size : null;
    final tmp = await getTemporaryDirectory();
    final dir = Directory('${tmp.path}/czechmate_share');
    await dir.create(recursive: true);
    final file = File('${dir.path}/czechmate_game.pgn');
    await file.writeAsBytes(bytes);
    await Share.shareXFiles(
      [XFile(file.path, mimeType: 'application/x-chess-pgn')],
      text: l10n.sharePgnSubject,
      sharePositionOrigin: rect,
    );
  }

  Future<void> _exportAndShare() async {
    final l10n = AppLocalizations.of(context)!;
    setState(() => _isExporting = true);
    try {
      final boundary = _shareBoundaryKey.currentContext?.findRenderObject() as RenderRepaintBoundary?;
      if (boundary == null) {
        if (mounted) {
          showGlassSnackBar(context, l10n.shareSummaryNotReady);
        }
        return;
      }

      await WidgetsBinding.instance.endOfFrame;

      final image = await boundary.toImage(pixelRatio: 3.0);
      final byteData = await image.toByteData(format: ui.ImageByteFormat.png);
      if (byteData == null) {
        throw StateError(l10n.sharePngEncodeError);
      }
      final pngBytes = byteData.buffer.asUint8List();

      final snap = ref.read(boardSessionNotifierProvider).snapshot;
      final reportModel = snap != null ? GameEndReportModel.fromSnapshot(snap) : null;
      if (!mounted) return;
      final box = context.findRenderObject() as RenderBox?;
      final rect = box != null ? box.localToGlobal(Offset.zero) & box.size : null;

      final meta = reportModel == null
          ? l10n.appTitle
          : l10n.shareGameSummaryMeta(
              _resultText(l10n, reportModel.result),
              _formatDuration(l10n, reportModel.durationSec),
              reportModel.totalMoves,
            );
      final tmp = await getTemporaryDirectory();
      final dir = Directory('${tmp.path}/czechmate_share');
      await dir.create(recursive: true);
      final pngFile = File('${dir.path}/czechmate_game_report.png');
      await pngFile.writeAsBytes(pngBytes);
      await Share.shareXFiles(
        [XFile(pngFile.path, mimeType: 'image/png')],
        text: l10n.shareGameSummaryLine(meta),
        sharePositionOrigin: rect,
      );
    } catch (e) {
      if (mounted) showGlassSnackBar(context, l10n.shareExportFailed('$e'));
    } finally {
      if (mounted) setState(() => _isExporting = false);
    }
  }

  @override
  Widget build(BuildContext context) {
    final l10n = context.l10n;
    final session = ref.watch(boardSessionNotifierProvider);
    final snapshot = session.snapshot;
    if (snapshot == null) {
      return Scaffold(body: Center(child: Text(l10n.reportNoGameData)));
    }

    final model = GameEndReportModel.fromSnapshot(snapshot);
    final fen = fenFromSnapshot(snapshot);
    final eco = OpeningEco.titleForFen(fen);
    final moveEval = ref.watch(moveEvalNotifierProvider);
    final nMoves = snapshot.history.moves.length;
    final evalPoints = moveEval.entries
        .where((e) => e.evalWhitePawns != null && e.moveIndex1Based <= nMoves)
        .map((e) => (moveIndex: e.moveIndex1Based, eval: e.evalWhitePawns!, grade: e.grade))
        .toList();

    final think = GameEndReportTiming.thinkPlyPoints(snapshot.history.moves);
    final cumulative = GameEndReportTiming.cumulativePoints(think);
    final barPoints = think.where((p) => p.secondsFromPrevious != null).toList();
    final hasTiming = barPoints.isNotEmpty || cumulative.isNotEmpty;
    final clock = snapshot.clock;
    final cs = Theme.of(context).colorScheme;
    final tt = Theme.of(context).textTheme;

    Widget evalSection({required double chartHeight}) {
      return Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Text(l10n.reportMoveEvaluation, style: tt.titleMedium),
          const SizedBox(height: 6),
          if (evalPoints.isEmpty)
            Text(
              l10n.reportNoStockfishData,
              style: tt.bodySmall?.copyWith(color: cs.onSurfaceVariant),
            )
          else ...[
            Text(
              l10n.reportEvalPerspective,
              style: tt.bodySmall?.copyWith(color: cs.onSurfaceVariant),
            ),
            const SizedBox(height: 8),
            EvalLineChart(points: evalPoints, height: chartHeight),
          ],
        ],
      );
    }

    Widget timingSection() {
      return Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Text(l10n.reportMoveTiming, style: tt.titleMedium),
          const SizedBox(height: 6),
          if (!hasTiming)
            Card.outlined(
              child: Padding(
                padding: const EdgeInsets.all(14),
                child: Text(
                  l10n.reportTimingUnavailable,
                  style: tt.bodySmall?.copyWith(color: cs.onSurfaceVariant),
                ),
              ),
            )
          else ...[
            Card.outlined(
              child: Padding(
                padding: const EdgeInsets.all(14),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(l10n.reportElapsedTime,
                        style: tt.titleSmall?.copyWith(fontWeight: FontWeight.w600)),
                    const SizedBox(height: 4),
                    Text(
                      l10n.reportElapsedCaption,
                      style: tt.bodySmall?.copyWith(color: cs.onSurfaceVariant),
                    ),
                    const SizedBox(height: 10),
                    CumulativePlayedTimeChart(points: cumulative, accent: cs.primary),
                    Text(
                      l10n.reportHalfMoveAxis,
                      style: tt.labelSmall?.copyWith(color: cs.onSurfaceVariant),
                    ),
                  ],
                ),
              ),
            ),
            const SizedBox(height: 12),
            Card.outlined(
              child: Padding(
                padding: const EdgeInsets.all(14),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(l10n.reportTimePerMove,
                        style: tt.titleSmall?.copyWith(fontWeight: FontWeight.w600)),
                    const SizedBox(height: 4),
                    Text(
                      l10n.reportSecondsSincePrev,
                      style: tt.bodySmall?.copyWith(color: cs.onSurfaceVariant),
                    ),
                    const SizedBox(height: 10),
                    TimePerMoveBarChart(
                      points: think,
                      whiteColor: cs.primary,
                      blackColor: Colors.purple,
                    ),
                    const SizedBox(height: 8),
                    Wrap(
                      spacing: 20,
                      children: [
                        _LegendDot(
                            color: cs.primary.withValues(alpha: 0.75),
                            label: l10n.colorWhite),
                        _LegendDot(
                            color: Colors.purple.withValues(alpha: 0.55),
                            label: l10n.colorBlack),
                      ],
                    ),
                  ],
                ),
              ),
            ),
          ],
        ],
      );
    }

    final shareCard = RepaintBoundary(
      key: _shareBoundaryKey,
      child: Container(
        padding: const EdgeInsets.fromLTRB(22, 22, 22, 20),
        decoration: BoxDecoration(
          gradient: LinearGradient(
            begin: Alignment.topLeft,
            end: Alignment.bottomRight,
            colors: [
              cs.surfaceContainerHigh,
              cs.surfaceContainerHighest.withValues(alpha: 0.85),
            ],
          ),
          borderRadius: BorderRadius.circular(16),
          border: Border.all(color: cs.outlineVariant.withValues(alpha: 0.5)),
        ),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                Icon(Icons.grid_view_rounded, size: 22, color: cs.primary),
                const SizedBox(width: 8),
                Text(
                  l10n.pgnEventHeader,
                  style: tt.labelLarge?.copyWith(
                    letterSpacing: 1.2,
                    fontWeight: FontWeight.w700,
                    color: cs.primary,
                  ),
                ),
              ],
            ),
            const SizedBox(height: 18),
            Text(
              _resultText(l10n, model.result),
              style: tt.headlineMedium?.copyWith(fontWeight: FontWeight.bold),
            ),
            const SizedBox(height: 4),
            Text(
              _localizedGameEndReason(l10n, model),
              style: tt.bodyMedium?.copyWith(color: cs.onSurfaceVariant),
            ),
            const SizedBox(height: 20),
            Divider(height: 1, color: cs.outlineVariant.withValues(alpha: 0.6)),
            const SizedBox(height: 18),
            Wrap(
              spacing: 20,
              runSpacing: 14,
              alignment: WrapAlignment.spaceBetween,
              children: [
                _ShareStatChip(label: l10n.statTime, value: _formatDuration(l10n, model.durationSec)),
                _ShareStatChip(label: l10n.statMoves, value: '${model.totalMoves}'),
                _ShareStatChip(label: l10n.statWhiteAvg, value: _formatAvgSec(l10n, model.whiteAvgMoveSec)),
                _ShareStatChip(label: l10n.statBlackAvg, value: _formatAvgSec(l10n, model.blackAvgMoveSec)),
              ],
            ),
            if (model.capturedValueWhite > 0 || model.capturedValueBlack > 0) ...[
              const SizedBox(height: 16),
              Text(
                l10n.reportMaterialCaptured(model.capturedValueWhite, model.capturedValueBlack),
                style: tt.bodySmall?.copyWith(color: cs.onSurfaceVariant),
              ),
            ],
          ],
        ),
      ),
    );

    return Scaffold(
      backgroundColor: cs.surface,
      appBar: AppBar(
        title: Text(l10n.reportAppBarTitle),
        centerTitle: true,
        scrolledUnderElevation: 0,
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: Text(l10n.reportDone),
          ),
        ],
      ),
      body: LayoutBuilder(
        builder: (context, constraints) {
          const sidePad = 16.0;
          final avail = (constraints.maxWidth - sidePad * 2).clamp(0.0, double.infinity);
          final evalHasChart = evalPoints.isNotEmpty;
          final useTwoCol = avail >= 920 && evalHasChart && hasTiming;
          final maxW = useTwoCol ? math.min(avail, 1080.0) : math.min(avail, 640.0);

          return Align(
            alignment: Alignment.topCenter,
            child: ConstrainedBox(
              constraints: BoxConstraints(maxWidth: maxW),
              child: ListView(
                padding: EdgeInsets.fromLTRB(sidePad, 8, sidePad, 40),
                children: [
                  Text(
                    l10n.reportResultHeading,
                    style: tt.titleSmall?.copyWith(color: cs.onSurfaceVariant, fontWeight: FontWeight.w600),
                  ),
                  const SizedBox(height: 10),
                  shareCard,
                  const SizedBox(height: 22),
                  Text(
                    l10n.reportShareHeading,
                    style: tt.titleSmall?.copyWith(color: cs.onSurfaceVariant, fontWeight: FontWeight.w600),
                  ),
                  const SizedBox(height: 8),
                  Card.outlined(
                    margin: EdgeInsets.zero,
                    child: Padding(
                      padding: const EdgeInsets.fromLTRB(14, 14, 14, 12),
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.stretch,
                        children: [
                          Text(
                            l10n.reportShareExportHint,
                            style: tt.bodySmall?.copyWith(color: cs.onSurfaceVariant),
                          ),
                          const SizedBox(height: 14),
                          FilledButton.icon(
                            onPressed: _isExporting ? null : _exportAndShare,
                            icon: _isExporting
                                ? SizedBox(
                                    width: 18,
                                    height: 18,
                                    child: CircularProgressIndicator(
                                      strokeWidth: 2,
                                      color: cs.onPrimary,
                                    ),
                                  )
                                : const Icon(Icons.ios_share_rounded),
                            label: Text(_isExporting ? l10n.reportSharePreparing : l10n.reportShareSummaryImage),
                          ),
                          const SizedBox(height: 10),
                          OutlinedButton.icon(
                            onPressed: _sharePgn,
                            icon: const Icon(Icons.description_outlined),
                            label: Text(l10n.reportSharePgn),
                          ),
                        ],
                      ),
                    ),
                  ),
                  const SizedBox(height: 24),
                  if (useTwoCol)
                    Row(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Expanded(child: evalSection(chartHeight: 228)),
                        const SizedBox(width: 20),
                        Expanded(child: timingSection()),
                      ],
                    )
                  else ...[
                    evalSection(chartHeight: 200),
                    const SizedBox(height: 24),
                    timingSection(),
                  ],
                  if (clock != null && clock.isTimeControlEnabled) ...[
                    const SizedBox(height: 24),
                    Text(
                      l10n.reportClock,
                      style: tt.titleSmall?.copyWith(color: cs.onSurfaceVariant, fontWeight: FontWeight.w600),
                    ),
                    const SizedBox(height: 8),
                    Card.outlined(
                      child: Padding(
                        padding: const EdgeInsets.all(14),
                        child: Row(
                          children: [
                            Expanded(
                              child: Column(
                                children: [
                                  Text(l10n.colorWhite, style: tt.titleSmall),
                                  Text(
                                    l10n.reportMinutesShort(
                                      '${(clock.whiteTimeMs / 60000).toStringAsFixed(1)}',
                                    ),
                                    style: tt.titleLarge,
                                  ),
                                ],
                              ),
                            ),
                            Expanded(
                              child: Column(
                                children: [
                                  Text(l10n.colorBlack, style: tt.titleSmall),
                                  Text(
                                    l10n.reportMinutesShort(
                                      '${(clock.blackTimeMs / 60000).toStringAsFixed(1)}',
                                    ),
                                    style: tt.titleLarge,
                                  ),
                                ],
                              ),
                            ),
                          ],
                        ),
                      ),
                    ),
                  ],
                  const SizedBox(height: 24),
                  Text(
                    l10n.reportFinalFen,
                    style: tt.titleSmall?.copyWith(color: cs.onSurfaceVariant, fontWeight: FontWeight.w600),
                  ),
                  const SizedBox(height: 8),
                  if (eco != null)
                    Padding(
                      padding: const EdgeInsets.only(bottom: 6),
                      child: Text(eco, style: TextStyle(color: cs.primary, fontWeight: FontWeight.w600)),
                    ),
                  Card.outlined(
                    child: Padding(
                      padding: const EdgeInsets.all(12),
                      child: SelectableText(fen, style: const TextStyle(fontFamily: 'monospace', fontSize: 12)),
                    ),
                  ),
                ],
              ),
            ),
          );
        },
      ),
    );
  }

  String _resultText(AppLocalizations l10n, GameResultState res) {
    if (res == GameResultState.whiteWon) return l10n.resultWhiteWins;
    if (res == GameResultState.blackWon) return l10n.resultBlackWins;
    if (res == GameResultState.draw) return l10n.resultDraw;
    return l10n.resultGameEnded;
  }
}

class _LegendDot extends StatelessWidget {
  const _LegendDot({required this.color, required this.label});
  final Color color;
  final String label;

  @override
  Widget build(BuildContext context) {
    return Row(
      mainAxisSize: MainAxisSize.min,
      children: [
        Container(width: 8, height: 8, decoration: BoxDecoration(color: color, shape: BoxShape.circle)),
        const SizedBox(width: 6),
        Text(label, style: Theme.of(context).textTheme.bodySmall),
      ],
    );
  }
}

/// Compact stat for the exported share card (matches iOS-style chips).
class _ShareStatChip extends StatelessWidget {
  const _ShareStatChip({required this.label, required this.value});

  final String label;
  final String value;

  @override
  Widget build(BuildContext context) {
    final tt = Theme.of(context).textTheme;
    return SizedBox(
      width: 112,
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(
            label,
            style: tt.labelSmall?.copyWith(
              letterSpacing: 0.8,
              color: Theme.of(context).colorScheme.onSurfaceVariant,
              fontWeight: FontWeight.w600,
            ),
          ),
          const SizedBox(height: 4),
          Text(value, style: tt.titleMedium?.copyWith(fontWeight: FontWeight.bold)),
        ],
      ),
    );
  }
}
