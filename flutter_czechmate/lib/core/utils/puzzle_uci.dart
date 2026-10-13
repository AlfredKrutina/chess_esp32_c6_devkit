import 'package:chess/chess.dart' as ch;

/// UCI move from sandbox selection (lower case, promo as single letter q/r/b/n).
String sandboxMoveToUci(String from, String to, [String? promotionLower]) {
  final f = from.toLowerCase().trim();
  final t = to.toLowerCase().trim();
  if (promotionLower != null && promotionLower.isNotEmpty) {
    return '$f$t${promotionLower.substring(0, 1).toLowerCase()}';
  }
  return '$f$t';
}

bool puzzleUciListsMatch(List<String> expected, List<String> played) {
  if (expected.length != played.length) return false;
  for (var i = 0; i < expected.length; i++) {
    if (expected[i].toLowerCase().trim() != played[i].toLowerCase().trim()) {
      return false;
    }
  }
  return true;
}

/// Lichess daily FEN is the position before the opponent's setup move.
/// Bundled puzzles are already the solver-to-move position.
enum PuzzleLineKind { playerFirst, lichess }

class PuzzlePlayLine {
  const PuzzlePlayLine({required this.startFen, required this.moves});

  /// Position where the solver is to move.
  final String startFen;

  /// Solver, opponent, solver… UCI.
  final List<String> moves;

  int get solverPlies => moves.isEmpty ? 0 : (moves.length + 1) ~/ 2;
}

bool applyPuzzleUci(ch.Chess chess, String uci) {
  final u = uci.trim().toLowerCase();
  if (u.length < 4) return false;
  final move = <String, String>{
    'from': u.substring(0, 2),
    'to': u.substring(2, 4),
  };
  if (u.length >= 5) {
    move['promotion'] = u.substring(4, 5);
  }
  return chess.move(move);
}

PuzzlePlayLine? normalizePuzzleLine({
  required String fen,
  required List<String> solution,
  required PuzzleLineKind kind,
}) {
  final cleaned = solution
      .map((e) => e.trim().toLowerCase())
      .where((e) => e.isNotEmpty)
      .toList();
  final chess = ch.Chess();
  if (!chess.load(fen.trim())) return null;
  if (kind == PuzzleLineKind.lichess) {
    if (cleaned.isEmpty) {
      return PuzzlePlayLine(startFen: chess.generate_fen(), moves: const []);
    }
    if (!applyPuzzleUci(chess, cleaned.first)) return null;
    return PuzzlePlayLine(
      startFen: chess.generate_fen(),
      moves: cleaned.sublist(1),
    );
  }
  return PuzzlePlayLine(startFen: chess.generate_fen(), moves: cleaned);
}

/// Saved library rows without `lineKind`: two or more moves are Lichess dailies.
PuzzleLineKind puzzleLineKindFor({String? stored, required int solutionLength}) {
  if (stored == 'player') return PuzzleLineKind.playerFirst;
  if (stored == 'lichess') return PuzzleLineKind.lichess;
  return solutionLength >= 2 ? PuzzleLineKind.lichess : PuzzleLineKind.playerFirst;
}

List<int> occupancyFromFen(String fen) {
  final exp = List<int>.filled(64, 0);
  final first = fen.trim().split(RegExp(r'\s+')).first;
  var row = 7;
  var col = 0;
  for (final ch in first.split('')) {
    if (ch == '/') {
      row -= 1;
      col = 0;
      continue;
    }
    final skip = int.tryParse(ch);
    if (skip != null) {
      col += skip;
      continue;
    }
    if (row >= 0 && row < 8 && col >= 0 && col < 8) {
      exp[row * 8 + col] = 1;
    }
    col += 1;
  }
  return exp;
}

class PuzzleSquareDiff {
  const PuzzleSquareDiff({required this.square, required this.missing});

  final String square;
  final bool missing;
}

List<PuzzleSquareDiff> puzzleOccupancyDiffs(String fen, List<int>? matrix) {
  if (matrix == null || matrix.length < 64) return const [];
  final exp = occupancyFromFen(fen);
  final out = <PuzzleSquareDiff>[];
  for (var i = 0; i < 64; i++) {
    final got = matrix[i] == 0 ? 0 : 1;
    if (got == exp[i]) continue;
    final file = String.fromCharCode(97 + (i % 8));
    final rank = (i ~/ 8) + 1;
    out.add(PuzzleSquareDiff(square: '$file$rank', missing: exp[i] == 1));
  }
  return out;
}

/// How many solver moves are already on the board (`ply` is the next line index).
int puzzleSolverMovesDone({required int plyIndex, required bool solved, required int plyTotal}) {
  if (solved || (plyTotal > 0 && plyIndex >= plyTotal)) {
    return (plyTotal + 1) ~/ 2;
  }
  if (plyIndex <= 0) return 0;
  return (plyIndex + 1) ~/ 2;
}
