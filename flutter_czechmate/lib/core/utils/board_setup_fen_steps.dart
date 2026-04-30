/// Parita `BoardSetupFENSteps.swift` — pořadí polí z placement části FEN (rank 8 → 1, zleva doprava).
class BoardSetupFenSteps {
  BoardSetupFenSteps._();

  static const String standardStartFen = 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1';

  static List<BoardSetupPieceStep> build(String fen) {
    final trimmed = fen.trim();
    final first = trimmed.split(RegExp(r'\s+')).first;
    final out = <BoardSetupPieceStep>[];
    var row = 7;
    var col = 0;
    for (var i = 0; i < first.length; i++) {
      final ch = first[i];
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
      final file = String.fromCharCode(97 + col);
      final rank = row + 1;
      final sq = '$file$rank';
      out.add(BoardSetupPieceStep(square: sq, pieceChar: ch, label: _czechLabel(ch, sq)));
      col += 1;
    }
    return out;
  }

  /// Index do `matrix_occupied` (0…63), shodně s iOS `matrixIndex(forSquare:)`.
  static int? matrixIndexForSquare(String notation) {
    final t = notation.trim().toLowerCase();
    if (t.length < 2) return null;
    final file = t.codeUnitAt(0);
    final rank = int.tryParse(t.substring(1));
    if (rank == null || rank < 1 || rank > 8) return null;
    final c = file - 97;
    final r = rank - 1;
    if (c < 0 || c > 7 || r < 0 || r > 7) return null;
    return r * 8 + c;
  }

  /// `snapshot.board`: řádek 0 = rank 8 (`FirmwareSquareNotation` na iOS).
  static ({int row, int col})? boardIndices(String notation) {
    final t = notation.trim().toLowerCase();
    if (t.length < 2) return null;
    final file = t.codeUnitAt(0);
    final rank = int.tryParse(t.substring(1));
    if (rank == null || rank < 1 || rank > 8) return null;
    final col = file - 97;
    if (col < 0 || col > 7) return null;
    final row = 8 - rank;
    if (row < 0 || row > 7) return null;
    return (row: row, col: col);
  }

  static String _czechLabel(String pieceCh, String square) {
    final piece = pieceCh;
    final up = piece.toUpperCase() == piece;
    final low = piece.toLowerCase();
    final pieceWord = switch (low) {
      'k' => up ? 'White king' : 'Black king',
      'q' => up ? 'White queen' : 'Black queen',
      'r' => up ? 'White rook' : 'Black rook',
      'b' => up ? 'White bishop' : 'Black bishop',
      'n' => up ? 'White knight' : 'Black knight',
      'p' => up ? 'White pawn' : 'Black pawn',
      _ => 'Figurka',
    };
    final sqU = square.length >= 2 ? '${square.substring(0, 1).toUpperCase()}${square.substring(1)}' : square;
    return '$pieceWord → $sqU';
  }
}

class BoardSetupPieceStep {
  const BoardSetupPieceStep({
    required this.square,
    required this.pieceChar,
    required this.label,
  });

  final String square;
  final String pieceChar;
  final String label;
}
