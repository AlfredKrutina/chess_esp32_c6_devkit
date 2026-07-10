import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../core/utils/fen_board_parser.dart';
import '../connection/board_session_notifier.dart';
import '../game/board/chess_board_widget.dart';
import 'opening_catalog_repository.dart';

/// Miniboard v opening lekci — sync logické pozice ze snapshotu (fallback start FEN).
class OpeningLessonBoardPreview extends ConsumerWidget {
  const OpeningLessonBoardPreview({
    super.key,
    required this.line,
    this.hintFrom,
    this.hintTo,
    this.checkpointSquares = const [],
    this.compact = true,
  });

  final OpeningLine line;
  final String? hintFrom;
  final String? hintTo;
  final List<String> checkpointSquares;
  final bool compact;

  static bool _boardHasPieces(List<List<String>> board) {
    for (final row in board) {
      for (final cell in row) {
        final t = cell.trim();
        if (t.isNotEmpty && t != ' ') return true;
      }
    }
    return false;
  }

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final snap = ref.watch(boardSessionNotifierProvider).snapshot;
    final normalized = snap?.normalizingBoardRowsFromFirmwareExportIfNeeded();
    final board = normalized?.board;
    final locale = Localizations.localeOf(context).languageCode;
    final cs = locale.startsWith('cs');

    Widget preview;
    if (board != null && _boardHasPieces(board)) {
      final placement = placementFenFromSnapshotBoard(board);
      final fen = placement.isNotEmpty
          ? '$placement w KQkq - 0 1'
          : line.startFen;
      preview = FenBoardPreview(
        fen: fen,
        showCoordinates: !compact,
        highlightFrom: hintFrom,
        highlightTo: hintTo,
        accentSquares: checkpointSquares,
      );
    } else {
      preview = FenBoardPreview(
        fen: line.startFen,
        showCoordinates: !compact,
        highlightFrom: hintFrom,
        highlightTo: hintTo,
        accentSquares: checkpointSquares,
      );
    }

    return Semantics(
      label: cs ? 'Miniboard — logická pozice lekce' : 'Miniboard — lesson logical position',
      child: Padding(
        padding: const EdgeInsets.symmetric(vertical: 8),
        child: Center(
          child: ConstrainedBox(
            constraints: BoxConstraints(maxWidth: compact ? 280 : 360),
            child: preview,
          ),
        ),
      ),
    );
  }
}
