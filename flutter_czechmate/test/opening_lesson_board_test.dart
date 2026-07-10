import 'package:flutter_test/flutter_test.dart';
import 'package:czechmate/core/utils/fen_board_parser.dart';

/// Simulates `GameSnapshot.normalizingBoardRowsFromFirmwareExportIfNeeded()`.
List<List<String>> _normalizedFromFirmwareFen(String placement) {
  return boardFromPlacementFen(placement).reversed.toList();
}

void main() {
  group('placementFenFromSnapshotBoard', () {
    test('converts normalized snapshot board to FEN placement', () {
      const fen = 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR';
      final board = _normalizedFromFirmwareFen(fen);
      expect(placementFenFromSnapshotBoard(board), fen);
    });

    test('round-trips a midgame normalized snapshot', () {
      const fen = 'r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R';
      final board = _normalizedFromFirmwareFen(fen);
      expect(placementFenFromSnapshotBoard(board), fen);
    });

    test('returns empty string for invalid dimensions', () {
      expect(placementFenFromSnapshotBoard([]), '');
      expect(
        placementFenFromSnapshotBoard(List.generate(8, (_) => List.filled(7, ''))),
        '',
      );
    });
  });
}
