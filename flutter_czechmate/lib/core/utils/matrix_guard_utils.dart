/// Decode matrix guard bitmask fields from `/api/status` into chess squares.
List<String> matrixGuardMaskToSquares(int? maskLow, int? maskHigh) {
  final low = maskLow ?? 0;
  final high = maskHigh ?? 0;
  final squares = <String>[];
  for (var i = 0; i < 64; i++) {
    final bit = i < 32 ? ((low >> i) & 1) : ((high >> (i - 32)) & 1);
    if (bit == 0) continue;
    final file = String.fromCharCode('a'.codeUnitAt(0) + (i % 8));
    final rank = (i ~/ 8) + 1;
    squares.add('$file$rank');
  }
  return squares;
}

String matrixGuardSquaresLabel({
  required int? liftedLow,
  required int? liftedHigh,
  required int? droppedLow,
  required int? droppedHigh,
}) {
  final all = <String>{
    ...matrixGuardMaskToSquares(liftedLow, liftedHigh),
    ...matrixGuardMaskToSquares(droppedLow, droppedHigh),
  }.toList()
    ..sort();
  return all.isEmpty ? '—' : all.join(', ');
}
