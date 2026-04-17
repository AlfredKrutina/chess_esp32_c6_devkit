import 'board_timer_state.dart';
import 'game_status_models.dart';

/// Odpověď `GET /api/game/snapshot` / BLE / WS — `GameSnapshot.swift`.
class GameSnapshot {
  const GameSnapshot({
    this.stateVersion,
    required this.board,
    required this.timestamp,
    required this.status,
    required this.history,
    required this.captured,
    this.clock,
    this.gameId,
  });

  final int? stateVersion;
  final List<List<String>> board;
  final int timestamp;
  final GameStatus status;
  final GameMoveHistory history;
  final CapturedPieces captured;
  final BoardTimerState? clock;
  final String? gameId;

  String? get etagTag =>
      stateVersion != null ? '"$stateVersion"' : null;

  GameSnapshot replacingStatus(GameStatus newStatus) {
    return GameSnapshot(
      stateVersion: stateVersion,
      board: board,
      timestamp: timestamp,
      status: newStatus,
      history: history,
      captured: captured,
      clock: clock,
      gameId: gameId,
    );
  }

  factory GameSnapshot.fromJson(Map<String, dynamic> json) {
    final rawBoard = json['board'] as List<dynamic>? ?? [];
    BoardTimerState? clock;
    final clockRaw = json['clock'];
    if (clockRaw is Map) {
      clock = BoardTimerState.fromJson(Map<String, dynamic>.from(clockRaw));
    }
    return GameSnapshot(
      stateVersion: (json['state_version'] as num?)?.toInt(),
      board: rawBoard
          .map((row) => (row as List<dynamic>).map((c) => '$c').toList())
          .toList(),
      timestamp: (json['timestamp'] as num?)?.toInt() ?? 0,
      status: GameStatus.fromJson(
        Map<String, dynamic>.from(json['status'] as Map? ?? const {}),
      ),
      history: GameMoveHistory.fromJson(
        Map<String, dynamic>.from(json['history'] as Map? ?? const {}),
      ),
      captured: CapturedPieces.fromJson(
        Map<String, dynamic>.from(json['captured'] as Map? ?? const {}),
      ),
      clock: clock,
      gameId: json['game_id'] as String?,
    );
  }
}

class GameMoveHistory {
  const GameMoveHistory({required this.moves});

  final List<GameHistoryMove> moves;

  factory GameMoveHistory.fromJson(Map<String, dynamic> json) {
    final raw = json['moves'] as List<dynamic>? ?? [];
    return GameMoveHistory(
      moves: raw
          .map((e) => GameHistoryMove.fromJson(Map<String, dynamic>.from(e as Map)))
          .toList(),
    );
  }
}

class GameHistoryMove {
  const GameHistoryMove({
    this.from,
    this.to,
    this.piece,
    this.timestamp,
    this.san,
  });

  final String? from;
  final String? to;
  final String? piece;
  final int? timestamp;
  final String? san;

  factory GameHistoryMove.fromJson(Map<String, dynamic> json) {
    return GameHistoryMove(
      from: json['from'] as String?,
      to: json['to'] as String?,
      piece: json['piece'] as String?,
      timestamp: (json['timestamp'] as num?)?.toInt(),
      san: json['san'] as String?,
    );
  }
}

class CapturedPieces {
  const CapturedPieces({
    required this.whiteCaptured,
    required this.blackCaptured,
  });

  final List<String> whiteCaptured;
  final List<String> blackCaptured;

  factory CapturedPieces.fromJson(Map<String, dynamic> json) {
    return CapturedPieces(
      whiteCaptured:
          (json['white_captured'] as List<dynamic>?)?.map((e) => '$e').toList() ??
              const [],
      blackCaptured:
          (json['black_captured'] as List<dynamic>?)?.map((e) => '$e').toList() ??
              const [],
    );
  }
}
