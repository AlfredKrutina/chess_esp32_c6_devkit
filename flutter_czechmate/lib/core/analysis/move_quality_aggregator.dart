import 'move_evaluation.dart';

/// Parita `MoveQualityAggregator` / `MoveQualitySummary.swift`.
class MoveQualityWindowStats {
  const MoveQualityWindowStats({
    required this.count,
    this.averageQualityPercent,
    this.averageCpLoss,
  });

  final int count;
  final double? averageQualityPercent;
  final double? averageCpLoss;
}

class PlayerMoveQualityTriplet {
  const PlayerMoveQualityTriplet({
    required this.last3,
    required this.last10,
    required this.fullGame,
  });

  final MoveQualityWindowStats last3;
  final MoveQualityWindowStats last10;
  final MoveQualityWindowStats fullGame;
}

class MoveQualityBoardSummary {
  const MoveQualityBoardSummary({required this.white, required this.black});

  final PlayerMoveQualityTriplet white;
  final PlayerMoveQualityTriplet black;
}

class MoveQualityAggregator {
  static bool isWhiteMove(int moveIndex1Based) => moveIndex1Based % 2 == 1;

  static double qualityPercent(MoveGrade grade) {
    switch (grade) {
      case MoveGrade.best:
        return 100;
      case MoveGrade.good:
        return 88;
      case MoveGrade.inaccuracy:
        return 62;
      case MoveGrade.mistake:
        return 35;
      case MoveGrade.blunder:
        return 12;
      case MoveGrade.error:
        return 45;
    }
  }

  static MoveQualityWindowStats windowStats(List<MoveEvalHistoryEntry> entries) {
    if (entries.isEmpty) {
      return const MoveQualityWindowStats(count: 0, averageQualityPercent: null, averageCpLoss: null);
    }
    final percents = entries.map((e) => qualityPercent(e.grade)).toList();
    final avgP = percents.reduce((a, b) => a + b) / percents.length;
    final cps = entries.where((e) => e.lossPawns != null).map((e) => e.lossPawns! * 100).toList();
    final avgCp = cps.isEmpty ? null : cps.reduce((a, b) => a + b) / cps.length;
    return MoveQualityWindowStats(count: entries.length, averageQualityPercent: avgP, averageCpLoss: avgCp);
  }

  static List<T> _suffix<T>(List<T> list, int n) {
    if (list.length <= n) return list;
    return list.sublist(list.length - n);
  }

  static MoveQualityBoardSummary boardSummary(List<MoveEvalHistoryEntry> history) {
    final sorted = [...history]..sort((a, b) => a.moveIndex1Based.compareTo(b.moveIndex1Based));
    final whiteAll = sorted.where((e) => isWhiteMove(e.moveIndex1Based)).toList();
    final blackAll = sorted.where((e) => !isWhiteMove(e.moveIndex1Based)).toList();

    return MoveQualityBoardSummary(
      white: PlayerMoveQualityTriplet(
        last3: windowStats(_suffix(whiteAll, 3)),
        last10: windowStats(_suffix(whiteAll, 10)),
        fullGame: windowStats(whiteAll),
      ),
      black: PlayerMoveQualityTriplet(
        last3: windowStats(_suffix(blackAll, 3)),
        last10: windowStats(_suffix(blackAll, 10)),
        fullGame: windowStats(blackAll),
      ),
    );
  }
}
