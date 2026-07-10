import 'package:flutter_test/flutter_test.dart';
import 'package:czechmate/features/opening/opening_catalog_repository.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  test('commentForOpponentPly resolves annotation by ply_index', () async {
    final repo = OpeningCatalogRepository();
    final italian =
        (await repo.loadAll()).firstWhere((l) => l.id == 'italian_giuoco_white');

    expect(
      italian.commentForOpponentPly(1, 'cs'),
      contains('e5'),
    );
    expect(
      italian.commentForOpponentPly(1, 'en'),
      contains('e5'),
    );
    expect(italian.commentForOpponentPly(0, 'cs'), isNull);
  });

  test('at least 5 lines define opponent_annotations', () async {
    final repo = OpeningCatalogRepository();
    final lines = await repo.loadAll();
    final withAnnotations = lines
        .where((l) => l.opponentCommentsCs.isNotEmpty)
        .length;
    expect(withAnnotations, greaterThanOrEqualTo(5));
  });
}
