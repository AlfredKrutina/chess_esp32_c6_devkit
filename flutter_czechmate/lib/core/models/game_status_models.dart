// Parita s GameStatus.swift / BoardHTTPModels — JSON ze snapshot.status.

class GameEndStatus {
  const GameEndStatus({
    required this.ended,
    this.reason,
    this.winner,
    this.loser,
  });

  final bool ended;
  final String? reason;
  final String? winner;
  final String? loser;

  factory GameEndStatus.fromJson(Map<String, dynamic> json) {
    return GameEndStatus(
      ended: json['ended'] as bool? ?? false,
      reason: json['reason'] as String?,
      winner: json['winner'] as String?,
      loser: json['loser'] as String?,
    );
  }
}

class PieceLifted {
  const PieceLifted({
    required this.lifted,
    required this.row,
    required this.col,
    required this.piece,
    required this.notation,
  });

  final bool lifted;
  final int row;
  final int col;
  final String piece;
  final String notation;

  factory PieceLifted.fromJson(Map<String, dynamic> json) {
    return PieceLifted(
      lifted: json['lifted'] as bool? ?? false,
      row: (json['row'] as num?)?.toInt() ?? 0,
      col: (json['col'] as num?)?.toInt() ?? 0,
      piece: json['piece'] as String? ?? '',
      notation: json['notation'] as String? ?? '',
    );
  }

  PieceLifted copyWith({
    bool? lifted,
    int? row,
    int? col,
    String? piece,
    String? notation,
  }) {
    return PieceLifted(
      lifted: lifted ?? this.lifted,
      row: row ?? this.row,
      col: col ?? this.col,
      piece: piece ?? this.piece,
      notation: notation ?? this.notation,
    );
  }
}

class ErrorStateJson {
  const ErrorStateJson({
    required this.active,
    this.invalidPos,
    this.originalPos,
    this.errorCount,
  });

  final bool active;
  final String? invalidPos;
  final String? originalPos;
  final int? errorCount;

  factory ErrorStateJson.fromJson(Map<String, dynamic> json) {
    return ErrorStateJson(
      active: json['active'] as bool? ?? false,
      invalidPos: json['invalid_pos'] as String?,
      originalPos: json['original_pos'] as String?,
      errorCount: (json['error_count'] as num?)?.toInt(),
    );
  }
}

class RestoreStateJson {
  const RestoreStateJson({
    this.snapshotLoaded,
    this.snapshotFallbackUsed,
    this.snapshotRestoreFailed,
    this.snapshotSaveFailed,
    this.resyncRequired,
    this.bootNewGameTriggered,
  });

  final bool? snapshotLoaded;
  final bool? snapshotFallbackUsed;
  final bool? snapshotRestoreFailed;
  final bool? snapshotSaveFailed;
  final bool? resyncRequired;
  final bool? bootNewGameTriggered;

  factory RestoreStateJson.fromJson(Map<String, dynamic> json) {
    return RestoreStateJson(
      snapshotLoaded: json['snapshot_loaded'] as bool?,
      snapshotFallbackUsed: json['snapshot_fallback_used'] as bool?,
      snapshotRestoreFailed: json['snapshot_restore_failed'] as bool?,
      snapshotSaveFailed: json['snapshot_save_failed'] as bool?,
      resyncRequired: json['resync_required'] as bool?,
      bootNewGameTriggered: json['boot_new_game_triggered'] as bool?,
    );
  }
}

class PuzzleStatusJson {
  const PuzzleStatusJson({
    this.active,
    this.setupActive,
    this.setupId,
    this.physicalMatch,
    this.fen,
    this.id,
    this.difficulty,
    this.title,
    this.teaser,
    this.feedback,
    this.message,
  });

  final bool? active;
  final bool? setupActive;
  final int? setupId;
  final bool? physicalMatch;
  final String? fen;
  final int? id;
  final int? difficulty;
  final String? title;
  final String? teaser;
  final String? feedback;
  final String? message;

  factory PuzzleStatusJson.fromJson(Map<String, dynamic> json) {
    return PuzzleStatusJson(
      active: json['active'] as bool?,
      setupActive: json['setup_active'] as bool?,
      setupId: (json['setup_id'] as num?)?.toInt(),
      physicalMatch: json['physical_match'] as bool?,
      fen: json['fen'] as String?,
      id: (json['id'] as num?)?.toInt(),
      difficulty: (json['difficulty'] as num?)?.toInt(),
      title: json['title'] as String?,
      teaser: json['teaser'] as String?,
      feedback: json['feedback'] as String?,
      message: json['message'] as String?,
    );
  }
}

class OpeningTrainingStatusJson {
  const OpeningTrainingStatusJson({
    this.active,
    this.feedback,
    this.playerPlyIndex,
    this.playerPlyTotal,
    this.expectedFrom,
    this.expectedTo,
    this.awaitingCheckpointAck,
  });

  final bool? active;
  final String? feedback;
  final int? playerPlyIndex;
  final int? playerPlyTotal;
  final String? expectedFrom;
  final String? expectedTo;
  final bool? awaitingCheckpointAck;

  factory OpeningTrainingStatusJson.fromJson(Map<String, dynamic> json) {
    return OpeningTrainingStatusJson(
      active: json['active'] as bool?,
      feedback: json['feedback'] as String?,
      playerPlyIndex: (json['player_ply_index'] as num?)?.toInt(),
      playerPlyTotal: (json['player_ply_total'] as num?)?.toInt(),
      expectedFrom: json['expected_from'] as String?,
      expectedTo: json['expected_to'] as String?,
      awaitingCheckpointAck: json['awaiting_checkpoint_ack'] as bool?,
    );
  }
}

class GameStatus {
  const GameStatus({
    required this.gameState,
    required this.currentPlayer,
    required this.moveCount,
    this.whiteTime,
    this.blackTime,
    required this.inCheck,
    this.checkmate,
    this.stalemate,
    this.webLocked,
    this.internetConnected,
    this.brightness,
    this.castlingInProgress,
    this.gameEnd,
    this.pieceLifted,
    this.errorState,
    this.restoreState,
    this.boardSetupTutorial,
    this.puzzle,
    this.openingTraining,
    this.guidedCaptureHintsEnabled,
    this.ledGuidanceLevel,
    this.matrixGuardActive,
    this.matrixGuardConflicts,
    this.matrixGuardLiftedLow,
    this.matrixGuardLiftedHigh,
    this.matrixGuardDroppedLow,
    this.matrixGuardDroppedHigh,
    this.lightMode,
    this.lightState,
    this.lightR,
    this.lightG,
    this.lightB,
    this.autoLampTimeoutSec,
    this.chessHintLimit,
    this.matrixOccupied,
  });

  final String gameState;
  final String currentPlayer;
  final int moveCount;
  final int? whiteTime;
  final int? blackTime;
  final bool inCheck;
  final bool? checkmate;
  final bool? stalemate;
  final bool? webLocked;
  final bool? internetConnected;
  final int? brightness;
  final bool? castlingInProgress;
  final GameEndStatus? gameEnd;
  final PieceLifted? pieceLifted;
  final ErrorStateJson? errorState;
  final RestoreStateJson? restoreState;
  final bool? boardSetupTutorial;
  final PuzzleStatusJson? puzzle;
  final OpeningTrainingStatusJson? openingTraining;
  final bool? guidedCaptureHintsEnabled;
  final int? ledGuidanceLevel;
  final bool? matrixGuardActive;
  final int? matrixGuardConflicts;
  final int? matrixGuardLiftedLow;
  final int? matrixGuardLiftedHigh;
  final int? matrixGuardDroppedLow;
  final int? matrixGuardDroppedHigh;
  final String? lightMode;
  final bool? lightState;
  final int? lightR;
  final int? lightG;
  final int? lightB;
  final int? autoLampTimeoutSec;
  final int? chessHintLimit;
  final List<int>? matrixOccupied;

  bool get isTimerRunning {
    final s = gameState.toLowerCase();
    return s == 'active' || s == 'playing';
  }

  bool get isGameFinished {
    final s = gameState.toLowerCase();
    if (s == 'finished') return true;
    return gameEnd?.ended == true;
  }

  bool get isErrorRecoveryActive => errorState?.active == true;

  factory GameStatus.fromJson(Map<String, dynamic> json) {
    return GameStatus(
      gameState: json['game_state'] as String? ?? '',
      currentPlayer: json['current_player'] as String? ?? 'White',
      moveCount: (json['move_count'] as num?)?.toInt() ?? 0,
      whiteTime: (json['white_time'] as num?)?.toInt(),
      blackTime: (json['black_time'] as num?)?.toInt(),
      inCheck: json['in_check'] as bool? ?? false,
      checkmate: json['checkmate'] as bool?,
      stalemate: json['stalemate'] as bool?,
      webLocked: json['web_locked'] as bool?,
      internetConnected: json['internet_connected'] as bool?,
      brightness: (json['brightness'] as num?)?.toInt(),
      castlingInProgress: json['castling_in_progress'] as bool?,
      gameEnd: json['game_end'] != null
          ? GameEndStatus.fromJson(
              Map<String, dynamic>.from(json['game_end'] as Map),
            )
          : null,
      pieceLifted: json['piece_lifted'] != null
          ? PieceLifted.fromJson(
              Map<String, dynamic>.from(json['piece_lifted'] as Map),
            )
          : null,
      errorState: json['error_state'] != null
          ? ErrorStateJson.fromJson(
              Map<String, dynamic>.from(json['error_state'] as Map),
            )
          : null,
      restoreState: json['restore_state'] != null
          ? RestoreStateJson.fromJson(
              Map<String, dynamic>.from(json['restore_state'] as Map),
            )
          : null,
      boardSetupTutorial: json['board_setup_tutorial'] as bool?,
      puzzle: json['puzzle'] != null
          ? PuzzleStatusJson.fromJson(
              Map<String, dynamic>.from(json['puzzle'] as Map),
            )
          : null,
      openingTraining: json['opening_training'] != null
          ? OpeningTrainingStatusJson.fromJson(
              Map<String, dynamic>.from(json['opening_training'] as Map),
            )
          : null,
      guidedCaptureHintsEnabled: json['guided_capture_hints_enabled'] as bool?,
      ledGuidanceLevel: (json['led_guidance_level'] as num?)?.toInt(),
      matrixGuardActive: json['matrix_guard_active'] as bool?,
      matrixGuardConflicts: (json['matrix_guard_conflicts'] as num?)?.toInt(),
      matrixGuardLiftedLow: (json['matrix_guard_lifted_low'] as num?)?.toInt(),
      matrixGuardLiftedHigh:
          (json['matrix_guard_lifted_high'] as num?)?.toInt(),
      matrixGuardDroppedLow:
          (json['matrix_guard_dropped_low'] as num?)?.toInt(),
      matrixGuardDroppedHigh:
          (json['matrix_guard_dropped_high'] as num?)?.toInt(),
      lightMode: json['light_mode'] as String?,
      lightState: json['light_state'] as bool?,
      lightR: (json['light_r'] as num?)?.toInt(),
      lightG: (json['light_g'] as num?)?.toInt(),
      lightB: (json['light_b'] as num?)?.toInt(),
      autoLampTimeoutSec: (json['auto_lamp_timeout_sec'] as num?)?.toInt(),
      chessHintLimit: (json['chess_hint_limit'] as num?)?.toInt(),
      matrixOccupied: (json['matrix_occupied'] as List<dynamic>?)
          ?.map((e) => (e as num).toInt())
          .toList(),
    );
  }

  /// Sloučení přechodného `light_state: false` po HTTP změně lampy (Swift parita).
  GameStatus coalescingTransientLampOff(GameStatus previous) {
    if (previous.lightState != true || lightState != false) return this;
    return GameStatus(
      gameState: gameState,
      currentPlayer: currentPlayer,
      moveCount: moveCount,
      whiteTime: whiteTime,
      blackTime: blackTime,
      inCheck: inCheck,
      checkmate: checkmate,
      stalemate: stalemate,
      webLocked: webLocked,
      internetConnected: internetConnected,
      brightness: brightness ?? previous.brightness,
      castlingInProgress: castlingInProgress,
      gameEnd: gameEnd,
      pieceLifted: pieceLifted,
      errorState: errorState,
      restoreState: restoreState,
      boardSetupTutorial: boardSetupTutorial,
      puzzle: puzzle,
      openingTraining: openingTraining,
      guidedCaptureHintsEnabled: guidedCaptureHintsEnabled,
      ledGuidanceLevel: ledGuidanceLevel,
      matrixGuardActive: matrixGuardActive,
      matrixGuardConflicts: matrixGuardConflicts,
      matrixGuardLiftedLow: matrixGuardLiftedLow,
      matrixGuardLiftedHigh: matrixGuardLiftedHigh,
      matrixGuardDroppedLow: matrixGuardDroppedLow,
      matrixGuardDroppedHigh: matrixGuardDroppedHigh,
      lightMode: lightMode ?? previous.lightMode,
      lightState: true,
      lightR: lightR ?? previous.lightR,
      lightG: lightG ?? previous.lightG,
      lightB: lightB ?? previous.lightB,
      autoLampTimeoutSec: autoLampTimeoutSec ?? previous.autoLampTimeoutSec,
      chessHintLimit: chessHintLimit ?? previous.chessHintLimit,
      matrixOccupied: matrixOccupied ?? previous.matrixOccupied,
    );
  }

  /// Po převrácení řádků `board` MCU→rank8: zrcadlit `piece_lifted.row`.
  GameStatus withPieceLiftedRowMirroredVertically() {
    final pl = pieceLifted;
    if (pl == null || !pl.lifted) return this;
    final mirrored = pl.copyWith(row: 7 - pl.row);
    return copyWithPieceLifted(mirrored);
  }

  GameStatus copyWithPieceLifted(PieceLifted? pl) {
    return GameStatus(
      gameState: gameState,
      currentPlayer: currentPlayer,
      moveCount: moveCount,
      whiteTime: whiteTime,
      blackTime: blackTime,
      inCheck: inCheck,
      checkmate: checkmate,
      stalemate: stalemate,
      webLocked: webLocked,
      internetConnected: internetConnected,
      brightness: brightness,
      castlingInProgress: castlingInProgress,
      gameEnd: gameEnd,
      pieceLifted: pl,
      errorState: errorState,
      restoreState: restoreState,
      boardSetupTutorial: boardSetupTutorial,
      puzzle: puzzle,
      openingTraining: openingTraining,
      guidedCaptureHintsEnabled: guidedCaptureHintsEnabled,
      ledGuidanceLevel: ledGuidanceLevel,
      matrixGuardActive: matrixGuardActive,
      matrixGuardConflicts: matrixGuardConflicts,
      matrixGuardLiftedLow: matrixGuardLiftedLow,
      matrixGuardLiftedHigh: matrixGuardLiftedHigh,
      matrixGuardDroppedLow: matrixGuardDroppedLow,
      matrixGuardDroppedHigh: matrixGuardDroppedHigh,
      lightMode: lightMode,
      lightState: lightState,
      lightR: lightR,
      lightG: lightG,
      lightB: lightB,
      autoLampTimeoutSec: autoLampTimeoutSec,
      chessHintLimit: chessHintLimit,
      matrixOccupied: matrixOccupied,
    );
  }
}
