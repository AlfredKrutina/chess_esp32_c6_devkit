import 'dart:convert';
import 'dart:io';
import 'dart:math' as math;
import 'dart:ui' as ui;

import 'package:flutter/foundation.dart'
    show defaultTargetPlatform, kIsWeb, TargetPlatform;
import 'package:flutter/material.dart';
import 'package:flutter/rendering.dart';
import 'package:flutter/services.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_image_clipboard/flutter_image_clipboard.dart';
import 'package:image/image.dart' as img;
import 'package:path_provider/path_provider.dart';
import 'package:share_plus/share_plus.dart';

import '../../../core/analysis/move_evaluation.dart';
import '../../../core/models/game_end_report_model.dart';
import '../../../core/models/game_snapshot.dart';
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
import 'board_timeline.dart';
import 'export_frame_capture.dart';
import 'game_export_options.dart';
import 'game_recap_gif_export.dart';
import 'game_share_export_canvas.dart';
import 'timing_charts.dart';

class GameEndReportScreen extends ConsumerStatefulWidget {
  const GameEndReportScreen({super.key});

  @override
  ConsumerState<GameEndReportScreen> createState() => _GameEndReportScreenState();
}

class _GameEndReportScreenState extends ConsumerState<GameEndReportScreen> {
  final GlobalKey _shareBoundaryKey = GlobalKey();
  bool _pngBusy = false;
  /// Which action shows the spinner: true = copy, false = share sheet.
  bool _pngClipboardOp = false;
  bool _gifBusy = false;
  GameExportOptions _exportOpts = const GameExportOptions();

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

  Widget _shareRasterCanvas({
    required AppLocalizations l10n,
    required GameExportOptions opts,
    required GameEndReportModel model,
    required GameSnapshot snapshot,
    required List<({int moveIndex, double eval, MoveGrade? grade})> evalPoints,
    required List<GameEndThinkPlyPoint> think,
    required List<GameEndCumulativePoint> cumulative,
    required bool hasTiming,
    List<List<String>>? boardCellsOverride,
    String? recapCaption,
  }) {
    return GameShareExportCanvas(
      options: opts,
      theme: GameShareExportTheme.adaptive(context, transparent: opts.transparentBackground),
      model: model,
      boardCells: boardCellsOverride ?? boardFromSnapshot(snapshot),
      resultHeadline: _resultText(l10n, model.result),
      localizedReason: _localizedGameEndReason(l10n, model),
      formatDuration: (sec) => _formatDuration(l10n, sec),
      formatAvg: (v) => _formatAvgSec(l10n, v),
      materialCaption:
          l10n.reportMaterialCaptured(model.capturedValueWhite, model.capturedValueBlack),
      brandingLabel: l10n.pgnEventHeader,
      statTimeLabel: l10n.statTime,
      statMovesLabel: l10n.statMoves,
      statWhiteAvgLabel: l10n.statWhiteAvg,
      statBlackAvgLabel: l10n.statBlackAvg,
      recapCaption: recapCaption,
      evalPoints: evalPoints,
      evalCaption: l10n.reportEvalPerspective,
      noEvalCaption: l10n.reportNoStockfishData,
      think: think,
      cumulative: cumulative,
      hasTiming: hasTiming,
      elapsedTitle: l10n.reportElapsedTime,
      elapsedSubtitle: l10n.reportElapsedCaption,
      halfMoveAxis: l10n.reportHalfMoveAxis,
      perMoveTitle: l10n.reportTimePerMove,
      perMoveSubtitle: l10n.reportSecondsSincePrev,
      legendWhite: l10n.colorWhite,
      legendBlack: l10n.colorBlack,
      noTimingCaption: l10n.reportTimingUnavailable,
    );
  }

  Future<void> _shareGifRecap(
    AppLocalizations l10n,
    GameSnapshot snapshot,
    GameEndReportModel model,
    List<({int moveIndex, double eval, MoveGrade? grade})> evalPoints,
    List<GameEndThinkPlyPoint> think,
    List<GameEndCumulativePoint> cumulative,
    bool hasTiming,
  ) async {
    if (_gifBusy) return;
    setState(() => _gifBusy = true);
    try {
      final total = snapshot.history.moves.length;
      final indices = recapFrameIndices(total, 52);
      final logical = const GameExportOptions(aspect: GameExportAspect.story).logicalSize();
      final pkgFrames = <img.Image>[];
      const baseOpts = GameExportOptions(
        aspect: GameExportAspect.story,
        showEvalChart: false,
        showCumulativeChart: false,
        showPerMoveChart: false,
        roundedClip: true,
        transparentBackground: false,
      );
      for (final ply in indices) {
        if (!mounted) return;
        final cells = boardAfterHistoryPrefix(snapshot, ply) ?? boardFromSnapshot(snapshot);
        final caption = total <= 0 ? null : l10n.reportExportRecapMove(ply, total);
        final opts = baseOpts.copyWith(recapMoveIndex: ply, recapMoveTotal: total);
        final wrapped = Theme(
          data: Theme.of(context),
          child: _shareRasterCanvas(
            l10n: l10n,
            opts: opts,
            model: model,
            snapshot: snapshot,
            evalPoints: evalPoints,
            think: think,
            cumulative: cumulative,
            hasTiming: hasTiming,
            boardCellsOverride: cells,
            recapCaption: caption,
          ),
        );
        final uiFrame =
            await captureWidgetOffscreen(context: context, child: wrapped, logicalSize: logical, pixelRatio: 2);
        if (uiFrame == null) continue;
        final pkg = await uiImageToPackageImage(uiFrame);
        uiFrame.dispose();
        if (pkg != null) pkgFrames.add(pkg);
      }
      if (pkgFrames.isEmpty) {
        if (mounted) showGlassSnackBar(context, l10n.reportExportGifFailed('no frames'));
        return;
      }
      final gifBytes = encodeGifAnimation(pkgFrames, durationHundredths: 45);
      if (gifBytes == null || gifBytes.isEmpty) {
        if (mounted) showGlassSnackBar(context, l10n.reportExportGifFailed('encode'));
        return;
      }
      if (!mounted) return;
      final box = context.findRenderObject() as RenderBox?;
      final rect = box != null ? box.localToGlobal(Offset.zero) & box.size : null;
      final tmp = await getTemporaryDirectory();
      final dir = Directory('${tmp.path}/czechmate_share');
      await dir.create(recursive: true);
      final gifFile = File('${dir.path}/czechmate_game_recap.gif');
      await gifFile.writeAsBytes(gifBytes);
      await Share.shareXFiles(
        [XFile(gifFile.path, mimeType: 'image/gif')],
        text: l10n.reportExportGifShareSubject,
        sharePositionOrigin: rect,
      );
    } catch (e) {
      if (mounted) showGlassSnackBar(context, l10n.reportExportGifFailed('$e'));
    } finally {
      if (mounted) setState(() => _gifBusy = false);
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

  Future<Uint8List?> _renderSummaryPngBytes(AppLocalizations l10n) async {
    final snap = ref.read(boardSessionNotifierProvider).snapshot;
    if (snap == null) return null;

    final model = GameEndReportModel.fromSnapshot(snap);
    final moveEval = ref.read(moveEvalNotifierProvider);
    final nMoves = snap.history.moves.length;
    final evalPoints = moveEval.entries
        .where((e) => e.evalWhitePawns != null && e.moveIndex1Based <= nMoves)
        .map((e) => (moveIndex: e.moveIndex1Based, eval: e.evalWhitePawns!, grade: e.grade))
        .toList();
    final think = GameEndReportTiming.thinkPlyPoints(snap.history.moves);
    final cumulative = GameEndReportTiming.cumulativePoints(think);
    final barPoints = think.where((p) => p.secondsFromPrevious != null).toList();
    final hasTiming = barPoints.isNotEmpty || cumulative.isNotEmpty;
    final theme = Theme.of(context);

    await WidgetsBinding.instance.endOfFrame;
    if (!mounted) return null;

    final logical = _exportOpts.logicalSize();
    final wrapped = Theme(
      data: theme,
      child: _shareRasterCanvas(
        l10n: l10n,
        opts: _exportOpts,
        model: model,
        snapshot: snap,
        evalPoints: evalPoints,
        think: think,
        cumulative: cumulative,
        hasTiming: hasTiming,
      ),
    );

    ui.Image? image = await captureWidgetOffscreen(
      context: context,
      child: wrapped,
      logicalSize: logical,
      pixelRatio: 3,
    );

    if (image == null) {
      await WidgetsBinding.instance.endOfFrame;
      final boundary =
          _shareBoundaryKey.currentContext?.findRenderObject() as RenderRepaintBoundary?;
      if (boundary != null) {
        image = await boundary.toImage(pixelRatio: 3.0);
      }
    }

    if (image == null) return null;

    final byteData = await image.toByteData(format: ui.ImageByteFormat.png);
    image.dispose();
    if (byteData == null) {
      throw StateError(l10n.sharePngEncodeError);
    }
    return byteData.buffer.asUint8List();
  }

  Future<void> _sharePngBytes(Uint8List pngBytes, AppLocalizations l10n) async {
    final snap = ref.read(boardSessionNotifierProvider).snapshot;
    if (!mounted) return;
    final box = context.findRenderObject() as RenderBox?;
    final rect = box != null ? box.localToGlobal(Offset.zero) & box.size : null;

    final String meta;
    if (snap == null) {
      meta = l10n.appTitle;
    } else {
      final m = GameEndReportModel.fromSnapshot(snap);
      meta = l10n.shareGameSummaryMeta(
        _resultText(l10n, m.result),
        _formatDuration(l10n, m.durationSec),
        m.totalMoves,
      );
    }

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
  }

  Future<void> _exportAndShare() async {
    final l10n = AppLocalizations.of(context)!;
    if (ref.read(boardSessionNotifierProvider).snapshot == null) return;

    setState(() {
      _pngBusy = true;
      _pngClipboardOp = false;
    });
    try {
      final pngBytes = await _renderSummaryPngBytes(l10n);
      if (!mounted) return;
      if (pngBytes == null) {
        showGlassSnackBar(context, l10n.shareSummaryNotReady);
        return;
      }
      await _sharePngBytes(pngBytes, l10n);
    } catch (e) {
      if (mounted) showGlassSnackBar(context, l10n.shareExportFailed('$e'));
    } finally {
      if (mounted) {
        setState(() {
          _pngBusy = false;
        });
      }
    }
  }

  Future<void> _copySummaryImageToClipboard() async {
    final l10n = AppLocalizations.of(context)!;
    if (ref.read(boardSessionNotifierProvider).snapshot == null) return;

    if (kIsWeb) {
      showGlassSnackBar(context, l10n.reportCopyImageWebHint);
      return;
    }

    final mobile = defaultTargetPlatform == TargetPlatform.iOS ||
        defaultTargetPlatform == TargetPlatform.android;

    setState(() {
      _pngBusy = true;
      _pngClipboardOp = true;
    });
    try {
      final pngBytes = await _renderSummaryPngBytes(l10n);
      if (!mounted) return;
      if (pngBytes == null) {
        showGlassSnackBar(context, l10n.shareSummaryNotReady);
        return;
      }

      if (!mobile) {
        showGlassSnackBar(context, l10n.reportCopyImageDesktopFallback);
        await _sharePngBytes(pngBytes, l10n);
        return;
      }

      final tmp = await getTemporaryDirectory();
      final dir = Directory('${tmp.path}/czechmate_share');
      await dir.create(recursive: true);
      final clipFile = File('${dir.path}/czechmate_clipboard.png');
      await clipFile.writeAsBytes(pngBytes);

      await FlutterImageClipboard().copyImageToClipboard(clipFile);
      HapticFeedback.mediumImpact();
      if (mounted) showGlassSnackBar(context, l10n.reportSummaryCopiedSnack);
    } catch (e) {
      if (mounted) showGlassSnackBar(context, l10n.reportCopyImageFailed);
    } finally {
      if (mounted) {
        setState(() {
          _pngBusy = false;
        });
      }
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

    final exportSize = _exportOpts.logicalSize();
    final exportPreview = ClipRRect(
      borderRadius: BorderRadius.circular(14),
      child: ColoredBox(
        color: _exportOpts.transparentBackground ? Colors.grey.shade900.withValues(alpha: 0.25) : cs.surfaceContainerLow,
        child: AspectRatio(
          aspectRatio: exportSize.width / exportSize.height,
          child: LayoutBuilder(
            builder: (ctx, bc) {
              return FittedBox(
                fit: BoxFit.contain,
                alignment: Alignment.center,
                child: SizedBox(
                  width: exportSize.width,
                  height: exportSize.height,
                  child: RepaintBoundary(
                    key: _shareBoundaryKey,
                    child: Theme(
                      data: Theme.of(context),
                      child: _shareRasterCanvas(
                        l10n: l10n,
                        opts: _exportOpts,
                        model: model,
                        snapshot: snapshot,
                        evalPoints: evalPoints,
                        think: think,
                        cumulative: cumulative,
                        hasTiming: hasTiming,
                      ),
                    ),
                  ),
                ),
              );
            },
          ),
        ),
      ),
    );

    final exportControls = Card.outlined(
      margin: EdgeInsets.zero,
      child: Padding(
        padding: const EdgeInsets.fromLTRB(14, 14, 14, 12),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Text(
              l10n.reportExportAppearanceTitle,
              style: tt.titleSmall?.copyWith(fontWeight: FontWeight.w600),
            ),
            const SizedBox(height: 6),
            Text(
              l10n.reportExportAppearanceSubtitle,
              style: tt.bodySmall?.copyWith(color: cs.onSurfaceVariant),
            ),
            const SizedBox(height: 12),
            Wrap(
              spacing: 8,
              runSpacing: 8,
              children: [
                ActionChip(
                  label: Text(l10n.reportExportPresetFull),
                  onPressed: () => setState(() => _exportOpts = const GameExportOptions()),
                ),
                ActionChip(
                  label: Text(l10n.reportExportPresetMinimal),
                  onPressed: () => setState(() => _exportOpts = GameExportOptions.minimal),
                ),
                ActionChip(
                  label: Text(l10n.reportExportPresetStory),
                  onPressed: () => setState(() => _exportOpts = GameExportOptions.storyHero),
                ),
              ],
            ),
            const SizedBox(height: 12),
            DropdownButtonFormField<GameExportAspect>(
              // Controlled field; `initialValue` does not track preset changes.
              // ignore: deprecated_member_use
              value: _exportOpts.aspect,
              decoration: InputDecoration(
                labelText: l10n.reportExportAspectRatio,
                border: const OutlineInputBorder(),
                isDense: true,
              ),
              items: [
                DropdownMenuItem(
                  value: GameExportAspect.card,
                  child: Text(l10n.reportExportAspectCard),
                ),
                DropdownMenuItem(
                  value: GameExportAspect.square,
                  child: Text(l10n.reportExportAspectSquare),
                ),
                DropdownMenuItem(
                  value: GameExportAspect.story,
                  child: Text(l10n.reportExportAspectStory),
                ),
                DropdownMenuItem(
                  value: GameExportAspect.landscape,
                  child: Text(l10n.reportExportAspectLandscape),
                ),
              ],
              onChanged: (v) {
                if (v != null) setState(() => _exportOpts = _exportOpts.copyWith(aspect: v));
              },
            ),
            SwitchListTile(
              contentPadding: EdgeInsets.zero,
              title: Text(l10n.reportExportTransparentBg),
              subtitle: Text(l10n.reportExportTransparentHint, style: tt.bodySmall),
              value: _exportOpts.transparentBackground,
              onChanged: (v) =>
                  setState(() => _exportOpts = _exportOpts.copyWith(transparentBackground: v)),
            ),
            SwitchListTile(
              contentPadding: EdgeInsets.zero,
              title: Text(l10n.reportExportShowBranding),
              value: _exportOpts.showBranding,
              onChanged: (v) => setState(() => _exportOpts = _exportOpts.copyWith(showBranding: v)),
            ),
            SwitchListTile(
              contentPadding: EdgeInsets.zero,
              title: Text(l10n.reportExportShowStats),
              value: _exportOpts.showStats,
              onChanged: (v) => setState(() => _exportOpts = _exportOpts.copyWith(showStats: v)),
            ),
            SwitchListTile(
              contentPadding: EdgeInsets.zero,
              title: Text(l10n.reportExportShowFinalBoard),
              value: _exportOpts.showFinalBoard,
              onChanged: (v) =>
                  setState(() => _exportOpts = _exportOpts.copyWith(showFinalBoard: v)),
            ),
            SwitchListTile(
              contentPadding: EdgeInsets.zero,
              title: Text(l10n.reportExportFlipBoard),
              value: _exportOpts.flipBoard,
              onChanged: (v) => setState(() => _exportOpts = _exportOpts.copyWith(flipBoard: v)),
            ),
            SwitchListTile(
              contentPadding: EdgeInsets.zero,
              title: Text(l10n.reportExportShowEvalChart),
              value: _exportOpts.showEvalChart,
              onChanged: (v) =>
                  setState(() => _exportOpts = _exportOpts.copyWith(showEvalChart: v)),
            ),
            SwitchListTile(
              contentPadding: EdgeInsets.zero,
              title: Text(l10n.reportExportShowCumulativeChart),
              value: _exportOpts.showCumulativeChart,
              onChanged: (v) =>
                  setState(() => _exportOpts = _exportOpts.copyWith(showCumulativeChart: v)),
            ),
            SwitchListTile(
              contentPadding: EdgeInsets.zero,
              title: Text(l10n.reportExportShowPerMoveChart),
              value: _exportOpts.showPerMoveChart,
              onChanged: (v) =>
                  setState(() => _exportOpts = _exportOpts.copyWith(showPerMoveChart: v)),
            ),
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
                padding: const EdgeInsets.fromLTRB(sidePad, 8, sidePad, 40),
                children: [
                  Text(
                    l10n.reportResultHeading,
                    style: tt.titleSmall?.copyWith(color: cs.onSurfaceVariant, fontWeight: FontWeight.w600),
                  ),
                  const SizedBox(height: 10),
                  SizedBox(height: 340, child: exportPreview),
                  const SizedBox(height: 14),
                  exportControls,
                  const SizedBox(height: 22),
                  Text(
                    l10n.reportShareHeading,
                    style: tt.titleSmall?.copyWith(color: cs.onSurfaceVariant, fontWeight: FontWeight.w600),
                  ),
                  const SizedBox(height: 8),
                  DecoratedBox(
                    decoration: BoxDecoration(
                      borderRadius: BorderRadius.circular(20),
                      gradient: LinearGradient(
                        begin: Alignment.topLeft,
                        end: Alignment.bottomRight,
                        colors: [
                          cs.primary.withValues(alpha: 0.9),
                          cs.tertiary.withValues(alpha: 0.72),
                        ],
                      ),
                      boxShadow: [
                        BoxShadow(
                          color: cs.primary.withValues(alpha: 0.22),
                          blurRadius: 18,
                          offset: const Offset(0, 10),
                        ),
                      ],
                    ),
                    child: Padding(
                      padding: const EdgeInsets.all(2),
                      child: Material(
                        color: cs.surface,
                        elevation: 0,
                        borderRadius: BorderRadius.circular(18),
                        clipBehavior: Clip.antiAlias,
                        child: Padding(
                          padding: const EdgeInsets.fromLTRB(14, 14, 14, 12),
                          child: Column(
                            crossAxisAlignment: CrossAxisAlignment.stretch,
                            children: [
                              Row(
                                crossAxisAlignment: CrossAxisAlignment.start,
                                children: [
                                  Icon(Icons.auto_awesome_rounded, color: cs.primary, size: 24),
                                  const SizedBox(width: 10),
                                  Expanded(
                                    child: Text(
                                      l10n.reportShareExportHint,
                                      style: tt.bodyMedium?.copyWith(
                                        height: 1.35,
                                        color: cs.onSurface,
                                      ),
                                    ),
                                  ),
                                ],
                              ),
                              const SizedBox(height: 16),
                              LayoutBuilder(
                                builder: (ctx, bc) {
                                  final narrow = bc.maxWidth < 360;
                                  final copyBtn = FilledButton.icon(
                                    style: FilledButton.styleFrom(
                                      padding: const EdgeInsets.symmetric(vertical: 13),
                                      backgroundColor: cs.primary,
                                      foregroundColor: cs.onPrimary,
                                    ),
                                    onPressed: _pngBusy ? null : _copySummaryImageToClipboard,
                                    icon: _pngBusy && _pngClipboardOp
                                        ? SizedBox(
                                            width: 18,
                                            height: 18,
                                            child: CircularProgressIndicator(
                                              strokeWidth: 2,
                                              color: cs.onPrimary,
                                            ),
                                          )
                                        : const Icon(Icons.content_paste_go_rounded),
                                    label: Text(
                                      _pngBusy && _pngClipboardOp
                                          ? l10n.reportCopySummaryBusy
                                          : l10n.reportCopySummaryImage,
                                    ),
                                  );
                                  final shareBtn = OutlinedButton.icon(
                                    style: OutlinedButton.styleFrom(
                                      padding: const EdgeInsets.symmetric(vertical: 13),
                                      foregroundColor: cs.primary,
                                      side: BorderSide(color: cs.primary.withValues(alpha: 0.45)),
                                    ),
                                    onPressed: _pngBusy ? null : _exportAndShare,
                                    icon: _pngBusy && !_pngClipboardOp
                                        ? SizedBox(
                                            width: 18,
                                            height: 18,
                                            child: CircularProgressIndicator(
                                              strokeWidth: 2,
                                              color: cs.primary,
                                            ),
                                          )
                                        : const Icon(Icons.ios_share_rounded),
                                    label: Text(
                                      _pngBusy && !_pngClipboardOp
                                          ? l10n.reportSharePreparing
                                          : l10n.reportShareSummaryImage,
                                    ),
                                  );
                                  if (narrow) {
                                    return Column(
                                      crossAxisAlignment: CrossAxisAlignment.stretch,
                                      children: [
                                        copyBtn,
                                        const SizedBox(height: 10),
                                        shareBtn,
                                      ],
                                    );
                                  }
                                  return Row(
                                    crossAxisAlignment: CrossAxisAlignment.start,
                                    children: [
                                      Expanded(child: copyBtn),
                                      const SizedBox(width: 10),
                                      Expanded(child: shareBtn),
                                    ],
                                  );
                                },
                              ),
                              const SizedBox(height: 10),
                            ],
                          ),
                        ),
                      ),
                    ),
                  ),
                  const SizedBox(height: 10),
                  Card.outlined(
                    margin: EdgeInsets.zero,
                    child: Padding(
                      padding: const EdgeInsets.fromLTRB(14, 14, 14, 12),
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.stretch,
                        children: [
                          Text(
                            l10n.reportExportGifSubtitle,
                            style: tt.bodySmall?.copyWith(color: cs.onSurfaceVariant),
                          ),
                          const SizedBox(height: 8),
                          OutlinedButton.icon(
                            onPressed: _gifBusy
                                ? null
                                : () => _shareGifRecap(
                                      l10n,
                                      snapshot,
                                      model,
                                      evalPoints,
                                      think,
                                      cumulative,
                                      hasTiming,
                                    ),
                            icon: _gifBusy
                                ? const SizedBox(
                                    width: 18,
                                    height: 18,
                                    child: CircularProgressIndicator(strokeWidth: 2),
                                  )
                                : const Icon(Icons.animation_outlined),
                            label: Text(_gifBusy ? l10n.reportExportGifBuilding : l10n.reportExportShareGifRecap),
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
                                      (clock.whiteTimeMs / 60000)
                                          .toStringAsFixed(1),
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
                                      (clock.blackTimeMs / 60000)
                                          .toStringAsFixed(1),
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
