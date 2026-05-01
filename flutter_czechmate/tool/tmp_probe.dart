import 'package:chess/chess.dart' as ch;

void main() {
  const fen = '8/8/8/8/8/3q2k1/8/6K1 b - - 0 1';
  final chess = ch.Chess();
  print('load ${chess.load(fen)}');
  for (final m in chess.generate_moves()) {
    final c = ch.Chess()..load(fen);
    c.move(m);
    if (c.in_checkmate) {
      print('mate ${m.fromAlgebraic}${m.toAlgebraic}');
    }
  }
}
