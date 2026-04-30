import '../utils/game_end_report_timing.dart';
import 'game_snapshot.dart';

enum GameResultState {
  whiteWon,
  blackWon,
  draw,
  unknown
}

class GameEndReportModel {
  GameEndReportModel({
    required this.result,
    required this.reason,
    required this.durationSec,
    required this.whiteAvgMoveSec,
    required this.blackAvgMoveSec,
    required this.totalMoves,
    required this.capturedValueWhite,
    required this.capturedValueBlack,
  });

  final GameResultState result;
  final String reason;
  final int durationSec;
  final double whiteAvgMoveSec;
  final double blackAvgMoveSec;
  final int totalMoves;
  final int capturedValueWhite;
  final int capturedValueBlack;

  static GameEndReportModel fromSnapshot(GameSnapshot snap) {
    GameResultState res = GameResultState.unknown;
    String rsn = 'Unknown result';

    final ge = snap.status.gameEnd;
    if (ge != null && ge.ended) {
      if (ge.winner?.toLowerCase() == 'white') {
        res = GameResultState.whiteWon;
      } else if (ge.winner?.toLowerCase() == 'black') {
        res = GameResultState.blackWon;
      } else {
        res = GameResultState.draw;
      }
      rsn = ge.reason ?? 'Game over';
    } else if (snap.status.gameState.toLowerCase() == 'finished') {
      if (snap.status.checkmate == true) rsn = 'Checkmate';
      else if (snap.status.stalemate == true) rsn = 'Stalemate';
      else rsn = 'Finished';
    }

    // Tahové časové razítka z firmware: esp_timer_get_time()/1000 → milisekundy od bootu.
    int tDuration = 0;
    double wAvg = 0.0;
    double bAvg = 0.0;

    final moves = snap.history.moves;
    if (moves.length >= 2) {
      final first = moves.first.timestamp ?? 0;
      final last = moves.last.timestamp ?? 0;
      if (last >= first && first > 0) {
        tDuration = ((last - first) / 1000).round();
      }

      int wTotalMs = 0;
      int wCount = 0;
      int bTotalMs = 0;
      int bCount = 0;

      for (int i = 1; i < moves.length; i++) {
        final t1 = moves[i - 1].timestamp ?? 0;
        final t2 = moves[i].timestamp ?? 0;
        if (t2 >= t1 && t1 > 0) {
          final diffMs = t2 - t1;
          // i=1 → tah černého (odpověď na bílý i=0)
          if (i % 2 != 0) {
            wTotalMs += diffMs;
            wCount++;
          } else {
            bTotalMs += diffMs;
            bCount++;
          }
        }
      }

      if (wCount > 0) wAvg = wTotalMs / (wCount * 1000.0);
      if (bCount > 0) bAvg = bTotalMs / (bCount * 1000.0);

      if (tDuration <= 0) {
        final think = GameEndReportTiming.thinkPlyPoints(moves);
        var sumSec = 0.0;
        for (final p in think) {
          final d = p.secondsFromPrevious;
          if (d != null) sumSec += d;
        }
        if (sumSec > 0.5) {
          tDuration = sumSec.round();
        }
      }
    }

    // Capture values
    final capW = _calculateCaptureValue(snap.captured.whiteCaptured);
    final capB = _calculateCaptureValue(snap.captured.blackCaptured);

    return GameEndReportModel(
      result: res,
      reason: rsn,
      durationSec: tDuration,
      whiteAvgMoveSec: wAvg,
      blackAvgMoveSec: bAvg,
      totalMoves: moves.length,
      capturedValueWhite: capW,
      capturedValueBlack: capB,
    );
  }

  static int _calculateCaptureValue(List<String> pieces) {
    int v = 0;
    for (final p in pieces) {
      final cp = p.toLowerCase();
      if (cp == 'p') v += 1;
      else if (cp == 'n' || cp == 'b') v += 3;
      else if (cp == 'r') v += 5;
      else if (cp == 'q') v += 9;
    }
    return v;
  }
}
