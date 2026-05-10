import 'dart:math' as math;

import 'package:flutter/material.dart';

import '../../../core/models/chart_palette.dart';
import '../../../core/models/game_end_report_model.dart';
import '../../../core/utils/game_end_report_timing.dart';
import '../../../core/analysis/move_evaluation.dart';
import '../../analysis/eval_line_chart.dart';
import 'export_board_preview.dart';
import 'game_export_block_id.dart';
import 'game_export_options.dart';
import 'timing_charts.dart';

/// Colors for export raster (supports transparent outer background only).
@immutable
class GameShareExportTheme {
  const GameShareExportTheme({
    required this.foreground,
    required this.muted,
    required this.accent,
    required this.evalLine,
    required this.cumulativeLine,
    required this.chartWhite,
    required this.chartBlack,
    required this.boardLight,
    required this.boardDark,
    required this.divider,
    this.chartPlotBackground,
    this.chartAxisMuted,
  });

  final Color foreground;
  final Color muted;

  /// Branding / recap capsule — aligned with eval line.
  final Color accent;
  final Color evalLine;
  final Color cumulativeLine;
  final Color chartWhite;
  final Color chartBlack;

  /// Always opaque — transparent PNG must not wash out squares.
  final Color boardLight;
  final Color boardDark;
  final Color divider;

  /// Opaque panel behind charts when the outer export background is transparent.
  final Color? chartPlotBackground;
  final Color? chartAxisMuted;

  factory GameShareExportTheme.forExport(
    BuildContext context, {
    required bool transparentExport,
    required ChartPaletteColors palette,
  }) {
    final cs = Theme.of(context).colorScheme;
    const boardLight = Color(0xFFE8D4B8);
    const boardDark = Color(0xFFB58863);

    if (transparentExport) {
      return GameShareExportTheme(
        foreground: Colors.white,
        muted: Colors.white.withValues(alpha: 0.78),
        accent: palette.evalLine,
        evalLine: palette.evalLine,
        cumulativeLine: palette.cumulative,
        chartWhite: palette.barWhite,
        chartBlack: palette.barBlack,
        boardLight: boardLight,
        boardDark: boardDark,
        divider: Colors.white.withValues(alpha: 0.35),
        chartPlotBackground: const Color(0xFF2C2C2C),
        chartAxisMuted: Colors.white.withValues(alpha: 0.38),
      );
    }
    return GameShareExportTheme(
      foreground: cs.onSurface,
      muted: cs.onSurfaceVariant,
      accent: palette.evalLine,
      evalLine: palette.evalLine,
      cumulativeLine: palette.cumulative,
      chartWhite: palette.barWhite,
      chartBlack: palette.barBlack,
      boardLight: boardLight,
      boardDark: boardDark,
      divider: cs.outlineVariant.withValues(alpha: 0.55),
    );
  }
}

/// Single raster layout for PNG/GIF export — content driven by [GameExportOptions].
class GameShareExportCanvas extends StatelessWidget {
  const GameShareExportCanvas({
    super.key,
    required this.options,
    required this.theme,
    required this.model,
    required this.boardCells,
    required this.resultHeadline,
    required this.localizedReason,
    required this.formatDuration,
    required this.formatAvg,
    required this.materialCaption,
    required this.brandingLabel,
    required this.statTimeLabel,
    required this.statMovesLabel,
    required this.statWhiteAvgLabel,
    required this.statBlackAvgLabel,
    required this.recapCaption,
    required this.evalPoints,
    required this.evalCaption,
    required this.noEvalCaption,
    required this.think,
    required this.cumulative,
    required this.hasTiming,
    required this.elapsedTitle,
    required this.elapsedSubtitle,
    required this.halfMoveAxis,
    required this.perMoveTitle,
    required this.perMoveSubtitle,
    required this.legendWhite,
    required this.legendBlack,
    required this.noTimingCaption,
  });

  final GameExportOptions options;
  final GameShareExportTheme theme;
  final GameEndReportModel model;
  final List<List<String>> boardCells;

  final String resultHeadline;
  final String localizedReason;
  final String Function(int sec) formatDuration;
  final String Function(double sec) formatAvg;
  final String materialCaption;
  final String brandingLabel;
  final String statTimeLabel;
  final String statMovesLabel;
  final String statWhiteAvgLabel;
  final String statBlackAvgLabel;
  final String? recapCaption;

  final List<({int moveIndex, double eval, MoveGrade? grade})> evalPoints;
  final String evalCaption;
  final String noEvalCaption;

  final List<GameEndThinkPlyPoint> think;
  final List<GameEndCumulativePoint> cumulative;
  final bool hasTiming;
  final String elapsedTitle;
  final String elapsedSubtitle;
  final String halfMoveAxis;
  final String perMoveTitle;
  final String perMoveSubtitle;
  final String legendWhite;
  final String legendBlack;
  final String noTimingCaption;

  TextStyle get _titleSm => TextStyle(
        fontSize: 13,
        fontWeight: FontWeight.w700,
        letterSpacing: 0.6,
        color: theme.muted,
      );

  @override
  Widget build(BuildContext context) {
    final size = options.logicalSize();
    final radius = options.roundedClip ? 16.0 : 0.0;

    Widget decorated({required Widget child}) {
      if (options.transparentBackground) {
        return ClipRRect(
          borderRadius: BorderRadius.circular(radius),
          child: SizedBox(width: size.width, height: size.height, child: child),
        );
      }
      return ClipRRect(
        borderRadius: BorderRadius.circular(radius),
        child: Container(
          width: size.width,
          height: size.height,
          decoration: BoxDecoration(
            gradient: LinearGradient(
              begin: Alignment.topLeft,
              end: Alignment.bottomRight,
              colors: [
                Theme.of(context).colorScheme.surfaceContainerHigh,
                Theme.of(context)
                    .colorScheme
                    .surfaceContainerHighest
                    .withValues(alpha: 0.88),
              ],
            ),
            border: Border.all(color: theme.divider),
          ),
          child: child,
        ),
      );
    }

    final content = Padding(
      padding: const EdgeInsets.fromLTRB(14, 14, 14, 12),
      child: LayoutBuilder(
        builder: (context, constraints) {
          return _scaledExportBody(constraints.maxWidth, constraints.maxHeight);
        },
      ),
    );

    return decorated(child: content);
  }

  Widget _scaledExportBody(double maxW, double maxH) {
    final core = _composeLayout(maxW, maxH);
    return FittedBox(
      fit: BoxFit.scaleDown,
      alignment: Alignment.topCenter,
      child: SizedBox(width: maxW, child: core),
    );
  }

  _ChartHeights _chartHeights(double maxH) {
    var n = 0;
    if (options.showEvalChart) n++;
    if (options.showCumulativeChart && hasTiming) n++;
    if (options.showPerMoveChart && hasTiming) n++;
    if (n == 0) {
      return const _ChartHeights(
          eval: 80, cumulative: 120, barOuter: 130, barPlot: 118);
    }
    final slice = (maxH * 0.36 / n).clamp(72.0, 118.0);
    final barPlot = (slice * 0.92).clamp(68.0, 112.0);
    return _ChartHeights(
      eval: slice,
      cumulative: slice,
      barOuter: slice + 18,
      barPlot: barPlot,
    );
  }

  Widget _composeLayout(double maxW, double maxH) {
    final ch = _chartHeights(maxH);
    final order = options.blockOrder;
    final boardIdx = order.indexOf(GameExportBlockId.board);
    final useMagazine = options.showFinalBoard &&
        boardIdx >= 0 &&
        maxW >= 292 &&
        options.aspect != GameExportAspect.square;

    if (!useMagazine) {
      final kids = <Widget>[];
      for (final id in order) {
        final w = _section(id, ch);
        if (w != null) kids.add(w);
      }
      return Column(
        mainAxisSize: MainAxisSize.min,
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: _withGaps(kids),
      );
    }

    final beforeIds = order.sublist(0, boardIdx);
    final afterIds = order.sublist(boardIdx + 1);

    final bandH = math.min(maxW * 0.44, maxH * 0.5).clamp(104.0, 248.0);

    final beforeKids = <Widget>[];
    for (final id in beforeIds) {
      final w = _section(id, ch);
      if (w != null) beforeKids.add(w);
    }
    final afterKids = <Widget>[];
    for (final id in afterIds) {
      final w = _section(id, ch);
      if (w != null) afterKids.add(w);
    }

    final boardWidget = _section(GameExportBlockId.board, ch);

    return Column(
      mainAxisSize: MainAxisSize.min,
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        Row(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Expanded(
              child: SizedBox(
                height: bandH,
                child: FittedBox(
                  alignment: Alignment.topLeft,
                  fit: BoxFit.scaleDown,
                  child: SizedBox(
                    width: maxW * 0.54,
                    child: Column(
                      mainAxisSize: MainAxisSize.min,
                      crossAxisAlignment: CrossAxisAlignment.stretch,
                      children: _withGaps(beforeKids),
                    ),
                  ),
                ),
              ),
            ),
            if (boardWidget != null) ...[
              const SizedBox(width: 8),
              SizedBox(width: bandH, height: bandH, child: boardWidget),
            ],
          ],
        ),
        if (afterKids.isNotEmpty) ...[
          const SizedBox(height: 8),
          ..._withGaps(afterKids),
        ],
      ],
    );
  }

  List<Widget> _withGaps(List<Widget> items) {
    if (items.isEmpty) return items;
    final out = <Widget>[items.first];
    for (var i = 1; i < items.length; i++) {
      out.add(const SizedBox(height: 8));
      out.add(items[i]);
    }
    return out;
  }

  Widget? _section(GameExportBlockId id, _ChartHeights ch) {
    switch (id) {
      case GameExportBlockId.recap:
        return _recapOrNull();
      case GameExportBlockId.branding:
        if (!options.showBranding) return null;
        return _header();
      case GameExportBlockId.result:
        if (!options.showResult && !options.showReason) return null;
        return _resultBlock();
      case GameExportBlockId.stats:
        if (!options.showStats) return null;
        return _stats();
      case GameExportBlockId.material:
        if (!options.showMaterial ||
            (model.capturedValueWhite <= 0 && model.capturedValueBlack <= 0)) {
          return null;
        }
        return Text(materialCaption,
            style: TextStyle(fontSize: 11, color: theme.muted));
      case GameExportBlockId.board:
        return _boardOrNull();
      case GameExportBlockId.eval:
        if (!options.showEvalChart) return null;
        return _eval(ch.eval);
      case GameExportBlockId.timing:
        if (!options.showCumulativeChart && !options.showPerMoveChart) {
          return null;
        }
        return _timing(ch.cumulative, ch.barOuter, ch.barPlot);
    }
  }

  Widget? _recapOrNull() {
    if (recapCaption == null || recapCaption!.isEmpty) return null;
    return Padding(
      padding: const EdgeInsets.only(bottom: 2),
      child: Container(
        padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
        decoration: BoxDecoration(
          color: theme.accent.withValues(alpha: 0.22),
          borderRadius: BorderRadius.circular(8),
          border: Border.all(color: theme.accent.withValues(alpha: 0.45)),
        ),
        child: Text(
          recapCaption!,
          style: TextStyle(
              color: theme.foreground,
              fontWeight: FontWeight.w600,
              fontSize: 13),
        ),
      ),
    );
  }

  Widget _header() {
    if (!options.showBranding) return const SizedBox.shrink();
    return Row(
      children: [
        ClipRRect(
          borderRadius: BorderRadius.circular(5),
          child: Image.asset(
            'assets/app_icon.png',
            width: 22,
            height: 22,
            fit: BoxFit.cover,
            filterQuality: FilterQuality.high,
          ),
        ),
        const SizedBox(width: 8),
        Text(
          brandingLabel,
          style: TextStyle(
            letterSpacing: 1.1,
            fontWeight: FontWeight.w800,
            fontSize: 13,
            color: theme.accent,
          ),
        ),
      ],
    );
  }

  Widget _resultBlock() {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        if (options.showResult)
          Text(
            resultHeadline,
            style: TextStyle(
              fontSize: options.aspect == GameExportAspect.square ? 22 : 28,
              fontWeight: FontWeight.bold,
              color: theme.foreground,
              shadows: options.transparentBackground
                  ? const [Shadow(blurRadius: 10, color: Colors.black54)]
                  : null,
            ),
          ),
        if (options.showReason) ...[
          const SizedBox(height: 4),
          Text(
            localizedReason,
            style: TextStyle(fontSize: 14, color: theme.muted),
          ),
        ],
      ],
    );
  }

  Widget _stats() {
    if (!options.showStats) return const SizedBox.shrink();
    return Wrap(
      spacing: 18,
      runSpacing: 10,
      children: [
        _miniStat(statTimeLabel, formatDuration(model.durationSec)),
        _miniStat(statMovesLabel, '${model.totalMoves}'),
        _miniStat(statWhiteAvgLabel, formatAvg(model.whiteAvgMoveSec)),
        _miniStat(statBlackAvgLabel, formatAvg(model.blackAvgMoveSec)),
      ],
    );
  }

  Widget _miniStat(String label, String value) {
    return SizedBox(
      width: 100,
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(label, style: _titleSm),
          const SizedBox(height: 2),
          Text(
            value,
            style: TextStyle(
              fontSize: 17,
              fontWeight: FontWeight.bold,
              color: theme.foreground,
            ),
          ),
        ],
      ),
    );
  }

  Widget? _boardOrNull() {
    if (!options.showFinalBoard) return null;
    return ExportBoardPreview(
      cells: boardCells,
      flip: options.flipBoard,
      lightSquare: theme.boardLight,
      darkSquare: theme.boardDark,
      padding: 4,
    );
  }

  Widget _eval(double height) {
    if (!options.showEvalChart) return const SizedBox.shrink();
    if (evalPoints.isEmpty) {
      return Text(noEvalCaption,
          style: TextStyle(fontSize: 11, color: theme.muted));
    }
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(evalCaption, style: _titleSm),
        const SizedBox(height: 4),
        EvalLineChart(
          points: evalPoints,
          height: height,
          accentColor: theme.evalLine,
          plotBackgroundColor: theme.chartPlotBackground,
          axisColor: theme.chartAxisMuted,
        ),
      ],
    );
  }

  Widget _timing(double cumulativeH, double barOuter, double barPlot) {
    if (!options.showCumulativeChart && !options.showPerMoveChart) {
      return const SizedBox.shrink();
    }
    if (!hasTiming) {
      return Text(noTimingCaption,
          style: TextStyle(fontSize: 11, color: theme.muted));
    }
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        if (options.showCumulativeChart) ...[
          Text(elapsedTitle, style: _titleSm.copyWith(fontSize: 12)),
          Text(elapsedSubtitle,
              style: TextStyle(fontSize: 10, color: theme.muted)),
          const SizedBox(height: 4),
          CumulativePlayedTimeChart(
            points: cumulative,
            accent: theme.cumulativeLine,
            height: cumulativeH,
            backgroundColor: theme.chartPlotBackground,
            axisColor: theme.chartAxisMuted,
          ),
          Text(halfMoveAxis, style: TextStyle(fontSize: 9, color: theme.muted)),
          const SizedBox(height: 8),
        ],
        if (options.showPerMoveChart) ...[
          Text(perMoveTitle, style: _titleSm.copyWith(fontSize: 12)),
          Text(perMoveSubtitle,
              style: TextStyle(fontSize: 10, color: theme.muted)),
          const SizedBox(height: 4),
          TimePerMoveBarChart(
            points: think,
            whiteColor: theme.chartWhite,
            blackColor: theme.chartBlack,
            outerHeight: barOuter,
            plotHeight: barPlot,
            backgroundColor: theme.chartPlotBackground,
          ),
          const SizedBox(height: 4),
          Row(
            children: [
              _dot(theme.chartWhite, legendWhite),
              const SizedBox(width: 14),
              _dot(theme.chartBlack, legendBlack),
            ],
          ),
        ],
      ],
    );
  }

  Widget _dot(Color c, String label) {
    return Row(
      mainAxisSize: MainAxisSize.min,
      children: [
        Container(
          width: 7,
          height: 7,
          decoration: BoxDecoration(color: c, shape: BoxShape.circle),
        ),
        const SizedBox(width: 5),
        Text(label, style: TextStyle(fontSize: 10, color: theme.muted)),
      ],
    );
  }
}

class _ChartHeights {
  const _ChartHeights({
    required this.eval,
    required this.cumulative,
    required this.barOuter,
    required this.barPlot,
  });

  final double eval;
  final double cumulative;
  final double barOuter;
  final double barPlot;
}
