import '../../l10n/app_localizations.dart';

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
    required AppLocalizations l10n,
  }) {
    final played = normalizeUci(playedFrom, playedTo);
    final best = normalizeUci(bestFrom, bestTo);
    if (played == best) {
      return MoveEvaluationResult(
        grade: MoveGrade.best,
        message: l10n.moveEvalExcellentEngine,
      );
    }
    if (evalBeforeWhite == null || evalAfterWhite == null) {
      return MoveEvaluationResult(
        grade: MoveGrade.inaccuracy,
        message: l10n.moveEvalWeakerNoScore(bestMoveFormatted),
      );
    }
    final whiteJustMoved = (moveIndex1Based - 1) % 2 == 0;
    var scoreDrop = whiteJustMoved ? (evalBeforeWhite - evalAfterWhite) : (evalAfterWhite - evalBeforeWhite);
    if (scoreDrop < 0) scoreDrop = 0;
    final cpLost = (scoreDrop * 100).round();
    if (scoreDrop <= 0.20) {
      return MoveEvaluationResult(
        grade: MoveGrade.good,
        message: l10n.moveEvalGoodLoss(cpLost),
      );
    }
    if (scoreDrop <= 0.50) {
      return MoveEvaluationResult(
        grade: MoveGrade.inaccuracy,
        message: l10n.moveEvalInaccuracy(cpLost, bestMoveFormatted),
      );
    }
    if (scoreDrop <= 1.00) {
      return MoveEvaluationResult(
        grade: MoveGrade.mistake,
        message: l10n.moveEvalMistake(cpLost, bestMoveFormatted),
      );
    }
    return MoveEvaluationResult(
      grade: MoveGrade.blunder,
      message: l10n.moveEvalBlunder(cpLost, bestMoveFormatted),
    );
  }
}
