import 'package:czechmate/core/models/game_status_models.dart';
import 'package:czechmate/core/utils/puzzle_uci.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  test('Lichess line drops the setup move and keeps the solver first', () {
    final line = normalizePuzzleLine(
      fen: '4k3/8/8/8/8/8/4P3/4K3 w - - 0 1',
      solution: const ['e2e4', 'e7e5', 'e4e5'],
      kind: PuzzleLineKind.lichess,
    );
    expect(line, isNotNull);
    expect(line!.moves, ['e7e5', 'e4e5']);
    expect(line.startFen.contains(' b '), isTrue);
    expect(line.solverPlies, 1);
  });

  test('bundled mate stays a single solver move', () {
    final line = normalizePuzzleLine(
      fen: '7k/5K2/6Q1/8/8/8/8/8 w - - 0 1',
      solution: const ['g6g8'],
      kind: PuzzleLineKind.playerFirst,
    );
    expect(line!.moves, ['g6g8']);
    expect(line.solverPlies, 1);
    expect(line.startFen.split(' ').first, '7k/5K2/6Q1/8/8/8/8/8');
  });

  test('saved rows without lineKind treat 2+ moves as Lichess', () {
    expect(
      puzzleLineKindFor(stored: null, solutionLength: 2),
      PuzzleLineKind.lichess,
    );
    expect(
      puzzleLineKindFor(stored: 'player', solutionLength: 3),
      PuzzleLineKind.playerFirst,
    );
  });

  test('occupancy diffs name missing and extra squares', () {
    final matrix = List<int>.filled(64, 0);
    matrix[0] = 1;
    final diffs = puzzleOccupancyDiffs('8/8/8/8/8/8/8/K7 w - - 0 1', matrix);
    expect(diffs, isEmpty);

    matrix[0] = 0;
    matrix[1] = 1;
    final wrong = puzzleOccupancyDiffs('8/8/8/8/8/8/8/K7 w - - 0 1', matrix);
    expect(wrong.map((d) => '${d.square}:${d.missing}').toList(),
        ['a1:true', 'b1:false']);
  });

  test('solver progress counts only the player plies', () {
    expect(puzzleSolverMovesDone(plyIndex: 0, solved: false, plyTotal: 3), 0);
    expect(puzzleSolverMovesDone(plyIndex: 1, solved: false, plyTotal: 3), 1);
    expect(puzzleSolverMovesDone(plyIndex: 2, solved: false, plyTotal: 3), 1);
    expect(puzzleSolverMovesDone(plyIndex: 3, solved: true, plyTotal: 3), 2);
  });

  test('status JSON keeps setup, feedback and sync hold', () {
    final status = PuzzleStatusJson.fromJson(const {
      'active': false,
      'setup_active': true,
      'physical_match': false,
      'feedback': 'none',
      'ply_index': 0,
      'ply_total': 2,
      'session': 4,
      'sync_hold': true,
      'last_uci': '',
      'awaiting_opponent': false,
      'solved': false,
    });
    expect(status.setupActive, isTrue);
    expect(status.physicalMatch, isFalse);
    expect(status.syncHold, isTrue);
    expect(status.session, 4);
    expect(status.plyTotal, 2);
  });
}
