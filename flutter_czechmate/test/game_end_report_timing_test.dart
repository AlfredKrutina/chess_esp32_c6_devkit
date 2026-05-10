import 'package:czechmate/core/models/game_snapshot.dart';
import 'package:czechmate/core/utils/game_end_report_timing.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  test('thinkPlyPoints converts firmware ms deltas to seconds', () {
    final moves = [
      const GameHistoryMove(from: 'e2', to: 'e4', timestamp: 1000),
      const GameHistoryMove(from: 'e7', to: 'e5', timestamp: 3500),
      const GameHistoryMove(from: 'g1', to: 'f3', timestamp: 4000),
    ];
    final pts = GameEndReportTiming.thinkPlyPoints(moves);
    expect(pts[0].secondsFromPrevious, isNull);
    expect(pts[1].secondsFromPrevious, closeTo(2.5, 1e-9));
    expect(pts[2].secondsFromPrevious, closeTo(0.5, 1e-9));
    final cum = GameEndReportTiming.cumulativePoints(pts);
    expect(cum.last.totalSeconds, closeTo(3.0, 1e-9));
  });

  test('thinkPlyPoints drops outlier move intervals', () {
    final moves = [
      const GameHistoryMove(from: 'e2', to: 'e4', timestamp: 1000),
      const GameHistoryMove(from: 'e7', to: 'e5', timestamp: 1000 + 5000),
      const GameHistoryMove(from: 'g1', to: 'f3', timestamp: 1000 + 5000 + 5000000),
    ];
    final pts = GameEndReportTiming.thinkPlyPoints(moves);
    expect(pts[1].secondsFromPrevious, closeTo(5.0, 1e-9));
    expect(pts[2].secondsFromPrevious, isNull);
  });
}
