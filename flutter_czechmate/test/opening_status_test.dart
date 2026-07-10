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
}
