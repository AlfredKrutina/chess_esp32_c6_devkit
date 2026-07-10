import 'package:flutter_test/flutter_test.dart';
import 'package:shared_preferences/shared_preferences.dart';

import 'package:czechmate/features/opening/opening_catalog_repository.dart';
import 'package:czechmate/features/opening/opening_progress_repository.dart';

/// Automated checks for §21 release gate (CI complement to MANUAL_TEST_CHECKLIST.md).
void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  group('§21 release gate — automated', () {
    test('G1: catalog has 41 lines with rationale', () async {
      final repo = OpeningCatalogRepository();
      final lines = await repo.loadAll();
      expect(lines.length, 41);
      final withRationale = lines.where((l) => l.rationale != null).length;
      expect(withRationale, 41);
    });

    test('P4: at least 10 lines define common_mistakes', () async {
      final repo = OpeningCatalogRepository();
      final lines = await repo.loadAll();
      final withMistakes =
          lines.where((l) => l.commonMistakes.isNotEmpty).length;
      expect(withMistakes, greaterThanOrEqualTo(10));
    });

    test('P6: at least 10 symmetric mirror pairs', () async {
      final repo = OpeningCatalogRepository();
      final lines = await repo.loadAll();
      final byId = {for (final l in lines) l.id: l};
      final seen = <String>{};
      var pairs = 0;

      for (final line in lines) {
        final mirrorId = line.mirrorLineId;
        if (mirrorId == null || mirrorId.isEmpty) continue;
        final key = [line.id, mirrorId]..sort();
        final pairKey = key.join('|');
        if (seen.contains(pairKey)) continue;
        seen.add(pairKey);

        final partner = byId[mirrorId];
        expect(partner, isNotNull);
        expect(line.side, isNot(equals(partner!.side)));
        expect(partner.mirrorLineId, line.id);
        pairs++;
      }
      expect(pairs, greaterThanOrEqualTo(10));
    });

    test('G7: progress survives simulated app restart', () async {
      SharedPreferences.setMockInitialValues({});
      final prefs1 = await SharedPreferences.getInstance();
      final repo1 = OpeningProgressRepository(prefs1);

      await repo1.recordCompletion(
        lineId: 'italian_giuoco_white',
        mode: 'learn',
      );
      await repo1.recordCompletion(
        lineId: 'italian_giuoco_white',
        mode: 'drill',
        drillErrors: 0,
      );

      // Simulate cold start — new repository instance, same persisted store.
      final prefs2 = await SharedPreferences.getInstance();
      final repo2 = OpeningProgressRepository(prefs2);
      final progress = repo2.progressFor('italian_giuoco_white');

      expect(progress.stars, 2);
      expect(progress.bestDrillErrors, 0);
      expect(progress.lastCompletedAt, isNotNull);
      expect(prefs2.getString(OpeningProgressRepository.storageKey), isNotNull);
    });

    test('G4 stars: timed and mirror progression', () async {
      SharedPreferences.setMockInitialValues({});
      final prefs = await SharedPreferences.getInstance();
      final repo = OpeningProgressRepository(prefs);

      await repo.recordCompletion(
        lineId: 'london_system_white',
        mode: 'timed',
        timedSuccess: true,
      );
      expect(repo.progressFor('london_system_white').stars, 3);

      await repo.recordCompletion(
        lineId: 'slav_defence_black',
        mode: 'mirror',
        drillErrors: 1,
      );
      expect(repo.progressFor('slav_defence_black').stars, 4);
    });
  });
}
