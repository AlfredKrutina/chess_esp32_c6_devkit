const kFiles = 'abcdefgh';

/// `screenRow` 0 = horní okraj widgetu, `screenCol` 0 = vlevo.
String algebraicAt({
  required bool flipped,
  required int screenRow,
  required int screenCol,
}) {
  if (screenRow < 0 || screenRow > 7 || screenCol < 0 || screenCol > 7) {
    return '';
  }
  if (!flipped) {
    final rank = 8 - screenRow;
    return '${kFiles[screenCol]}$rank';
  }
  final rank = screenRow + 1;
  return '${kFiles[7 - screenCol]}$rank';
}

(int screenRow, int screenCol) screenCoordsFromAlgebraic({
  required bool flipped,
  required String algebraic,
}) {
  if (algebraic.length < 2) return (-1, -1);
  final f = algebraic.codeUnitAt(0) - 97;
  final rank = int.tryParse(algebraic.substring(1)) ?? -1;
  if (f < 0 || f > 7 || rank < 1 || rank > 8) return (-1, -1);
  if (!flipped) {
    return (8 - rank, f);
  }
  return (rank - 1, 7 - f);
}
