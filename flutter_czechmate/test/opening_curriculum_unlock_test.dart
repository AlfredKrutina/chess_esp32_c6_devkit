import 'package:flutter_test/flutter_test.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:czechmate/features/opening/opening_catalog_repository.dart';
import 'package:czechmate/features/opening/opening_curriculum_unlock.dart';
import 'package:czechmate/features/opening/opening_progress_repository.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  test('mirror mode unlocks when source line has two stars', () async {
    SharedPreferences.setMockInitialValues({});
    final prefs = await SharedPreferences.getInstance();
    final repo = OpeningCatalogRepository();
    final progressRepo = OpeningProgressRepository(prefs);
    final italian =
        (await repo.loadAll()).firstWhere((l) => l.id == 'italian_giuoco_white');
    expect(italian.mirrorLineId, 'petrov_black');

    // Without stars — locked
    expect(
      progressRepo.mirrorUnlocked(italian.id, mirrorLineId: italian.mirrorLineId),
      isFalse,
    );

    await progressRepo.recordCompletion(
      lineId: italian.id,
      mode: 'drill',
      drillErrors: 1,
    );
    expect(
      progressRepo.mirrorUnlocked(italian.id, mirrorLineId: italian.mirrorLineId),
      isTrue,
    );
  });

  test('basics_black unlocks after two white basics stars', () {
    final curricula = [
      const OpeningCurriculum(
        id: 'basics_white',
        nameCs: 'W',
        nameEn: 'W',
        lineIds: ['a', 'b', 'c'],
      ),
      const OpeningCurriculum(
        id: 'basics_black',
        nameCs: 'B',
        nameEn: 'B',
        lineIds: ['x'],
      ),
    ];
    final progress = <String, OpeningLineProgress>{
      'a': const OpeningLineProgress(stars: 1),
    };
    expect(
      OpeningCurriculumUnlock.isUnlocked(
        curricula[1],
        allCurricula: curricula,
        progress: progress,
      ),
      isFalse,
    );
    progress['b'] = const OpeningLineProgress(stars: 1);
    expect(
      OpeningCurriculumUnlock.isUnlocked(
        curricula[1],
        allCurricula: curricula,
        progress: progress,
      ),
      isTrue,
    );
  });

  test('lines due for review after 3 days with two stars', () {
    final lines = [
      OpeningLine(
        id: 'test_line',
        eco: 'A00',
        nameCs: 'T',
        nameEn: 'T',
        side: 'white',
        difficulty: 1,
        startFen: 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1',
        lineUci: ['e2e4', 'e7e5'],
        playerPlyIndices: [0],
      ),
    ];
    final old = DateTime.now().toUtc().subtract(const Duration(days: 4));
    final progress = {
      'test_line': OpeningLineProgress(
        stars: 2,
        lastCompletedAt: old.toIso8601String(),
      ),
    };
    final due = OpeningCurriculumUnlock.linesDueForReview(lines, progress);
    expect(due, ['test_line']);
  });
}
