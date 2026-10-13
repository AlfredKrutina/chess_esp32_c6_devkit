/// Active puzzle in sandbox — comparison with reference UCI line (Lichess / library).
class PuzzleChallengeState {
  const PuzzleChallengeState({
    required this.title,
    required this.startFen,
    required this.solutionUci,
    this.playedUci = const [],
    this.rating,
  });

  final String title;
  final String startFen;
  final List<String> solutionUci;
  final List<String> playedUci;
  final int? rating;

  bool get hasVerifier => solutionUci.isNotEmpty;

  /// Solver moves in a solver-first line (solver, opponent, solver…).
  int get solverPlies =>
      solutionUci.isEmpty ? 0 : (solutionUci.length + 1) ~/ 2;

  int get targetPlies => solverPlies;

  PuzzleChallengeState copyWith({
    List<String>? playedUci,
  }) {
    return PuzzleChallengeState(
      title: title,
      startFen: startFen,
      solutionUci: solutionUci,
      playedUci: playedUci ?? this.playedUci,
      rating: rating,
    );
  }
}
