import 'package:flutter_test/flutter_test.dart';
import 'package:shared_preferences/shared_preferences.dart';

import 'package:flutter_czechmate/features/opening/opening_progress_repository.dart';

void main() {
  test('opening progress stars learn then drill', () async {
    SharedPreferences.setMockInitialValues({});
    final prefs = await SharedPreferences.getInstance();
    final repo = OpeningProgressRepository(prefs);

    expect(repo.progressFor('italian_giuoco_white').stars, 0);

    await repo.recordCompletion(lineId: 'italian_giuoco_white', mode: 'learn');
    expect(repo.progressFor('italian_giuoco_white').stars, 1);

    await repo.recordCompletion(
      lineId: 'italian_giuoco_white',
      mode: 'drill',
      drillErrors: 1,
    );
    expect(repo.progressFor('italian_giuoco_white').stars, 2);
    expect(repo.progressFor('italian_giuoco_white').bestDrillErrors, 1);
  });
}
