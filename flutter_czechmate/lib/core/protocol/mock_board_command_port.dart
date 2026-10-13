import '../models/board_timer_state.dart';
import '../models/game_snapshot.dart';
import '../services/board_api_exception.dart';
import '../services/mock_board_simulator.dart';
import 'board_command_payload.dart';
import 'board_command_port.dart';

/// Why a mock (or missing) session refuses a command. The session maps these
/// to the same localized strings the UI already shows.
enum BoardCommandRefuse {
  connectBeforeMoves,
  demoNoSnapshot,
  setupNeedsConnection,
  hintsNeedConnection,
  noActiveSession,
  brightnessNeedsBoard,
  lampNeedsBoard,
  gameLampNeedsBoard,
  autoLampNeedsBoard,
  otaMock,
}

/// Local simulator. Does not speak HTTP or BLE. Reconnect must not call
/// [postNewGame]; that resets the demo position on purpose.
class MockBoardCommandPort implements BoardCommandPort {
  MockBoardCommandPort({
    required this.readSnapshot,
    required this.readTimer,
    required this.apply,
    required this.plainNewGame,
    required this.resetWithTime,
    required this.onGameFinished,
    required this.onTimerResumed,
    required this.refuse,
  });

  final GameSnapshot? Function() readSnapshot;
  final BoardTimerState? Function() readTimer;
  final void Function(GameSnapshot snapshot, BoardTimerState? timer) apply;
  final Future<void> Function() plainNewGame;
  final Future<void> Function(TimerConfigPart config) resetWithTime;
  final void Function() onGameFinished;
  final void Function() onTimerResumed;
  final String Function(BoardCommandRefuse kind) refuse;

  TimerConfigPart? _pendingTime;

  @override
  Future<void> postMove({
    required String from,
    required String to,
    String? promotion,
  }) async {
    final snap = readSnapshot();
    if (snap == null) {
      throw StateError(refuse(BoardCommandRefuse.demoNoSnapshot));
    }
    final letter = BoardCommandPayload.promotionLetter(promotion);
    final result = mockApplyRemoteMove(
      snap: snap,
      timer: readTimer(),
      from: from,
      to: to,
      promotion: letter,
    );
    apply(result.snapshot, result.timer);
    if (result.snapshot.status.isGameFinished) onGameFinished();
  }

  @override
  Future<void> postNewGame({String? fen}) async {
    final pending = _pendingTime;
    _pendingTime = null;
    if (pending != null && (fen == null || fen.trim().isEmpty)) {
      await resetWithTime(pending);
      return;
    }
    await plainNewGame();
  }

  @override
  Future<void> postResign({required String side}) async {
    final snap = readSnapshot();
    if (snap == null) {
      throw StateError(refuse(BoardCommandRefuse.demoNoSnapshot));
    }
    final onMoveWhite = snap.status.currentPlayer.toLowerCase().startsWith('w');
    final sideWhite = side.trim().toLowerCase() == 'white';
    if (snap.status.isGameFinished || onMoveWhite != sideWhite) {
      throw BoardApiException(
        'Resign refused',
        statusCode: 409,
        detail: snap.status.isGameFinished ? 'not_running' : 'side_mismatch',
      );
    }
    final result = mockApplyResign(snap: snap, timer: readTimer());
    apply(result.snapshot, result.timer);
    onGameFinished();
  }

  @override
  Future<void> postTimerConfig({
    required int type,
    int? customMinutes,
    int? customIncrement,
  }) async {
    _pendingTime = mockTimerConfigForFirmware(
      type: type,
      customMinutes: customMinutes,
      customIncrement: customIncrement,
    );
  }

  @override
  Future<void> postTimerPause() async {
    final timer = readTimer();
    if (timer == null || !timer.isTimeControlEnabled) return;
    final snap = readSnapshot();
    if (snap == null) return;
    apply(
      snap,
      BoardTimerState(
        whiteTimeMs: timer.whiteTimeMs,
        blackTimeMs: timer.blackTimeMs,
        timerRunning: false,
        isWhiteTurn: timer.isWhiteTurn,
        gamePaused: true,
        timeExpired: timer.timeExpired,
        config: timer.config,
        totalMoves: timer.totalMoves,
        avgMoveTimeMs: timer.avgMoveTimeMs,
      ),
    );
  }

  @override
  Future<void> postTimerResume() async {
    final timer = readTimer();
    final snap = readSnapshot();
    if (timer == null || !timer.isTimeControlEnabled || snap == null) return;
    final finished = snap.status.isGameFinished;
    apply(
      snap,
      BoardTimerState(
        whiteTimeMs: timer.whiteTimeMs,
        blackTimeMs: timer.blackTimeMs,
        timerRunning: !finished,
        isWhiteTurn: timer.isWhiteTurn,
        gamePaused: false,
        timeExpired: timer.timeExpired,
        config: timer.config,
        totalMoves: timer.totalMoves,
        avgMoveTimeMs: timer.avgMoveTimeMs,
      ),
    );
    onTimerResumed();
  }

  @override
  Future<void> postHintDestination(String square) async {
    throw StateError(refuse(BoardCommandRefuse.hintsNeedConnection));
  }

  @override
  Future<void> postHintClear() async {}

  @override
  Future<void> postGuardClear() async {
    throw StateError(refuse(BoardCommandRefuse.hintsNeedConnection));
  }

  @override
  Future<void> postSetupTutorial(String action) async {
    throw StateError(refuse(BoardCommandRefuse.setupNeedsConnection));
  }

  @override
  Future<void> postOpening(Map<String, dynamic> body) async {
    throw StateError(refuse(BoardCommandRefuse.setupNeedsConnection));
  }

  @override
  Future<void> postPuzzle(Map<String, dynamic> body) async {
    throw StateError(refuse(BoardCommandRefuse.setupNeedsConnection));
  }

  @override
  Future<void> postBrightness(int percent) async {
    throw StateError(refuse(BoardCommandRefuse.brightnessNeedsBoard));
  }

  @override
  Future<void> postLightCommand({
    required bool lampState,
    required int r,
    required int g,
    required int b,
  }) async {
    throw StateError(refuse(BoardCommandRefuse.lampNeedsBoard));
  }

  @override
  Future<void> postLightGameMode() async {
    throw StateError(refuse(BoardCommandRefuse.gameLampNeedsBoard));
  }

  @override
  Future<void> postAutoLampTimeout(int seconds) async {
    throw StateError(refuse(BoardCommandRefuse.autoLampNeedsBoard));
  }

  @override
  Future<void> postOtaStart(String firmwareUrl) async {
    throw StateError(refuse(BoardCommandRefuse.otaMock));
  }
}
