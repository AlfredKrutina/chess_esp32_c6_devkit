import 'opening_catalog_repository.dart';
import 'opening_progress_repository.dart';

/// Curriculum unlock rules — plan §8.3.
class OpeningCurriculumUnlock {
  const OpeningCurriculumUnlock._();

  static bool isUnlocked(
    OpeningCurriculum curriculum, {
    required List<OpeningCurriculum> allCurricula,
    required Map<String, OpeningLineProgress> progress,
  }) {
    switch (curriculum.id) {
      case 'basics_white':
        return true;
      case 'basics_black':
        return _countLinesWithMinStars(
              curriculumId: 'basics_white',
              curricula: allCurricula,
              progress: progress,
              minStars: 1,
            ) >=
            2;
      case 'classical_deep':
        return _countLinesWithMinStars(
              curriculumId: 'basics_white',
              curricula: allCurricula,
              progress: progress,
              minStars: 2,
            ) >=
            1 ||
            _countLinesWithMinStars(
              curriculumId: 'basics_black',
              curricula: allCurricula,
              progress: progress,
              minStars: 2,
            ) >=
            1;
      case 'systems':
        return _totalLinesWithMinStars(progress, minStars: 2) >= 5;
      default:
        return true;
    }
  }

  static String lockReasonCs(
    OpeningCurriculum curriculum, {
    required List<OpeningCurriculum> allCurricula,
    required Map<String, OpeningLineProgress> progress,
  }) {
    switch (curriculum.id) {
      case 'basics_black':
        final n = _countLinesWithMinStars(
          curriculumId: 'basics_white',
          curricula: allCurricula,
          progress: progress,
          minStars: 1,
        );
        return 'Odemčeno po ★ na 2 liniích z Bílých základů ($n/2).';
      case 'classical_deep':
        return 'Odemčeno po ★★ alespoň na jedné linii z Bílých nebo Černých základů.';
      case 'systems':
        final n = _totalLinesWithMinStars(progress, minStars: 2);
        return 'Odemčeno po ★★ na 5 liniích celkem ($n/5).';
      default:
        return '';
    }
  }

  /// Lines with ★★+ not practiced in [reviewAfterDays] — plan §4.3 (v1).
  static List<String> linesDueForReview(
    List<OpeningLine> lines,
    Map<String, OpeningLineProgress> progress, {
    int reviewAfterDays = 3,
  }) {
    final now = DateTime.now().toUtc();
    final due = <String>[];
    for (final line in lines) {
      final p = progress[line.id];
      if (p == null || p.stars < 2 || p.lastCompletedAt == null) continue;
      final last = DateTime.tryParse(p.lastCompletedAt!);
      if (last == null) continue;
      if (now.difference(last).inDays >= reviewAfterDays) {
        due.add(line.id);
      }
    }
    return due;
  }

  static int _countLinesWithMinStars({
    required String curriculumId,
    required List<OpeningCurriculum> curricula,
    required Map<String, OpeningLineProgress> progress,
    required int minStars,
  }) {
    final curriculum = curricula.where((c) => c.id == curriculumId).firstOrNull;
    if (curriculum == null) return 0;
    var count = 0;
    for (final id in curriculum.lineIds) {
      if ((progress[id]?.stars ?? 0) >= minStars) count++;
    }
    return count;
  }

  static int _totalLinesWithMinStars(
    Map<String, OpeningLineProgress> progress, {
    required int minStars,
  }) {
    return progress.values.where((p) => p.stars >= minStars).length;
  }
}

extension _FirstOrNull<E> on Iterable<E> {
  E? get firstOrNull {
    final it = iterator;
    if (!it.moveNext()) return null;
    return it.current;
  }
}
