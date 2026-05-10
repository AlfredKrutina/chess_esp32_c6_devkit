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

import '../../../app_providers.dart';
import '../../../core/analysis/move_evaluation.dart';
import '../../../core/models/chart_palette.dart';
import '../../../core/models/game_end_report_model.dart';
import '../../../core/models/game_snapshot.dart';
import '../../../l10n/app_localizations.dart';
import '../../../core/utils/fen_from_board.dart';
import '../../../core/utils/user_facing_error_message.dart';
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
import 'game_export_block_id.dart';
import 'game_export_options.dart';
import 'game_recap_gif_export.dart';
import 'chart_palette_sheet.dart';
import 'game_share_export_canvas.dart';
import 'timing_charts.dart';

bool _exportLayoutSame(GameExportOptions a, GameExportOptions b) {
  return a.aspect == b.aspect &&
      a.transparentBackground == b.transparentBackground &&
      a.showBranding == b.showBranding &&
      a.showResult == b.showResult &&
      a.showReason == b.showReason &&
      a.showStats == b.showStats &&
      a.showMaterial == b.showMaterial &&
      a.showFinalBoard == b.showFinalBoard &&
      a.flipBoard == b.flipBoard &&
      a.showEvalChart == b.showEvalChart &&
      a.showCumulativeChart == b.showCumulativeChart &&
      a.showPerMoveChart == b.showPerMoveChart &&
      a.roundedClip == b.roundedClip;
}

/// Selected share layout thumbnail, or null when Advanced options diverge from presets.
int? _exportLayoutThumbIndex(GameExportOptions o) {
  if (_exportLayoutSame(o, GameExportOptions())) return 0;
  if (_exportLayoutSame(o, GameExportOptions.minimal)) return 1;
  if (_exportLayoutSame(o, GameExportOptions.storyHero)) return 2;
  if (_exportLayoutSame(o, GameExportOptions.landscapeWide)) return 3;
  return null;
}

class GameEndReportScreen extends ConsumerStatefulWidget {
  const GameEndReportScreen({super.key});

  @override
  ConsumerState<GameEndReportScreen> createState() =>
      _GameEndReportScreenState();
}

class _GameEndReportScreenState extends ConsumerState<GameEndReportScreen> {
  final GlobalKey _shareBoundaryKey = GlobalKey();
  bool _pngBusy = false;

  /// Which action shows the spinner: true = copy, false = share sheet.
  bool _pngClipboardOp = false;
  bool _gifBusy = false;
  GameExportOptions _exportOpts = GameExportOptions();
  late List<GameExportBlockId> _blockOrder;

  @override
  void initState() {
    super.initState();
    _blockOrder = ref.read(prefsRepositoryProvider).exportShareBlockOrder;
  }

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

  String _localizedGameEndReason(
      AppLocalizations l10n, GameEndReportModel model) {
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

  String _chartPresetLabel(AppLocalizations l10n, ChartPalettePreset p) {
    return switch (p) {
      ChartPalettePreset.theme => l10n.reportChartPaletteTheme,
      ChartPalettePreset.neon => l10n.reportChartPaletteNeon,
      ChartPalettePreset.sunset => l10n.reportChartPaletteSunset,
      ChartPalettePreset.ocean => l10n.reportChartPaletteOcean,
      ChartPalettePreset.custom => l10n.reportChartPaletteCustom,
    };
  }

  Future<void> _applyExportLayoutThumb(int index) async {
    final prefs = ref.read(prefsRepositoryProvider);
    switch (index) {
      case 0:
        final reset = normalizeGameExportBlockOrder(null);
        setState(() {
          _exportOpts = GameExportOptions();
          _blockOrder = reset;
        });
        await prefs.setExportShareBlockOrder(reset);
        break;
      case 1:
        setState(() => _exportOpts = GameExportOptions.minimal);
        break;
      case 2:
        setState(() => _exportOpts = GameExportOptions.storyHero);
        break;
      case 3:
        setState(() => _exportOpts = GameExportOptions.landscapeWide);
        break;
    }
  }

  Widget _exportLayoutThumbnailCard({
    required BuildContext context,
    required ColorScheme cs,
    required bool selected,
    required String title,
    required String subtitle,
    required double shapeW,
    required double shapeH,
    required VoidCallback onTap,
  }) {
    final ar = shapeW / shapeH;
    final tt = Theme.of(context).textTheme;
    return Padding(
      padding: const EdgeInsets.only(right: 10),
      child: InkWell(
        onTap: onTap,
        borderRadius: BorderRadius.circular(14),
        child: Ink(
          width: 108,
          padding: const EdgeInsets.fromLTRB(10, 10, 10, 8),
          decoration: BoxDecoration(
            borderRadius: BorderRadius.circular(14),
            border: Border.all(
              color: selected ? cs.primary : cs.outlineVariant,
              width: selected ? 2 : 1,
            ),
            color: selected
                ? cs.primaryContainer.withValues(alpha: 0.38)
                : cs.surfaceContainerLow,
          ),
          child: LayoutBuilder(
            builder: (context, c) {
              final w = c.maxWidth;
              final naturalPreviewH = w / ar;
              // Řádek má omezenou výšku + přístupnost (velký text); náhled omezit.
              const maxPreviewH = 40.0;
              final previewH = math.min(naturalPreviewH, maxPreviewH);
              return Column(
                mainAxisSize: MainAxisSize.min,
                crossAxisAlignment: CrossAxisAlignment.center,
                children: [
                  SizedBox(
                    width: w,
                    height: previewH,
                    child: FittedBox(
                      fit: BoxFit.contain,
                      alignment: Alignment.center,
                      child: SizedBox(
                        width: w,
                        height: naturalPreviewH,
                        child: DecoratedBox(
                          decoration: BoxDecoration(
                            borderRadius: BorderRadius.circular(8),
                            gradient: LinearGradient(
                              begin: Alignment.topLeft,
                              end: Alignment.bottomRight,
                              colors: [
                                cs.primary.withValues(alpha: 0.55),
                                cs.tertiary.withValues(alpha: 0.42),
                              ],
                            ),
                          ),
                        ),
                      ),
                    ),
                  ),
                  const SizedBox(height: 8),
                  Text(
                    title,
                    maxLines: 1,
                    overflow: TextOverflow.ellipsis,
                    textAlign: TextAlign.center,
                    style: tt.labelLarge?.copyWith(fontWeight: FontWeight.w600),
                  ),
                  Text(
                    subtitle,
                    maxLines: 1,
                    overflow: TextOverflow.ellipsis,
                    textAlign: TextAlign.center,
                    style: tt.labelSmall?.copyWith(color: cs.onSurfaceVariant),
                  ),
                ],
              );
            },
          ),
        ),
      ),
    );
  }

  Future<void> _applyChartPalette(
      AppLocalizations l10n, ChartPalettePreset preset) async {
    final prefs = ref.read(prefsRepositoryProvider);
    await prefs.setChartPalettePreset(preset);
    if (!mounted) return;
    if (preset == ChartPalettePreset.custom) {
      final next = await showChartPaletteCustomizeSheet(
        context,
        initial: prefs.chartPaletteCustom ?? ChartPaletteColors.neon,
        l10n: l10n,
      );
      if (next != null && mounted) {
        await prefs.setChartPaletteCustom(next);
      }
    }
    if (mounted) setState(() {});
  }

  Future<void> _editCustomChartPalette(AppLocalizations l10n) async {
    final prefs = ref.read(prefsRepositoryProvider);
    final next = await showChartPaletteCustomizeSheet(
      context,
      initial: prefs.chartPaletteCustom ?? ChartPaletteColors.neon,
      l10n: l10n,
    );
    if (next != null && mounted) {
      await prefs.setChartPaletteCustom(next);
      setState(() {});
    }
  }

  String _exportBlockLabel(AppLocalizations l10n, GameExportBlockId id) {
    return switch (id) {
      GameExportBlockId.recap => l10n.reportExportBlockRecap,
      GameExportBlockId.branding => l10n.reportExportBlockBranding,
      GameExportBlockId.result => l10n.reportExportBlockResult,
      GameExportBlockId.stats => l10n.reportExportBlockStats,
      GameExportBlockId.material => l10n.reportExportBlockMaterial,
      GameExportBlockId.board => l10n.reportExportBlockBoard,
      GameExportBlockId.eval => l10n.reportExportBlockEval,
      GameExportBlockId.timing => l10n.reportExportBlockTiming,
    };
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
      options: opts.copyWith(blockOrder: _blockOrder),
      theme: GameShareExportTheme.forExport(
        context,
        transparentExport: opts.transparentBackground,
        palette: ref
            .read(prefsRepositoryProvider)
            .resolvedChartPalette(Theme.of(context).colorScheme),
      ),
      model: model,
      boardCells: boardCellsOverride ?? boardFromSnapshot(snapshot),
      resultHeadline: _resultText(l10n, model.result),
      localizedReason: _localizedGameEndReason(l10n, model),
      formatDuration: (sec) => _formatDuration(l10n, sec),
      formatAvg: (v) => _formatAvgSec(l10n, v),
      materialCaption: l10n.reportMaterialCaptured(
          model.capturedValueWhite, model.capturedValueBlack),
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
      final logical =
          GameExportOptions(aspect: GameExportAspect.story).logicalSize();
      final pkgFrames = <img.Image>[];
      final baseOpts = GameExportOptions(
        aspect: GameExportAspect.story,
        showEvalChart: false,
        showCumulativeChart: false,
        showPerMoveChart: false,
        roundedClip: true,
        transparentBackground: false,
        blockOrder: _blockOrder,
      );
      for (final ply in indices) {
        if (!mounted) return;
        final cells = boardAfterHistoryPrefix(snapshot, ply) ??
            boardFromSnapshot(snapshot);
        final caption =
            total <= 0 ? null : l10n.reportExportRecapMove(ply, total);
        final opts =
            baseOpts.copyWith(recapMoveIndex: ply, recapMoveTotal: total);
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
        final uiFrame = await captureWidgetOffscreen(
            context: context,
            child: wrapped,
            logicalSize: logical,
            pixelRatio: 2);
        if (uiFrame == null) continue;
        final pkg = await uiImageToPackageImage(uiFrame);
        uiFrame.dispose();
        if (pkg != null) pkgFrames.add(pkg);
      }
      if (pkgFrames.isEmpty) {
        if (mounted) {
          showAppSnackBar(
            context,
            l10n.reportExportGifFailed('no frames'),
            errorStyle: true,
          );
        }
        return;
      }
      final gifBytes = encodeGifAnimation(pkgFrames, durationHundredths: 45);
      if (gifBytes == null || gifBytes.isEmpty) {
        if (mounted) {
          showAppSnackBar(
            context,
            l10n.reportExportGifFailed('encode'),
            errorStyle: true,
          );
        }
        return;
      }
      if (!mounted) return;
      final box = context.findRenderObject() as RenderBox?;
      final rect =
          box != null ? box.localToGlobal(Offset.zero) & box.size : null;
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
      if (mounted) {
        showAppSnackBar(
          context,
          l10n.reportExportGifFailed(userFacingErrorSummary(l10n, e)),
          errorStyle: true,
        );
      }
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
        .map((e) => (
              moveIndex: e.moveIndex1Based,
              eval: e.evalWhitePawns!,
              grade: e.grade
            ))
        .toList();
    final think = GameEndReportTiming.thinkPlyPoints(snap.history.moves);
    final cumulative = GameEndReportTiming.cumulativePoints(think);
    final barPoints =
        think.where((p) => p.secondsFromPrevious != null).toList();
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
      final boundary = _shareBoundaryKey.currentContext?.findRenderObject()
          as RenderRepaintBoundary?;
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
        showAppSnackBar(
          context,
          l10n.shareSummaryNotReady,
          errorStyle: true,
        );
        return;
      }
      await _sharePngBytes(pngBytes, l10n);
    } catch (e) {
      if (mounted) {
        showAppSnackBar(
          context,
          l10n.shareExportFailed(userFacingErrorSummary(l10n, e)),
          errorStyle: true,
        );
      }
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
      showAppSnackBar(context, l10n.reportCopyImageWebHint);
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
        showAppSnackBar(
          context,
          l10n.shareSummaryNotReady,
          errorStyle: true,
        );
        return;
      }

      if (!mobile) {
        showAppSnackBar(context, l10n.reportCopyImageDesktopFallback);
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
      if (mounted) showAppSnackBar(context, l10n.reportSummaryCopiedSnack);
    } catch (e) {
      if (mounted) {
        showAppSnackBar(
          context,
          l10n.reportCopyImageFailed,
          errorStyle: true,
        );
      }
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
        .map((e) => (
              moveIndex: e.moveIndex1Based,
              eval: e.evalWhitePawns!,
              grade: e.grade
            ))
        .toList();

    final think = GameEndReportTiming.thinkPlyPoints(snapshot.history.moves);
    final cumulative = GameEndReportTiming.cumulativePoints(think);
    final barPoints =
        think.where((p) => p.secondsFromPrevious != null).toList();
    final hasTiming = barPoints.isNotEmpty || cumulative.isNotEmpty;
    final clock = snapshot.clock;
    final cs = Theme.of(context).colorScheme;
    final tt = Theme.of(context).textTheme;
    final chartPalette =
        ref.read(prefsRepositoryProvider).resolvedChartPalette(cs);
    final chartPresetNow = ref.read(prefsRepositoryProvider).chartPalettePreset;

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
            EvalLineChart(
              points: evalPoints,
              height: chartHeight,
              accentColor: chartPalette.evalLine,
            ),
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
                        style: tt.titleSmall
                            ?.copyWith(fontWeight: FontWeight.w600)),
                    const SizedBox(height: 4),
                    Text(
                      l10n.reportElapsedCaption,
                      style: tt.bodySmall?.copyWith(color: cs.onSurfaceVariant),
                    ),
                    const SizedBox(height: 10),
                    CumulativePlayedTimeChart(
                      points: cumulative,
                      accent: chartPalette.cumulative,
                    ),
                    Text(
                      l10n.reportHalfMoveAxis,
                      style:
                          tt.labelSmall?.copyWith(color: cs.onSurfaceVariant),
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
                        style: tt.titleSmall
                            ?.copyWith(fontWeight: FontWeight.w600)),
                    const SizedBox(height: 4),
                    Text(
                      l10n.reportSecondsSincePrev,
                      style: tt.bodySmall?.copyWith(color: cs.onSurfaceVariant),
                    ),
                    const SizedBox(height: 10),
                    TimePerMoveBarChart(
                      points: think,
                      whiteColor: chartPalette.barWhite,
                      blackColor: chartPalette.barBlack,
                    ),
                    const SizedBox(height: 8),
                    Wrap(
                      spacing: 20,
                      children: [
                        _LegendDot(
                            color:
                                chartPalette.barWhite.withValues(alpha: 0.92),
                            label: l10n.colorWhite),
                        _LegendDot(
                            color:
                                chartPalette.barBlack.withValues(alpha: 0.88),
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
        color: _exportOpts.transparentBackground
            ? Colors.grey.shade900.withValues(alpha: 0.25)
            : cs.surfaceContainerLow,
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
            const SizedBox(height: 8),
            Text(
              l10n.reportExportStylePickHint,
              style: tt.bodySmall?.copyWith(color: cs.onSurfaceVariant),
            ),
            const SizedBox(height: 10),
            SizedBox(
              height: 172,
              child: ListView(
                scrollDirection: Axis.horizontal,
                children: [
                  _exportLayoutThumbnailCard(
                    context: context,
                    cs: cs,
                    selected: _exportLayoutThumbIndex(_exportOpts) == 0,
                    title: l10n.reportExportPresetFull,
                    subtitle: l10n.reportExportAspectCard,
                    shapeW: 38,
                    shapeH: 68,
                    onTap: () => _applyExportLayoutThumb(0),
                  ),
                  _exportLayoutThumbnailCard(
                    context: context,
                    cs: cs,
                    selected: _exportLayoutThumbIndex(_exportOpts) == 1,
                    title: l10n.reportExportPresetMinimal,
                    subtitle: l10n.reportExportAspectSquare,
                    shapeW: 52,
                    shapeH: 52,
                    onTap: () => _applyExportLayoutThumb(1),
                  ),
                  _exportLayoutThumbnailCard(
                    context: context,
                    cs: cs,
                    selected: _exportLayoutThumbIndex(_exportOpts) == 2,
                    title: l10n.reportExportPresetStory,
                    subtitle: l10n.reportExportAspectStory,
                    shapeW: 36,
                    shapeH: 64,
                    onTap: () => _applyExportLayoutThumb(2),
                  ),
                  _exportLayoutThumbnailCard(
                    context: context,
                    cs: cs,
                    selected: _exportLayoutThumbIndex(_exportOpts) == 3,
                    title: l10n.reportExportThumbWideLabel,
                    subtitle: l10n.reportExportThumbWideAspect,
                    shapeW: 72,
                    shapeH: 40,
                    onTap: () => _applyExportLayoutThumb(3),
                  ),
                ],
              ),
            ),
            const SizedBox(height: 12),
            Text(
              l10n.reportChartPaletteTitle,
              style: tt.titleSmall?.copyWith(fontWeight: FontWeight.w600),
            ),
            const SizedBox(height: 4),
            Text(
              l10n.reportChartPaletteSubtitle,
              style: tt.bodySmall?.copyWith(color: cs.onSurfaceVariant),
            ),
            const SizedBox(height: 6),
            Text(
              l10n.reportChartPaletteChoiceHint,
              style: tt.bodySmall?.copyWith(color: cs.onSurfaceVariant),
            ),
            const SizedBox(height: 8),
            Wrap(
              spacing: 6,
              runSpacing: 6,
              children: [
                for (final p in ChartPalettePreset.values)
                  ChoiceChip(
                    label: Text(_chartPresetLabel(l10n, p)),
                    selected: chartPresetNow == p,
                    onSelected: (sel) async {
                      if (!sel) return;
                      await _applyChartPalette(l10n, p);
                    },
                  ),
              ],
            ),
            if (chartPresetNow == ChartPalettePreset.custom)
              Align(
                alignment: Alignment.centerLeft,
                child: TextButton.icon(
                  onPressed: () => _editCustomChartPalette(l10n),
                  icon: const Icon(Icons.palette_outlined, size: 18),
                  label: Text(l10n.reportChartPaletteEditCustom),
                ),
              ),
            Theme(
              data:
                  Theme.of(context).copyWith(dividerColor: Colors.transparent),
              child: ExpansionTile(
                maintainState: true,
                initiallyExpanded: false,
                tilePadding: EdgeInsets.zero,
                title: Text(
                  l10n.reportExportAdvancedTitle,
                  style: tt.titleSmall?.copyWith(fontWeight: FontWeight.w600),
                ),
                subtitle: Text(
                  l10n.reportExportAdvancedSubtitle,
                  style: tt.bodySmall?.copyWith(color: cs.onSurfaceVariant),
                ),
                children: [
                  Padding(
                    padding: const EdgeInsets.only(bottom: 8),
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.stretch,
                      children: [
                        SwitchListTile(
                          contentPadding: EdgeInsets.zero,
                          title: Text(l10n.reportExportTransparentBg),
                          subtitle: Text(l10n.reportExportTransparentHint,
                              style: tt.bodySmall),
                          value: _exportOpts.transparentBackground,
                          onChanged: (v) => setState(() => _exportOpts =
                              _exportOpts.copyWith(transparentBackground: v)),
                        ),
                        SwitchListTile(
                          contentPadding: EdgeInsets.zero,
                          title: Text(l10n.reportExportShowBranding),
                          value: _exportOpts.showBranding,
                          onChanged: (v) => setState(() => _exportOpts =
                              _exportOpts.copyWith(showBranding: v)),
                        ),
                        SwitchListTile(
                          contentPadding: EdgeInsets.zero,
                          title: Text(l10n.reportExportShowStats),
                          value: _exportOpts.showStats,
                          onChanged: (v) => setState(() =>
                              _exportOpts = _exportOpts.copyWith(showStats: v)),
                        ),
                        SwitchListTile(
                          contentPadding: EdgeInsets.zero,
                          title: Text(l10n.reportExportShowFinalBoard),
                          value: _exportOpts.showFinalBoard,
                          onChanged: (v) => setState(() => _exportOpts =
                              _exportOpts.copyWith(showFinalBoard: v)),
                        ),
                        SwitchListTile(
                          contentPadding: EdgeInsets.zero,
                          title: Text(l10n.reportExportFlipBoard),
                          value: _exportOpts.flipBoard,
                          onChanged: (v) => setState(() =>
                              _exportOpts = _exportOpts.copyWith(flipBoard: v)),
                        ),
                        SwitchListTile(
                          contentPadding: EdgeInsets.zero,
                          title: Text(l10n.reportExportShowEvalChart),
                          value: _exportOpts.showEvalChart,
                          onChanged: (v) => setState(() => _exportOpts =
                              _exportOpts.copyWith(showEvalChart: v)),
                        ),
                        SwitchListTile(
                          contentPadding: EdgeInsets.zero,
                          title: Text(l10n.reportExportShowCumulativeChart),
                          value: _exportOpts.showCumulativeChart,
                          onChanged: (v) => setState(() => _exportOpts =
                              _exportOpts.copyWith(showCumulativeChart: v)),
                        ),
                        SwitchListTile(
                          contentPadding: EdgeInsets.zero,
                          title: Text(l10n.reportExportShowPerMoveChart),
                          value: _exportOpts.showPerMoveChart,
                          onChanged: (v) => setState(() => _exportOpts =
                              _exportOpts.copyWith(showPerMoveChart: v)),
                        ),
                        Theme(
                          data: Theme.of(context)
                              .copyWith(dividerColor: Colors.transparent),
                          child: ExpansionTile(
                            maintainState: true,
                            tilePadding: EdgeInsets.zero,
                            title: Text(
                              l10n.reportExportSectionOrderTitle,
                              style: tt.titleSmall
                                  ?.copyWith(fontWeight: FontWeight.w600),
                            ),
                            children: [
                              SizedBox(
                                height:
                                    math.min(380, _blockOrder.length * 54.0),
                                child: ReorderableListView(
                                  padding: EdgeInsets.zero,
                                  buildDefaultDragHandles: false,
                                  onReorder: (oldIndex, newIndex) async {
                                    setState(() {
                                      var ni = newIndex;
                                      if (ni > oldIndex) ni -= 1;
                                      final id = _blockOrder.removeAt(oldIndex);
                                      _blockOrder.insert(ni, id);
                                    });
                                    await ref
                                        .read(prefsRepositoryProvider)
                                        .setExportShareBlockOrder(_blockOrder);
                                  },
                                  children: [
                                    for (var i = 0; i < _blockOrder.length; i++)
                                      ListTile(
                                        key: ValueKey(
                                            '${_blockOrder[i].name}_$i'),
                                        contentPadding:
                                            const EdgeInsets.symmetric(
                                                horizontal: 0, vertical: 2),
                                        leading: ReorderableDragStartListener(
                                          index: i,
                                          child: Icon(Icons.drag_handle,
                                              color: cs.onSurfaceVariant),
                                        ),
                                        title: Text(_exportBlockLabel(
                                            l10n, _blockOrder[i])),
                                      ),
                                  ],
                                ),
                              ),
                            ],
                          ),
                        ),
                      ],
                    ),
                  ),
                ],
              ),
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
          final avail =
              (constraints.maxWidth - sidePad * 2).clamp(0.0, double.infinity);
          final evalHasChart = evalPoints.isNotEmpty;
          final useTwoCol = avail >= 920 && evalHasChart && hasTiming;
          final maxW =
              useTwoCol ? math.min(avail, 1080.0) : math.min(avail, 640.0);

          return Align(
            alignment: Alignment.topCenter,
            child: ConstrainedBox(
              constraints: BoxConstraints(maxWidth: maxW),
              child: ListView(
                padding: const EdgeInsets.fromLTRB(sidePad, 8, sidePad, 40),
                children: [
                  Text(
                    l10n.reportResultHeading,
                    style: tt.titleSmall?.copyWith(
                        color: cs.onSurfaceVariant,
                        fontWeight: FontWeight.w600),
                  ),
                  const SizedBox(height: 10),
                  SizedBox(height: 340, child: exportPreview),
                  const SizedBox(height: 14),
                  exportControls,
                  const SizedBox(height: 22),
                  Text(
                    l10n.reportShareHeading,
                    style: tt.titleSmall?.copyWith(
                        color: cs.onSurfaceVariant,
                        fontWeight: FontWeight.w600),
                  ),
                  const SizedBox(height: 8),
                  LayoutBuilder(
                    builder: (ctx, bc) {
                      final narrow = bc.maxWidth < 340;
                      final copyChild = _pngBusy && _pngClipboardOp
                          ? SizedBox(
                              width: 16,
                              height: 16,
                              child: CircularProgressIndicator(
                                strokeWidth: 2,
                                color: cs.onPrimary,
                              ),
                            )
                          : Icon(Icons.copy_rounded,
                              size: 18, color: cs.onPrimary);
                      final shareChild = _pngBusy && !_pngClipboardOp
                          ? SizedBox(
                              width: 16,
                              height: 16,
                              child: CircularProgressIndicator(
                                strokeWidth: 2,
                                color: cs.primary,
                              ),
                            )
                          : Icon(Icons.ios_share_rounded,
                              size: 18, color: cs.primary);
                      final copyBtn = FilledButton(
                        style: FilledButton.styleFrom(
                          padding: const EdgeInsets.symmetric(
                              horizontal: 10, vertical: 10),
                          visualDensity: VisualDensity.compact,
                        ),
                        onPressed:
                            _pngBusy ? null : _copySummaryImageToClipboard,
                        child: Row(
                          mainAxisAlignment: MainAxisAlignment.center,
                          mainAxisSize: MainAxisSize.min,
                          children: [
                            copyChild,
                            const SizedBox(width: 6),
                            Flexible(
                              child: Text(
                                _pngBusy && _pngClipboardOp
                                    ? l10n.reportCopySummaryBusy
                                    : l10n.reportCopySummaryImage,
                                maxLines: 1,
                                overflow: TextOverflow.ellipsis,
                                style: const TextStyle(fontSize: 13),
                              ),
                            ),
                          ],
                        ),
                      );
                      final shareBtn = OutlinedButton(
                        style: OutlinedButton.styleFrom(
                          padding: const EdgeInsets.symmetric(
                              horizontal: 10, vertical: 10),
                          visualDensity: VisualDensity.compact,
                          side: BorderSide(
                              color: cs.primary.withValues(alpha: 0.45)),
                        ),
                        onPressed: _pngBusy ? null : _exportAndShare,
                        child: Row(
                          mainAxisAlignment: MainAxisAlignment.center,
                          mainAxisSize: MainAxisSize.min,
                          children: [
                            shareChild,
                            const SizedBox(width: 6),
                            Flexible(
                              child: Text(
                                _pngBusy && !_pngClipboardOp
                                    ? l10n.reportSharePreparing
                                    : l10n.reportShareSummaryImage,
                                maxLines: 1,
                                overflow: TextOverflow.ellipsis,
                                style:
                                    TextStyle(fontSize: 13, color: cs.primary),
                              ),
                            ),
                          ],
                        ),
                      );
                      if (narrow) {
                        return Column(
                          crossAxisAlignment: CrossAxisAlignment.stretch,
                          children: [
                            copyBtn,
                            const SizedBox(height: 8),
                            shareBtn,
                          ],
                        );
                      }
                      return Row(
                        crossAxisAlignment: CrossAxisAlignment.stretch,
                        children: [
                          Expanded(child: copyBtn),
                          const SizedBox(width: 8),
                          Expanded(child: shareBtn),
                        ],
                      );
                    },
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
                            style: tt.bodySmall
                                ?.copyWith(color: cs.onSurfaceVariant),
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
                                    child: CircularProgressIndicator(
                                        strokeWidth: 2),
                                  )
                                : const Icon(Icons.animation_outlined),
                            label: Text(_gifBusy
                                ? l10n.reportExportGifBuilding
                                : l10n.reportExportShareGifRecap),
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
                      style: tt.titleSmall?.copyWith(
                          color: cs.onSurfaceVariant,
                          fontWeight: FontWeight.w600),
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
                    style: tt.titleSmall?.copyWith(
                        color: cs.onSurfaceVariant,
                        fontWeight: FontWeight.w600),
                  ),
                  const SizedBox(height: 8),
                  if (eco != null)
                    Padding(
                      padding: const EdgeInsets.only(bottom: 6),
                      child: Text(eco,
                          style: TextStyle(
                              color: cs.primary, fontWeight: FontWeight.w600)),
                    ),
                  Card.outlined(
                    child: Padding(
                      padding: const EdgeInsets.all(12),
                      child: SelectableText(fen,
                          style: const TextStyle(
                              fontFamily: 'monospace', fontSize: 12)),
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
        Container(
            width: 8,
            height: 8,
            decoration: BoxDecoration(color: color, shape: BoxShape.circle)),
        const SizedBox(width: 6),
        Text(label, style: Theme.of(context).textTheme.bodySmall),
      ],
    );
  }
}

/// Compact stat for the exported share card (matches iOS-style chips).
