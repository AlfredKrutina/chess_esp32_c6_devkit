import 'package:flutter_test/flutter_test.dart';
import 'package:flutter_czechmate/features/opening/opening_catalog_repository.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  test('opening catalog loads 3 sample lines', () async {
    final repo = OpeningCatalogRepository();
    final lines = await repo.loadAll();
    expect(lines.length, 3);
    expect(lines.any((l) => l.id == 'italian_giuoco_white'), isTrue);
    final italian = lines.firstWhere((l) => l.id == 'italian_giuoco_white');
    expect(italian.lineUci.length, 8);
    expect(italian.playerPlyIndices, [0, 2, 4, 6]);
    expect(italian.toStartPayload()['action'], 'start');
  });
}
