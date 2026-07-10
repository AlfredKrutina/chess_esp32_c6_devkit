import 'package:flutter_test/flutter_test.dart';
import 'package:czechmate/features/opening/opening_catalog_repository.dart';
import 'package:czechmate/features/opening/opening_feedback_l10n.dart';

void main() {
  group('OpeningFeedbackL10n', () {
    test('maps wrong feedback to human text in cs', () {
      final text = OpeningFeedbackL10n.moveLabel(
        locale: 'cs',
        playerPlyIndex: 1,
        playerPlyTotal: 4,
        from: 'g1',
        to: 'f3',
        feedback: 'wrong',
        drillLike: false,
        isSetupPhase: false,
        isCheckpoint: false,
        physicalSynced: false,
        mismatchCount: 0,
        physicalMatch: true,
        opponentMode: 'physical',
      );
      expect(text, contains('plánovaný tah'));
      expect(text, isNot(contains('wrong')));
    });

    test('maps none to play instruction in en', () {
      final text = OpeningFeedbackL10n.moveLabel(
        locale: 'en',
        playerPlyIndex: 0,
        playerPlyTotal: 4,
        from: 'e2',
        to: 'e4',
        feedback: 'none',
        drillLike: false,
        isSetupPhase: false,
        isCheckpoint: false,
        physicalSynced: false,
        mismatchCount: 0,
        physicalMatch: true,
        opponentMode: 'physical',
      );
      expect(text, 'Play on the board: e2 → e4');
    });
  });

  group('OpeningLine locale', () {
    test('commentForPlayerPly prefers en in english locale', () async {
      final repo = OpeningCatalogRepository();
      final italian =
          (await repo.loadAll()).firstWhere((l) => l.id == 'italian_giuoco_white');
      final en = italian.commentForPlayerPly(0, 'en');
      final cs = italian.commentForPlayerPly(0, 'cs');
      expect(en, isNotNull);
      expect(cs, isNotNull);
      expect(en, isNot(equals(cs)));
    });

    test('ideaForLocale returns english idea when available', () async {
      final repo = OpeningCatalogRepository();
      final italian =
          (await repo.loadAll()).firstWhere((l) => l.id == 'italian_giuoco_white');
      final en = italian.ideaForLocale('en');
      expect(en, isNotNull);
      expect(en!.isNotEmpty, isTrue);
    });
  });
}
