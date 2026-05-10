import 'package:chess/chess.dart' as ch;

import '../../../core/models/game_snapshot.dart';
import '../../../core/utils/fen_board_parser.dart';

/// Applies [movesAppliedCount] plies from [snap.history] onto the standard start position.
/// Returns null if a move could not be replayed (illegal SAN/UCI).
List<List<String>>? boardAfterHistoryPrefix(
    GameSnapshot snap, int movesAppliedCount) {
  final moves = snap.history.moves;
  if (movesAppliedCount <= 0) {
    final chess = ch.Chess();
    return boardFromPlacementFen(chess.generate_fen().split(' ').first);
  }
  final chess = ch.Chess();
  final n = movesAppliedCount.clamp(0, moves.length);
  for (var i = 0; i < n; i++) {
    if (!_applyOneMove(chess, moves[i])) {
      return null;
    }
  }
  return boardFromPlacementFen(chess.generate_fen().split(' ').first);
}

/// Final board from snapshot (normalized rows).
List<List<String>> boardFromSnapshot(GameSnapshot snap) =>
    snap.board.map((r) => List<String>.from(r)).toList();

bool _applyOneMove(ch.Chess chess, GameHistoryMove m) {
  final san = m.san?.trim();
  if (san != null && san.isNotEmpty) {
    return chess.move(san);
  }
  final from = m.from;
  final to = m.to;
  if (from == null || to == null) return false;
  final map = <String, dynamic>{'from': from, 'to': to};
  final promo = _inferPromotionLetter(chess, from, to);
  if (promo != null) {
    map['promotion'] = promo;
  }
  return chess.move(map);
}

String? _inferPromotionLetter(ch.Chess chess, String from, String to) {
  final piece = chess.get(from);
  if (piece?.type != ch.PieceType.PAWN) return null;
  final rank = int.tryParse(to.substring(1)) ?? 0;
  if (piece!.color == ch.Color.WHITE && rank == 8) return 'q';
  if (piece.color == ch.Color.BLACK && rank == 1) return 'q';
  return null;
}

/// Indices of half-moves to render for recap (0 = start, includes sampled endpoints).
List<int> recapFrameIndices(int totalPlies, int maxFrames) {
  if (totalPlies <= 0) return [0];
  final cap = maxFrames.clamp(4, 96);
  if (totalPlies + 1 <= cap) {
    return List.generate(totalPlies + 1, (i) => i);
  }
  final out = <int>{};
  final step = (totalPlies / (cap - 1));
  for (var k = 0; k < cap; k++) {
    out.add((k * step).round().clamp(0, totalPlies));
  }
  out.add(totalPlies);
  final sorted = out.toList()..sort();
  return sorted;
}
