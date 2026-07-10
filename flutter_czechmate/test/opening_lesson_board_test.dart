import 'package:flutter_test/flutter_test.dart';
import 'package:flutter_czechmate/core/utils/fen_board_parser.dart';

void main() {
  group('placementFenFromSnapshotBoard', () {
    test('converts firmware board rows to FEN placement', () {
      final board = boardFromPlacementFen(
        'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR',
      );
      final placement = placementFenFromSnapshotBoard(board);
      expect(placement, 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR');
    });

    test('round-trips a midgame snapshot', () {
      const fen = 'r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R';
      final board = boardFromPlacementFen(fen);
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
