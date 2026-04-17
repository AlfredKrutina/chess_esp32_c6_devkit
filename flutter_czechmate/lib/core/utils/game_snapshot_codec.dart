import 'dart:convert';

import '../models/game_snapshot.dart';
import 'game_json_repair.dart';

/// Dekód + normalizace řádků desky — `GameSnapshot+BoardNormalization.swift`.
class GameSnapshotCodec {
  static GameSnapshot decodeRepairingAndNormalizing(String raw) {
    final repaired = GameJsonRepair.repairStatusString(raw);
    final map = jsonDecode(repaired) as Map<String, dynamic>;
    final snap = GameSnapshot.fromJson(map);
    return snap.normalizingBoardRowsFromFirmwareExportIfNeeded();
  }

  static GameSnapshot decodeBytes(List<int> bytes) {
    return decodeRepairingAndNormalizing(utf8.decode(bytes));
  }
}

extension GameSnapshotBoardNormalization on GameSnapshot {
  GameSnapshot normalizingBoardRowsFromFirmwareExportIfNeeded() {
    if (!boardAppearsFirmwareWhiteNearRowZero(board)) return this;
    final flipped = board.reversed.toList();
    final newStatus = status.withPieceLiftedRowMirroredVertically();
    return GameSnapshot(
      stateVersion: stateVersion,
      board: flipped,
      timestamp: timestamp,
      status: newStatus,
      history: history,
      captured: captured,
      clock: clock,
      gameId: gameId,
    );
  }
}

bool boardAppearsFirmwareWhiteNearRowZero(List<List<String>> board) {
  if (board.length != 8) return false;
  int? whiteKingRow;
  int? blackKingRow;
  for (var r = 0; r < 8; r++) {
    if (board[r].length != 8) return false;
    for (var c = 0; c < 8; c++) {
      final raw = board[r][c].trim();
      if (raw.isEmpty) continue;
      final ch = raw.codeUnitAt(0);
      if (ch == 75 /* K */) whiteKingRow = r;
      if (ch == 107 /* k */) blackKingRow = r;
    }
  }
  final wr = whiteKingRow;
  final br = blackKingRow;
  if (wr == null || br == null) return false;
  return wr < br;
}
