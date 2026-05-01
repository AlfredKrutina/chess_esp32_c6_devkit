import 'package:chess/chess.dart' as ch;

String uciFromMove(ch.Move m) {
  final sb = StringBuffer()
    ..write(m.fromAlgebraic.toLowerCase())
    ..write(m.toAlgebraic.toLowerCase());
  final prom = m.promotion;
  if (prom != null) {
    sb.write(switch (prom) {
      ch.PieceType.QUEEN => 'q',
      ch.PieceType.ROOK => 'r',
      ch.PieceType.BISHOP => 'b',
      ch.PieceType.KNIGHT => 'n',
      _ => 'q',
    });
  }
  return sb.toString();
}

void dumpMateLines(String label, String fen) {
  final chess = ch.Chess();
  if (!chess.load(fen)) {
    print('$label invalid fen');
    return;
  }
  for (final m in chess.generate_moves()) {
    final c = ch.Chess()..load(fen);
    c.move(m);
    print('$label $m -> ${uciFromMove(m)} mate=${c.in_checkmate}');
  }
}

void main() {
  dumpMateLines('promo', '8/P7/8/1k6/8/8/8/4K3 w - - 0 1');
  dumpMateLines('rook2', '3k4/8/8/8/8/8/5R2/4RK2 w - - 0 1');
}
