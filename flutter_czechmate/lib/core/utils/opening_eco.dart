/// Parita `OpeningECO.swift` — jednoduchý offline název zahájení podle prefixu FEN.
class OpeningEco {
  OpeningEco._();

  /// Vrací název zahájení nebo `null` — pouze několik běžných vzorů.
  static String? titleForFen(String fen) {
    final t = fen.trim().toLowerCase();
    final parts = t.split(RegExp(r'\s+'));
    if (parts.isEmpty) return null;
    final board = parts.first;

    if (board == 'rnbqkbnr/pppppppp/8/8/8/8/pppppppp/rnbqkbnr') {
      return 'Starting position';
    }
    if (board.startsWith('rnbqkbnr/pppppppp/8/8/4p3/8/pppp1ppp/rnbqkbnr')) {
      return 'Englund Defence (unusual)';
    }
    if (board.startsWith('rnbqkbnr/pppppppp/8/8/3p4/8/ppp2ppp/rnbqkbnr')) {
      return 'Scandinavian Defence';
    }
    if (board.startsWith('rnbqkbnr/pppp1ppp/8/4p3/4p3/8/pppp1ppp/rnbqkbnr')) {
      return 'Open game';
    }
    return null;
  }
}
