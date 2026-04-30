import '../models/game_snapshot.dart';
import 'fen_from_board.dart';

/// Jednoduchý PGN z historie desky (SAN pokud je ve snapshotu, jinak UCI).
String buildPgnFromSnapshot(GameSnapshot snap) {
  final moves = snap.history.moves;
  final ge = snap.status.gameEnd;
  var result = '*';
  if (ge?.ended == true) {
    final w = ge!.winner?.toLowerCase();
    if (w == 'white') {
      result = '1-0';
    } else if (w == 'black') {
      result = '0-1';
    } else {
      result = '1/2-1/2';
    }
  } else if (snap.status.gameState.toLowerCase() == 'finished') {
    if (snap.status.checkmate == true) {
      result = snap.status.currentPlayer.toLowerCase().contains('white') ? '0-1' : '1-0';
    } else {
      result = '1/2-1/2';
    }
  }

  final sb = StringBuffer();
    sb.writeln('[Event "CZECHMATE"]');
  sb.writeln('[Site "?"]');
  final ts = snap.timestamp;
  if (ts > 0) {
    final d = DateTime.fromMillisecondsSinceEpoch(ts * 1000, isUtc: true);
    sb.writeln('[UTCDate "${d.year.toString().padLeft(4, '0')}.${d.month.toString().padLeft(2, '0')}.${d.day.toString().padLeft(2, '0')}"]');
  }
  sb.writeln('[Result "$result"]');
  sb.writeln('[FEN "${fenFromSnapshot(snap)}"]');
  if (moves.isNotEmpty) {
    final first = moves.first;
    final head = (first.san != null && first.san!.trim().isNotEmpty)
        ? first.san!.trim()
        : '${first.from ?? ''}${first.to ?? ''}';
    if (head.isNotEmpty) {
      sb.writeln('[Opening "Start: $head"]');
    }
  }
  sb.writeln();

  final line = StringBuffer();
  for (var i = 0; i < moves.length; i++) {
    if (i.isEven) {
      line.write('${i ~/ 2 + 1}. ');
    }
    final m = moves[i];
    if (m.san != null && m.san!.trim().isNotEmpty) {
      line.write('${m.san!.trim()} ');
    } else if (m.from != null && m.to != null) {
      line.write('${m.from}${m.to} ');
    }
    if (line.length > 72 && i.isOdd) {
      sb.writeln(line.toString().trimRight());
      line.clear();
    }
  }
  if (line.isNotEmpty) sb.writeln(line.toString().trimRight());
  sb.writeln(result);
  return sb.toString();
}
