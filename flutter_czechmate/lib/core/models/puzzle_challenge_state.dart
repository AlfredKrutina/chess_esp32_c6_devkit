/// Aktivní puzzle v sandboxu — porovnání s referenční UCI linkou (Lichess / knihovna).
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
  int get targetPlies => solutionUci.length;

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
