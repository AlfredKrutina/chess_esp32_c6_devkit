/// První pole FEN → `board[][]` (řádek 0 = rank 8).
List<List<String>> boardFromPlacementFen(String fen) {
  final placement = fen.split(' ').first;
  final ranks = placement.split('/');
  if (ranks.length != 8) {
    return List.generate(8, (_) => List.filled(8, ''));
  }
  final board = <List<String>>[];
  for (final rank in ranks) {
    final row = <String>[];
    for (var i = 0; i < rank.length; i++) {
      final ch = rank.codeUnitAt(i);
      if (ch >= 49 && ch <= 56) {
        final n = ch - 48;
        for (var k = 0; k < n; k++) {
          row.add('');
        }
      } else {
        row.add(String.fromCharCode(ch));
      }
    }
    while (row.length < 8) {
      row.add('');
    }
    board.add(row.take(8).toList());
  }
  return board;
}

/// Snapshot `board[row][col]` po normalizaci (řádek 0 = rank 1) → placement FEN (řádek 0 = rank 8).
String placementFenFromSnapshotBoard(List<List<String>> board) {
  if (board.length != 8) return '';
  final rows = <String>[];
  for (var r = 7; r >= 0; r--) {
    if (board[r].length != 8) return '';
    final buf = StringBuffer();
    var empty = 0;
    for (var c = 0; c < 8; c++) {
      final raw = board[r][c].trim();
      final p = raw.isEmpty ? '' : raw.substring(0, 1);
      if (p.isEmpty || p == ' ') {
        empty++;
      } else {
        if (empty > 0) {
          buf.write(empty);
          empty = 0;
        }
        buf.write(p);
      }
    }
    if (empty > 0) buf.write(empty);
    rows.add(buf.toString());
  }
  return rows.join('/');
}
