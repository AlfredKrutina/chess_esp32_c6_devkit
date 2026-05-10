import 'dart:ui' show Size;

import 'package:flutter/foundation.dart';

import 'game_export_block_id.dart';

/// Aspect preset for raster export (logical pixels before [pixelRatio]).
enum GameExportAspect {
  /// Tall summary (~ feed card).
  card,
  square,
  story,
  landscape,
}

@immutable
class GameExportOptions {
  GameExportOptions({
    this.aspect = GameExportAspect.card,
    this.transparentBackground = false,
    this.showBranding = true,
    this.showResult = true,
    this.showReason = true,
    this.showStats = true,
    this.showMaterial = true,
    this.showFinalBoard = true,
    this.flipBoard = false,
    this.showEvalChart = true,
    this.showCumulativeChart = true,
    this.showPerMoveChart = true,
    this.roundedClip = true,
    this.recapMoveIndex,
    this.recapMoveTotal,
    List<GameExportBlockId>? blockOrder,
  }) : blockOrder = normalizeGameExportBlockOrder(blockOrder);

  final GameExportAspect aspect;
  final bool transparentBackground;
  final bool showBranding;
  final bool showResult;
  final bool showReason;
  final bool showStats;
  final bool showMaterial;
  final bool showFinalBoard;
  final bool flipBoard;
  final bool showEvalChart;
  final bool showCumulativeChart;
  final bool showPerMoveChart;
  final bool roundedClip;

  /// When non-null, shows recap caption (GIF frames).
  final int? recapMoveIndex;
  final int? recapMoveTotal;

  /// Vertical blocks on the export image (drag-reordered in report UI).
  final List<GameExportBlockId> blockOrder;

  static final minimal = GameExportOptions(
    showBranding: false,
    showReason: false,
    showMaterial: false,
    showFinalBoard: false,
    showEvalChart: false,
    showCumulativeChart: false,
    showPerMoveChart: false,
    aspect: GameExportAspect.square,
  );

  static final storyHero = GameExportOptions(
    aspect: GameExportAspect.story,
    showEvalChart: true,
    showCumulativeChart: false,
    showPerMoveChart: false,
    showFinalBoard: true,
  );

  /// Wide share image (same content toggles as [GameExportOptions] default).
  static final landscapeWide = GameExportOptions(
    aspect: GameExportAspect.landscape,
  );

  Size logicalSize() {
    switch (aspect) {
      case GameExportAspect.card:
        return const Size(380, 680);
      case GameExportAspect.square:
        return const Size(360, 360);
      case GameExportAspect.story:
        return const Size(360, 640);
      case GameExportAspect.landscape:
        return const Size(640, 360);
    }
  }

  GameExportOptions copyWith({
    GameExportAspect? aspect,
    bool? transparentBackground,
    bool? showBranding,
    bool? showResult,
    bool? showReason,
    bool? showStats,
    bool? showMaterial,
    bool? showFinalBoard,
    bool? flipBoard,
    bool? showEvalChart,
    bool? showCumulativeChart,
    bool? showPerMoveChart,
    bool? roundedClip,
    int? recapMoveIndex,
    int? recapMoveTotal,
    bool clearRecap = false,
    List<GameExportBlockId>? blockOrder,
  }) {
    return GameExportOptions(
      aspect: aspect ?? this.aspect,
      transparentBackground: transparentBackground ?? this.transparentBackground,
      showBranding: showBranding ?? this.showBranding,
      showResult: showResult ?? this.showResult,
      showReason: showReason ?? this.showReason,
      showStats: showStats ?? this.showStats,
      showMaterial: showMaterial ?? this.showMaterial,
      showFinalBoard: showFinalBoard ?? this.showFinalBoard,
      flipBoard: flipBoard ?? this.flipBoard,
      showEvalChart: showEvalChart ?? this.showEvalChart,
      showCumulativeChart: showCumulativeChart ?? this.showCumulativeChart,
      showPerMoveChart: showPerMoveChart ?? this.showPerMoveChart,
      roundedClip: roundedClip ?? this.roundedClip,
      recapMoveIndex: clearRecap ? null : (recapMoveIndex ?? this.recapMoveIndex),
      recapMoveTotal: clearRecap ? null : (recapMoveTotal ?? this.recapMoveTotal),
      blockOrder: blockOrder ?? this.blockOrder,
    );
  }
}
