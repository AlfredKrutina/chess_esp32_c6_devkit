/// Parita `MoveEvaluation.swift` / `MoveGrade` — klasifikace tahu podle Stockfish.
enum MoveGrade {
  best,
  good,
  inaccuracy,
  mistake,
  blunder,
  error,
}

class MoveEvaluationResult {
  const MoveEvaluationResult({required this.grade, required this.message});
  final MoveGrade grade;
  final String message;
}

class MoveEvalHistoryEntry {
  const MoveEvalHistoryEntry({
    required this.moveIndex1Based,
    this.evalWhitePawns,
    required this.grade,
    this.lossPawns,
  });

  final int moveIndex1Based;
  final double? evalWhitePawns;
  final MoveGrade grade;
  final double? lossPawns;
}

class MoveEvaluation {
  static double evalToWhitePerspective(String fen, double evalRaw) {
    if (fen.contains(' b ')) return -evalRaw;
    return evalRaw;
  }

  static String normalizeUci(String from, String to) {
    final a = from.trim().toLowerCase();
    final b = to.trim().toLowerCase();
    return '$a$b';
  }

  static MoveEvaluationResult classify({
    required String playedFrom,
    required String playedTo,
    required String bestFrom,
    required String bestTo,
    required double? evalBeforeWhite,
    required double? evalAfterWhite,
    required int moveIndex1Based,
    required String bestMoveFormatted,
  }) {
    final played = normalizeUci(playedFrom, playedTo);
    final best = normalizeUci(bestFrom, bestTo);
    if (played == best) {
      return const MoveEvaluationResult(
        grade: MoveGrade.best,
        message: 'Excellent — matches the engine best move.',
      );
    }
    if (evalBeforeWhite == null || evalAfterWhite == null) {
      return MoveEvaluationResult(
        grade: MoveGrade.inaccuracy,
        message: 'Weaker move (no exact score). Stronger was $bestMoveFormatted.',
      );
    }
    final whiteJustMoved = (moveIndex1Based - 1) % 2 == 0;
    var scoreDrop = whiteJustMoved ? (evalBeforeWhite - evalAfterWhite) : (evalAfterWhite - evalBeforeWhite);
    if (scoreDrop < 0) scoreDrop = 0;
    final cpLost = (scoreDrop * 100).round();
    if (scoreDrop <= 0.20) {
      return MoveEvaluationResult(
        grade: MoveGrade.good,
        message: 'Good move — small loss (~$cpLost cp).',
      );
    }
    if (scoreDrop <= 0.50) {
      return MoveEvaluationResult(
        grade: MoveGrade.inaccuracy,
        message: 'Inaccuracy — about $cpLost cp worse. Stronger was $bestMoveFormatted.',
      );
    }
    if (scoreDrop <= 1.00) {
      return MoveEvaluationResult(
        grade: MoveGrade.mistake,
        message: 'Mistake — about $cpLost cp worse. Better was $bestMoveFormatted.',
      );
    }
    return MoveEvaluationResult(
      grade: MoveGrade.blunder,
      message: 'Blunder — about $cpLost cp worse. Much better was $bestMoveFormatted.',
    );
  }
}
