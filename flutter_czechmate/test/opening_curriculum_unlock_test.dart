import 'package:flutter_test/flutter_test.dart';
import 'package:flutter_czechmate/features/opening/opening_catalog_repository.dart';
import 'package:flutter_czechmate/features/opening/opening_curriculum_unlock.dart';
import 'package:flutter_czechmate/features/opening/opening_progress_repository.dart';

void main() {
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
