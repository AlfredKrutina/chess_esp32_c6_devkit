import 'package:flutter_test/flutter_test.dart';
import 'package:flutter_czechmate/features/opening/opening_common_mistake.dart';

void main() {
  test('openingCommonMistakeHint matches ply and wrong uci', () {
    const mistakes = [
      OpeningCommonMistake(
        wrongUci: 'f1b5',
        atPlyIndex: 4,
        hintCs: 'Nejdřív c4.',
        hintEn: 'Play c4 first.',
      ),
    ];
    expect(
      openingCommonMistakeHint(
        mistakes: mistakes,
        plyIndex: 4,
        lastWrongUci: 'f1b5',
        locale: 'cs',
      ),
      'Nejdřív c4.',
    );
    expect(
      openingCommonMistakeHint(
        mistakes: mistakes,
        plyIndex: 4,
        lastWrongUci: 'f1c4',
        locale: 'cs',
      ),
      isNull,
    );
  });
}
