import '../../../core/models/game_status_models.dart';

/// A game counts as running when the board reports a paused game, or an active
/// clock with moves already played, and no puzzle, opening drill or setup
/// tutorial owns the board.
bool boardHasRunningGame(GameStatus? status) {
  if (status == null || status.isGameFinished) return false;
  if (status.puzzle?.active == true || status.puzzle?.setupActive == true) {
    return false;
  }
  if (status.openingTraining?.active == true) return false;
  if (status.boardSetupTutorial == true) return false;
  final s = status.gameState.toLowerCase();
  if (s == 'paused' || s == 'promotion') return true;
  return status.isTimerRunning && status.moveCount > 0;
}
