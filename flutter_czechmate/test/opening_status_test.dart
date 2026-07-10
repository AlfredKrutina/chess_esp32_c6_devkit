import 'package:czechmate/core/models/game_status_models.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  test('OpeningTrainingStatusJson decodes setup and mistake fields', () {
    final status = OpeningTrainingStatusJson.fromJson({
      'active': false,
      'setup_phase': true,
      'feedback': 'none',
      'physical_match': false,
      'wrong_move_count': 2,
      'player_ply_index': 0,
      'player_ply_total': 4,
    });

    expect(status.active, false);
    expect(status.setupPhase, true);
    expect(status.physicalMatch, false);
    expect(status.wrongMoveCount, 2);
  });

  test('OpeningTrainingStatusJson decodes mistake_hint feedback', () {
    final status = OpeningTrainingStatusJson.fromJson({
      'active': true,
      'setup_phase': false,
      'feedback': 'mistake_hint',
      'expected_from': 'e2',
      'expected_to': 'e4',
      'wrong_move_count': 3,
    });

    expect(status.feedback, 'mistake_hint');
    expect(status.expectedFrom, 'e2');
    expect(status.expectedTo, 'e4');
    expect(status.wrongMoveCount, 3);
  });

  test('OpeningTrainingStatusJson decodes opponent physical turn', () {
    final status = OpeningTrainingStatusJson.fromJson({
      'active': true,
      'opponent_mode': 'physical',
      'awaiting_opponent_physical': true,
      'feedback': 'opponent_turn',
      'expected_from': 'e7',
      'expected_to': 'e5',
    });

    expect(status.opponentMode, 'physical');
    expect(status.awaitingOpponentPhysical, true);
    expect(status.feedback, 'opponent_turn');
    expect(status.expectedFrom, 'e7');
    expect(status.expectedTo, 'e5');
  });

  test('OpeningTrainingStatusJson decodes last_wrong_uci and ply_index', () {
    final status = OpeningTrainingStatusJson.fromJson({
      'active': true,
      'feedback': 'wrong',
      'ply_index': 4,
      'last_wrong_uci': 'f1b5',
      'wrong_move_count': 1,
    });
    expect(status.plyIndex, 4);
    expect(status.lastWrongUci, 'f1b5');
    expect(status.feedback, 'wrong');
  });
}
