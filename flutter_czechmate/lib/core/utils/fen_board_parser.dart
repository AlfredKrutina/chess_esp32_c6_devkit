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
