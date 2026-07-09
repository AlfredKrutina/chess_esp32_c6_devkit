import 'package:flutter_test/flutter_test.dart';
import 'package:flutter_czechmate/core/utils/matrix_guard_utils.dart';

void main() {
  test('matrixGuardMaskToSquares decodes low and high masks', () {
    // e2 = index 12 -> bit 12 in low mask
    expect(matrixGuardMaskToSquares(1 << 12, 0), ['e2']);
    // h8 = index 63 -> bit 31 in high mask (63-32=31)
    expect(matrixGuardMaskToSquares(0, 1 << 31), ['h8']);
  });

  test('matrixGuardSquaresLabel merges lifted and dropped', () {
    expect(
      matrixGuardSquaresLabel(
        liftedLow: 1 << 12,
        liftedHigh: 0,
        droppedLow: 0,
        droppedHigh: 1 << 12,
      ),
      'e2, e6',
    );
  });
}
