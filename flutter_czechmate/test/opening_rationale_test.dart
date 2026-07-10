import 'package:flutter_test/flutter_test.dart';
import 'package:czechmate/features/opening/opening_catalog_repository.dart';
import 'package:czechmate/features/opening/opening_rationale.dart';

void main() {
  group('OpeningRationale', () {
    test('parses full rationale block from catalog json', () {
      final rationale = OpeningRationale.fromJson({
        'summary': {'cs': 'Krátké shrnutí.', 'en': 'Short summary.'},
        'why_this_line': {
          'cs': 'Proč učíme tuto linii.',
          'en': 'Why we teach this line.',
        },
        'instead_of': {
          'cs': 'Místo jiné varianty.',
          'en': 'Instead of another line.',
        },
        'when_to_play': {
          'cs': 'Kdy hrát.',
          'en': 'When to play.',
        },
        'related_line_ids': ['italian_two_knights_white'],
      });

      expect(rationale.summaryCs, 'Krátké shrnutí.');
      expect(rationale.whyThisLineCs, 'Proč učíme tuto linii.');
      expect(rationale.insteadOfEn, 'Instead of another line.');
      expect(rationale.whenToPlayForLocale('cs'), 'Kdy hrát.');
      expect(rationale.relatedLineIds, ['italian_two_knights_white']);
    });
  });

  test('catalog lines include rationale after sync', () async {
    final repo = OpeningCatalogRepository();
    final lines = await repo.loadAll();
    final italian = lines.firstWhere((l) => l.id == 'italian_giuoco_white');

    expect(italian.rationale, isNotNull);
    expect(italian.rationale!.hasSummary, isTrue);
    expect(italian.rationale!.whyThisLineCs, isNotEmpty);
    expect(italian.rationale!.insteadOfCs, isNotEmpty);
    expect(italian.subtitleForLocale('cs'), isNotEmpty);

    for (final line in lines) {
      expect(
        line.rationale,
        isNotNull,
        reason: 'missing rationale for ${line.id}',
      );
      expect(line.rationale!.whyThisLineCs, isNotEmpty);
      expect(line.rationale!.insteadOfCs, isNotEmpty);
    }
  });
}
