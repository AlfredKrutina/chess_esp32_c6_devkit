import 'dart:convert';

import 'package:flutter/services.dart';
import 'package:czechmate/core/models/game_snapshot.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  test('decodes golden snapshot without errors', () async {
    final jsonString = await rootBundle.loadString('assets/snapshot_golden.json');
    
    // Parse
    final dynamic jsonMap = jsonDecode(jsonString);
    final snapshot = GameSnapshot.fromJson(jsonMap as Map<String, dynamic>);
    
    expect(snapshot.stateVersion, 1);
    expect(snapshot.board.length, 8);
    expect(snapshot.board[0].length, 8);
    expect(snapshot.status.gameState, 'active');
    expect(snapshot.status.currentPlayer, 'White');
    expect(snapshot.status.isTimerRunning, true);
    expect(snapshot.status.whiteTime, 300);
    expect(snapshot.status.brightness, 50);
    expect(snapshot.status.guidedCaptureHintsEnabled, true);
    
    final normalized = snapshot.normalizingBoardRowsFromFirmwareExportIfNeeded();
    expect(normalized.board.length, 8);
    
  });
}
