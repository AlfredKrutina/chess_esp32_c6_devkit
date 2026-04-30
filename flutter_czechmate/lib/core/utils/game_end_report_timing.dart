import '../models/game_snapshot.dart';

/// Parita `GameEndReportTiming.swift` / `EndgameThinkPlyPoint`.
class GameEndThinkPlyPoint {
  const GameEndThinkPlyPoint({
    required this.plyIndex,
    required this.isWhite,
    this.secondsFromPrevious,
  });

  final int plyIndex;
  final bool isWhite;
  final double? secondsFromPrevious;
}

class GameEndCumulativePoint {
  const GameEndCumulativePoint({required this.plyIndex, required this.totalSeconds});

  final int plyIndex;
  final double totalSeconds;
}

class GameEndReportTiming {
  /// Oříznutí nesmyslných intervalů (chyba hodin / reboot desky) — graf bez vertikálních artifactů.
  static const double maxPlausibleMoveSeconds = 3600;

  static double? _sanitizeMoveDeltaSec(double raw) {
    if (raw.isNaN || raw.isInfinite || raw < 0) return null;
    if (raw > maxPlausibleMoveSeconds) return null;
    return raw;
  }

  static List<GameEndThinkPlyPoint> thinkPlyPoints(List<GameHistoryMove> moves) {
    final out = <GameEndThinkPlyPoint>[];
    for (var i = 0; i < moves.length; i++) {
      final m = moves[i];
      double? prev;
      if (i == 0) {
        prev = null;
      } else {
        final a = moves[i - 1].timestamp;
        final b = m.timestamp;
        if (a != null && b != null && b >= a) {
          prev = _sanitizeMoveDeltaSec((b - a) / 1000.0);
        } else {
          prev = null;
        }
      }
      out.add(GameEndThinkPlyPoint(plyIndex: i + 1, isWhite: i % 2 == 0, secondsFromPrevious: prev));
    }
    return out;
  }

  static List<GameEndCumulativePoint> cumulativePoints(List<GameEndThinkPlyPoint> think) {
    var sum = 0.0;
    final out = <GameEndCumulativePoint>[];
    for (final p in think) {
      final d = p.secondsFromPrevious;
      if (d != null) {
        sum += d;
        out.add(GameEndCumulativePoint(plyIndex: p.plyIndex, totalSeconds: sum));
      }
    }
    return out;
  }
}
