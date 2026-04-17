import '../models/game_snapshot.dart';

/// Vytvoří minimální FEN z `board[][]` (řádek 0 = rank 8) + barva tahu.
/// Castling/en-passant jsou heuristické — pro `chess` knihovnu v sandboxu.
String fenFromSnapshot(GameSnapshot snap) {
  final turn =
      snap.status.currentPlayer.toLowerCase().startsWith('b') ? 'b' : 'w';
  final placement = _placement(snap.board);
  final castling = _guessCastling(snap.board);
  return '$placement $turn $castling - 0 1';
}

String _placement(List<List<String>> board) {
  final ranks = <String>[];
  for (var r = 0; r < 8; r++) {
    var empty = 0;
    final buf = StringBuffer();
    for (var c = 0; c < 8; c++) {
      final cell = board[r][c].trim();
      if (cell.isEmpty) {
        empty++;
      } else {
        if (empty > 0) {
          buf.write(empty);
          empty = 0;
        }
        buf.write(cell.substring(0, 1));
      }
    }
    if (empty > 0) buf.write(empty);
    ranks.add(buf.toString());
  }
  return ranks.join('/');
}

String _guessCastling(List<List<String>> board) {
  var k = false;
  var q = false;
  var kk = false;
  var qq = false;
  if (board.length == 8 && board[7].length == 8) {
    final row = board[7];
    if (_char(row[4]) == 'K') {
      if (_char(row[7]) == 'R') k = true;
      if (_char(row[0]) == 'R') q = true;
    }
  }
  if (board.length == 8 && board[0].length == 8) {
    final row = board[0];
    if (_char(row[4]) == 'k') {
      if (_char(row[7]) == 'r') kk = true;
      if (_char(row[0]) == 'r') qq = true;
    }
  }
  final w = '${k ? 'K' : ''}${q ? 'Q' : ''}';
  final b = '${kk ? 'k' : ''}${qq ? 'q' : ''}';
  final all = '$w$b';
  return all.isEmpty ? '-' : all;
}

String _char(String cell) {
  final t = cell.trim();
  if (t.isEmpty) return '';
  return t.substring(0, 1);
}
