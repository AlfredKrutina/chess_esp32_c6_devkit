import 'package:flutter/material.dart';

import '../../../core/models/game_end_report_model.dart';
import '../../../core/utils/game_end_report_timing.dart';
import '../../../core/analysis/move_evaluation.dart';
import '../../analysis/eval_line_chart.dart';
import 'export_board_preview.dart';
import 'game_export_options.dart';
import 'timing_charts.dart';

/// Colors for export raster (supports transparent / high-contrast story).
@immutable
class GameShareExportTheme {
  const GameShareExportTheme({
    required this.foreground,
    required this.muted,
    required this.accent,
    required this.chartWhite,
    required this.chartBlack,
    required this.boardLight,
    required this.boardDark,
    required this.divider,
  });

  final Color foreground;
  final Color muted;
  final Color accent;
  final Color chartWhite;
  final Color chartBlack;
  final Color boardLight;
  final Color boardDark;
  final Color divider;

  factory GameShareExportTheme.adaptive(BuildContext context, {required bool transparent}) {
    final cs = Theme.of(context).colorScheme;
    if (transparent) {
      return GameShareExportTheme(
        foreground: Colors.white,
        muted: Colors.white.withValues(alpha: 0.78),
        accent: const Color(0xFF90CAF9),
        chartWhite: Colors.white,
        chartBlack: const Color(0xFFD1C4E9),
        boardLight: const Color(0x55FFFFFF),
        boardDark: const Color(0x33FFFFFF),
        divider: Colors.white.withValues(alpha: 0.35),
      );
    }
    return GameShareExportTheme(
      foreground: cs.onSurface,
      muted: cs.onSurfaceVariant,
      accent: cs.primary,
      chartWhite: cs.primary,
      chartBlack: Colors.purple.shade400,
      boardLight: const Color(0xFFE8D4B8),
      boardDark: const Color(0xFFB58863),
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
                Theme.of(context).colorScheme.surfaceContainerHighest.withValues(alpha: 0.88),
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
      child: options.aspect == GameExportAspect.landscape
          ? _landscapeBody(context)
          : _portraitBody(context),
    );

    return decorated(child: content);
  }

  Widget _recapBanner() {
    if (recapCaption == null || recapCaption!.isEmpty) return const SizedBox.shrink();
    return Padding(
      padding: const EdgeInsets.only(bottom: 8),
      child: Container(
        padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
        decoration: BoxDecoration(
          color: theme.accent.withValues(alpha: 0.22),
          borderRadius: BorderRadius.circular(8),
          border: Border.all(color: theme.accent.withValues(alpha: 0.45)),
        ),
        child: Text(
          recapCaption!,
          style: TextStyle(color: theme.foreground, fontWeight: FontWeight.w600, fontSize: 13),
        ),
      ),
    );
  }

  Widget _header() {
    if (!options.showBranding) return const SizedBox.shrink();
    return Row(
      children: [
        Icon(Icons.grid_view_rounded, size: 20, color: theme.accent),
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

  Widget _board() {
    if (!options.showFinalBoard) return const SizedBox.shrink();
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
      return Text(noEvalCaption, style: TextStyle(fontSize: 11, color: theme.muted));
    }
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(evalCaption, style: _titleSm),
        const SizedBox(height: 4),
        EvalLineChart(points: evalPoints, height: height, accentColor: theme.accent),
      ],
    );
  }

  Widget _timing(double chartH) {
    if (!options.showCumulativeChart && !options.showPerMoveChart) {
      return const SizedBox.shrink();
    }
    if (!hasTiming) {
      return Text(noTimingCaption, style: TextStyle(fontSize: 11, color: theme.muted));
    }
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        if (options.showCumulativeChart) ...[
          Text(elapsedTitle, style: _titleSm.copyWith(fontSize: 12)),
          Text(elapsedSubtitle, style: TextStyle(fontSize: 10, color: theme.muted)),
          const SizedBox(height: 4),
          CumulativePlayedTimeChart(points: cumulative, accent: theme.accent),
          Text(halfMoveAxis, style: TextStyle(fontSize: 9, color: theme.muted)),
          const SizedBox(height: 8),
        ],
        if (options.showPerMoveChart) ...[
          Text(perMoveTitle, style: _titleSm.copyWith(fontSize: 12)),
          Text(perMoveSubtitle, style: TextStyle(fontSize: 10, color: theme.muted)),
          const SizedBox(height: 4),
          TimePerMoveBarChart(
            points: think,
            whiteColor: theme.chartWhite,
            blackColor: theme.chartBlack,
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

  Widget _portraitBody(BuildContext context) {
    final chartH = switch (options.aspect) {
      GameExportAspect.story => 88.0,
      GameExportAspect.card => 96.0,
      GameExportAspect.square => 52.0,
      GameExportAspect.landscape => 72.0,
    };

    return LayoutBuilder(
      builder: (context, constraints) {
        final w = constraints.maxWidth;
        final children = <Widget>[
          _recapBanner(),
          _header(),
          if (options.showBranding) const SizedBox(height: 10),
          _resultBlock(),
          const SizedBox(height: 10),
          Divider(height: 16, color: theme.divider),
          const SizedBox(height: 6),
          _stats(),
          if (options.showMaterial &&
              (model.capturedValueWhite > 0 || model.capturedValueBlack > 0)) ...[
            const SizedBox(height: 8),
            Text(materialCaption, style: TextStyle(fontSize: 11, color: theme.muted)),
          ],
          if (options.showFinalBoard) ...[
            const SizedBox(height: 8),
            SizedBox(width: w, height: w, child: Center(child: _board())),
          ],
          const SizedBox(height: 6),
          if (options.aspect != GameExportAspect.square) ...[
            SizedBox(height: chartH + 36, child: _eval(chartH)),
            const SizedBox(height: 6),
            _timing(chartH),
          ],
        ];

        final column = Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: children,
        );

        // Timing charts use fixed ~200px heights; card/story canvas is shorter → scale whole column down.
        return FittedBox(
          fit: BoxFit.scaleDown,
          alignment: Alignment.topCenter,
          child: SizedBox(width: w, child: column),
        );
      },
    );
  }

  Widget _landscapeBody(BuildContext context) {
    return LayoutBuilder(
      builder: (context, constraints) {
        final w = constraints.maxWidth;
        final rowH = (constraints.maxHeight * 0.42).clamp(120.0, 260.0);

        final column = Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            _recapBanner(),
            SizedBox(
              height: rowH,
              width: w,
              child: Row(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Expanded(
                    flex: 11,
                    child: ClipRect(
                      child: Align(
                        alignment: Alignment.topLeft,
                        child: Column(
                          mainAxisSize: MainAxisSize.min,
                          crossAxisAlignment: CrossAxisAlignment.start,
                          children: [
                            _header(),
                            const SizedBox(height: 8),
                            _resultBlock(),
                            const SizedBox(height: 8),
                            Divider(height: 12, color: theme.divider),
                            const SizedBox(height: 6),
                            _stats(),
                            if (options.showMaterial &&
                                (model.capturedValueWhite > 0 || model.capturedValueBlack > 0)) ...[
                              const SizedBox(height: 6),
                              Text(materialCaption,
                                  style: TextStyle(fontSize: 10, color: theme.muted)),
                            ],
                          ],
                        ),
                      ),
                    ),
                  ),
                  const SizedBox(width: 10),
                  Expanded(flex: 13, child: Center(child: _board())),
                ],
              ),
            ),
            const SizedBox(height: 8),
            SizedBox(height: 76, child: _eval(72)),
            const SizedBox(height: 6),
            _timing(72),
          ],
        );

        return FittedBox(
          fit: BoxFit.scaleDown,
          alignment: Alignment.topCenter,
          child: SizedBox(width: w, child: column),
        );
      },
    );
  }
}
