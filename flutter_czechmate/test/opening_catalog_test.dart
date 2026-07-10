import 'package:flutter_test/flutter_test.dart';
import 'package:flutter_czechmate/features/opening/opening_catalog_repository.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  test('opening catalog loads 41 validated lines', () async {
    final repo = OpeningCatalogRepository();
    final lines = await repo.loadAll();
    expect(lines.length, 41);
    expect(lines.any((l) => l.id == 'italian_giuoco_white'), isTrue);
    expect(lines.any((l) => l.id == 'spanish_berlin_white'), isTrue);
    final italian = lines.firstWhere((l) => l.id == 'italian_giuoco_white');
    expect(italian.lineUci.length, 8);
    expect(italian.playerPlyIndices, [0, 2, 4, 6]);
    expect(italian.mirrorLineId, 'sicilian_odb_black');
    expect(italian.toStartPayload()['action'], 'start');
    expect(italian.commonMistakes.length, greaterThanOrEqualTo(1));
  });

  test('curricula reference existing line ids', () async {
    final repo = OpeningCatalogRepository();
    final curricula = await repo.loadCurricula();
    final lines = await repo.loadAll();
    final ids = lines.map((l) => l.id).toSet();
    expect(curricula.length, greaterThanOrEqualTo(4));
    for (final c in curricula) {
      for (final id in c.lineIds) {
        expect(ids.contains(id), isTrue, reason: 'missing $id in ${c.id}');
      }
    }
  });
}
