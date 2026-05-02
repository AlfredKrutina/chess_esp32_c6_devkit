import 'package:chess/chess.dart' as ch;

import '../models/board_timer_state.dart';
import '../models/game_snapshot.dart';
import '../models/game_status_models.dart';
import '../utils/fen_board_parser.dart';
import '../utils/fen_from_board.dart';
import 'board_api_exception.dart';

/// Konfigurace času podle firmware `type` (HTTP `timer_config`), včetně 14 custom.
TimerConfigPart mockTimerConfigForFirmware({
  required int type,
  int? customMinutes,
  int? customIncrement,
}) {
  switch (type) {
    case 0:
      return const TimerConfigPart(
        type: 0,
        name: 'No clock',
        description: 'Demo',
        initialTimeMs: 0,
        incrementMs: 0,
        isFast: false,
      );
    case 1:
      return const TimerConfigPart(
        type: 1,
        name: 'Bullet',
        description: '1 min',
        initialTimeMs: 60000,
        incrementMs: 0,
        isFast: true,
      );
    case 3:
      return const TimerConfigPart(
        type: 3,
        name: 'Bullet',
        description: '2|1',
        initialTimeMs: 120000,
        incrementMs: 1000,
        isFast: true,
      );
    case 4:
      return const TimerConfigPart(
        type: 4,
        name: 'Blitz',
        description: '3 min',
        initialTimeMs: 180000,
        incrementMs: 0,
        isFast: true,
      );
    case 5:
      return const TimerConfigPart(
        type: 5,
        name: 'Blitz',
        description: '3|2',
        initialTimeMs: 180000,
        incrementMs: 2000,
        isFast: true,
      );
    case 6:
      return const TimerConfigPart(
        type: 6,
        name: 'Blitz',
        description: '5 min',
        initialTimeMs: 300000,
        incrementMs: 0,
        isFast: true,
      );
    case 8:
      return const TimerConfigPart(
        type: 8,
        name: 'Rapid',
        description: '10 min',
        initialTimeMs: 600000,
        incrementMs: 0,
        isFast: false,
      );
    case 10:
      return const TimerConfigPart(
        type: 10,
        name: 'Rapid',
        description: '15|10',
        initialTimeMs: 900000,
        incrementMs: 10000,
        isFast: false,
      );
    case 14:
      final m = (customMinutes ?? 10).clamp(1, 180);
      final incSec = (customIncrement ?? 0).clamp(0, 60);
      return TimerConfigPart(
        type: 14,
        name: 'Custom',
        description: '$m min',
        initialTimeMs: m * 60000,
        incrementMs: incSec * 1000,
        isFast: m <= 5,
      );
    default:
      return TimerConfigPart(
        type: type,
        name: 'Preset',
        description: 'Demo',
        initialTimeMs: 300000,
        incrementMs: 0,
        isFast: true,
      );
  }
}

GameStatus mockStatusForNewGame(
  GameStatus template, {
  required int whiteSec,
  required int blackSec,
}) {
  return GameStatus(
    gameState: 'active',
    currentPlayer: 'White',
    moveCount: 0,
    whiteTime: whiteSec,
    blackTime: blackSec,
    inCheck: false,
    checkmate: false,
    stalemate: false,
    webLocked: template.webLocked,
    internetConnected: template.internetConnected,
    brightness: template.brightness,
    castlingInProgress: false,
    gameEnd: const GameEndStatus(
      ended: false,
      reason: '',
      winner: '',
      loser: '',
    ),
    pieceLifted: template.pieceLifted,
    errorState: template.errorState,
    restoreState: template.restoreState,
    boardSetupTutorial: false,
    puzzle: template.puzzle,
    guidedCaptureHintsEnabled: template.guidedCaptureHintsEnabled,
    ledGuidanceLevel: template.ledGuidanceLevel,
    matrixGuardActive: template.matrixGuardActive,
    matrixGuardConflicts: template.matrixGuardConflicts,
    matrixGuardLiftedLow: template.matrixGuardLiftedLow,
    matrixGuardLiftedHigh: template.matrixGuardLiftedHigh,
    matrixGuardDroppedLow: template.matrixGuardDroppedLow,
    matrixGuardDroppedHigh: template.matrixGuardDroppedHigh,
    lightMode: template.lightMode,
    lightState: template.lightState,
    lightR: template.lightR,
    lightG: template.lightG,
    lightB: template.lightB,
    autoLampTimeoutSec: template.autoLampTimeoutSec,
    chessHintLimit: template.chessHintLimit,
    matrixOccupied: template.matrixOccupied,
  );
}

/// Výsledek simulovaného tahu v demo režimu.
class MockRemoteMoveResult {
  MockRemoteMoveResult({
    required this.snapshot,
    required this.timer,
  });

  final GameSnapshot snapshot;
  final BoardTimerState? timer;
}

/// Důvod zamítnutí tahu z aplikace (stejná pravidla jako [mockApplyRemoteMove]).
enum RemoteMoveReject {
  finished,
  badFen,
  wrongTurn,
  illegal,
}

/// Ověření legality tahu proti [snap] před odesláním na desku (BLE nevrací 400).
RemoteMoveReject? validateRemoteMoveLegality({
  required GameSnapshot snap,
  required String from,
  required String to,
  String? promotion,
}) {
  if (snap.status.isGameFinished) return RemoteMoveReject.finished;

  final fen = fenFromSnapshot(snap);
  final chess = ch.Chess();
  if (!chess.load(fen)) return RemoteMoveReject.badFen;

  final f = from.trim().toLowerCase();
  final t = to.trim().toLowerCase();
  final whiteToMove = snap.status.currentPlayer.toLowerCase().startsWith('w');
  if ((chess.turn == ch.Color.WHITE) != whiteToMove) {
    return RemoteMoveReject.wrongTurn;
  }

  final moves = chess.generate_moves();
  final moveObj = _pickLegalMove(moves, f, t, promotion);
  if (moveObj == null) return RemoteMoveReject.illegal;
  return null;
}

ch.Move? _pickLegalMove(
  List<ch.Move> moves,
  String from,
  String to,
  String? promotion,
) {
  for (final m in moves) {
    if (m.fromAlgebraic != from || m.toAlgebraic != to) continue;
    if (m.promotion != null) {
      if (promotion == null) return null;
      if (m.promotion!.name == promotion.toLowerCase()) return m;
      continue;
    }
    return m;
  }
  return null;
}

GameSnapshot _snapshotAfterApply({
  required GameSnapshot snap,
  required ch.Chess chess,
  required ch.Move played,
  required String san,
  required GameStatus newStatus,
}) {
  final placement = chess.fen.split(' ').first;
  final board = boardFromPlacementFen(placement);
  final hist = List<GameHistoryMove>.from(snap.history.moves)
    ..add(
      GameHistoryMove(
        from: played.fromAlgebraic,
        to: played.toAlgebraic,
        piece: null,
        timestamp: DateTime.now().millisecondsSinceEpoch,
        san: san,
      ),
    );
  return GameSnapshot(
    stateVersion: (snap.stateVersion ?? 0) + 1,
    board: board,
    timestamp: DateTime.now().millisecondsSinceEpoch,
    status: newStatus,
    history: GameMoveHistory(moves: hist),
    captured: snap.captured,
    clock: null,
    gameId: snap.gameId,
  );
}

/// Lokální „deska“: aplikuje tah a vrátí nový snapshot + čas (nebo vyhodí [BoardApiException] 400).
MockRemoteMoveResult mockApplyRemoteMove({
  required GameSnapshot snap,
  required BoardTimerState? timer,
  required String from,
  required String to,
  String? promotion,
}) {
  final reject = validateRemoteMoveLegality(
    snap: snap,
    from: from,
    to: to,
    promotion: promotion,
  );
  if (reject != null) {
    switch (reject) {
      case RemoteMoveReject.finished:
        throw BoardApiException(
          'Game already finished',
          statusCode: 400,
          detail: 'Game already finished',
        );
      case RemoteMoveReject.badFen:
        throw BoardApiException(
          'Invalid position',
          statusCode: 400,
          detail: 'Invalid position',
        );
      case RemoteMoveReject.wrongTurn:
        throw BoardApiException(
          'Side to move mismatch',
          statusCode: 400,
          detail: 'Side to move mismatch',
        );
      case RemoteMoveReject.illegal:
        throw BoardApiException(
          'Illegal move',
          statusCode: 400,
          detail: 'Illegal move',
        );
    }
  }

  final fen = fenFromSnapshot(snap);
  final chess = ch.Chess();
  chess.load(fen);
  final f = from.trim().toLowerCase();
  final t = to.trim().toLowerCase();
  final moves = chess.generate_moves();
  final moveObj = _pickLegalMove(moves, f, t, promotion)!;
  final san = chess.move_to_san(moveObj);
  chess.make_move(moveObj);

  final moverWasWhite =
      snap.status.currentPlayer.toLowerCase().startsWith('w');
  final finishedMate = chess.in_checkmate;
  final finishedStale = chess.in_stalemate;
  final finishedDraw = chess.in_draw && !finishedMate && !finishedStale;

  String gameState = 'active';
  GameEndStatus? gameEnd = snap.status.gameEnd;

  if (finishedMate) {
    gameState = 'finished';
    gameEnd = GameEndStatus(
      ended: true,
      reason: 'checkmate',
      winner: moverWasWhite ? 'White' : 'Black',
      loser: moverWasWhite ? 'Black' : 'White',
    );
  } else if (finishedStale) {
    gameState = 'finished';
    gameEnd = const GameEndStatus(
      ended: true,
      reason: 'stalemate',
      winner: '',
      loser: '',
    );
  } else if (finishedDraw) {
    gameState = 'finished';
    gameEnd = const GameEndStatus(
      ended: true,
      reason: 'draw',
      winner: '',
      loser: '',
    );
  }

  final nextPlayer = chess.turn == ch.Color.WHITE ? 'White' : 'Black';
  final wSec = timer != null && timer.isTimeControlEnabled
      ? timer.whiteTimeMs ~/ 1000
      : (snap.status.whiteTime ?? 0);
  final bSec = timer != null && timer.isTimeControlEnabled
      ? timer.blackTimeMs ~/ 1000
      : (snap.status.blackTime ?? 0);

  final newStatus = GameStatus(
    gameState: gameState,
    currentPlayer: nextPlayer,
    moveCount: snap.status.moveCount + 1,
    whiteTime: wSec,
    blackTime: bSec,
    inCheck: chess.in_check,
    checkmate: finishedMate ? true : false,
    stalemate: finishedStale ? true : false,
    webLocked: snap.status.webLocked,
    internetConnected: snap.status.internetConnected,
    brightness: snap.status.brightness,
    castlingInProgress: false,
    gameEnd: gameEnd,
    pieceLifted: snap.status.pieceLifted?.copyWith(
          lifted: false,
          piece: ' ',
          notation: '',
        ) ??
        snap.status.pieceLifted,
    errorState: snap.status.errorState,
    restoreState: snap.status.restoreState,
    boardSetupTutorial: snap.status.boardSetupTutorial,
    puzzle: snap.status.puzzle,
    guidedCaptureHintsEnabled: snap.status.guidedCaptureHintsEnabled,
    ledGuidanceLevel: snap.status.ledGuidanceLevel,
    matrixGuardActive: snap.status.matrixGuardActive,
    matrixGuardConflicts: snap.status.matrixGuardConflicts,
    matrixGuardLiftedLow: snap.status.matrixGuardLiftedLow,
    matrixGuardLiftedHigh: snap.status.matrixGuardLiftedHigh,
    matrixGuardDroppedLow: snap.status.matrixGuardDroppedLow,
    matrixGuardDroppedHigh: snap.status.matrixGuardDroppedHigh,
    lightMode: snap.status.lightMode,
    lightState: snap.status.lightState,
    lightR: snap.status.lightR,
    lightG: snap.status.lightG,
    lightB: snap.status.lightB,
    autoLampTimeoutSec: snap.status.autoLampTimeoutSec,
    chessHintLimit: snap.status.chessHintLimit,
    matrixOccupied: snap.status.matrixOccupied,
  );

  final newSnap = _snapshotAfterApply(
    snap: snap,
    chess: chess,
    played: moveObj,
    san: san,
    newStatus: newStatus,
  );

  BoardTimerState? newTimer = timer;
  if (timer != null && timer.isTimeControlEnabled) {
    var wMs = timer.whiteTimeMs;
    var bMs = timer.blackTimeMs;
    final inc = timer.config.incrementMs;
    if (moverWasWhite) {
      wMs += inc;
    } else {
      bMs += inc;
    }
    final gameOver = newSnap.status.isGameFinished;
    newTimer = BoardTimerState(
      whiteTimeMs: wMs,
      blackTimeMs: bMs,
      timerRunning: gameOver ? false : timer.timerRunning,
      isWhiteTurn: chess.turn == ch.Color.WHITE,
      gamePaused: timer.gamePaused,
      timeExpired: timer.timeExpired,
      config: timer.config,
      totalMoves: newSnap.history.moves.length,
      avgMoveTimeMs: timer.avgMoveTimeMs,
    );
  }

  return MockRemoteMoveResult(snapshot: newSnap, timer: newTimer);
}
