import 'package:flutter/material.dart';
import 'package:czechmate/core/constants/board_styles.dart';
import 'package:czechmate/core/utils/board_algebraic.dart';
import 'package:czechmate/features/game/board/board_coordinates.dart';
import 'package:flutter_test/flutter_test.dart';

const _styles = [
  'wooden',
  'modernDark',
  'iceBlue',
  'forestGreen',
  'marbleGray',
  'midnight',
  'coral',
  'slate',
];

Future<BoardGeometry> _pump(
  WidgetTester tester, {
  required double side,
  required bool flipped,
  double clipRadius = 14,
}) async {
  final g = BoardGeometry.forSide(side, clipRadius: clipRadius);
  await tester.binding.setSurfaceSize(const Size(1000, 1000));
  addTearDown(() => tester.binding.setSurfaceSize(null));
  await tester.pumpWidget(
    MaterialApp(
      home: Align(
        alignment: Alignment.topLeft,
        child: BoardCoordinatesLayer(
          geometry: g,
          flipped: flipped,
          themeColors: BoardStyleColors.fromRaw('wooden'),
        ),
      ),
    ),
  );
  return g;
}

Rect _squareRect(BoardGeometry g, int row, int col) => Rect.fromLTWH(
    g.gutter + col * g.square, row * g.square, g.square, g.square);

Rect _label(WidgetTester tester, String key) =>
    tester.getRect(find.byKey(ValueKey(key)));

void main() {
  group('algebraicAt orientation', () {
    test('white at bottom: a1 bottom-left, h8 top-right', () {
      expect(algebraicAt(flipped: false, screenRow: 7, screenCol: 0), 'a1');
      expect(algebraicAt(flipped: false, screenRow: 7, screenCol: 7), 'h1');
      expect(algebraicAt(flipped: false, screenRow: 0, screenCol: 7), 'h8');
    });

    test('black at bottom: h8 bottom-left, a8 bottom-right, a1 top-right', () {
      expect(algebraicAt(flipped: true, screenRow: 7, screenCol: 0), 'h8');
      expect(algebraicAt(flipped: true, screenRow: 7, screenCol: 7), 'a8');
      expect(algebraicAt(flipped: true, screenRow: 0, screenCol: 7), 'a1');
      expect(algebraicAt(flipped: true, screenRow: 0, screenCol: 0), 'h1');
    });

    test('screenCoordsFromAlgebraic inverts algebraicAt', () {
      for (final flipped in [false, true]) {
        for (var r = 0; r < 8; r++) {
          for (var c = 0; c < 8; c++) {
            final alg =
                algebraicAt(flipped: flipped, screenRow: r, screenCol: c);
            expect(
              screenCoordsFromAlgebraic(flipped: flipped, algebraic: alg),
              (r, c),
            );
          }
        }
      }
    });
  });

  group('in-square coordinates', () {
    testWidgets('white: files a–h on bottom row, ranks 8–1 on left column',
        (tester) async {
      final g = await _pump(tester, side: 400, flipped: false);
      expect(g.labelsInGutter, isFalse);
      for (var i = 0; i < 8; i++) {
        final file = kFiles[i];
        expect(
            _squareRect(g, 7, i)
                .contains(_label(tester, 'coord-file-$file').center),
            isTrue,
            reason: '$file must sit on ${file}1');
        final rank = 8 - i;
        expect(
            _squareRect(g, i, 0)
                .contains(_label(tester, 'coord-rank-$rank').center),
            isTrue,
            reason: '$rank must sit on a$rank');
      }
    });

    testWidgets('flipped: files h–a on bottom row, ranks 1–8 on left column',
        (tester) async {
      final g = await _pump(tester, side: 400, flipped: true);
      for (var i = 0; i < 8; i++) {
        final file = kFiles[7 - i];
        expect(
            _squareRect(g, 7, i)
                .contains(_label(tester, 'coord-file-$file').center),
            isTrue,
            reason: '$file must sit on ${file}8 (bottom edge for black)');
        final rank = i + 1;
        expect(
            _squareRect(g, i, 0)
                .contains(_label(tester, 'coord-rank-$rank').center),
            isTrue,
            reason: '$rank must sit on h$rank (left edge for black)');
      }
    });

    // The test font (Ahem) draws every glyph as a full 1 em square, so these
    // bounds are looser than real Roboto/SF glyphs.
    testWidgets('labels stay in their corner, clear of the piece body',
        (tester) async {
      for (final side in [336.0, 360.0, 400.0, 480.0, 640.0]) {
        final g = await _pump(tester, side: side, flipped: false);
        expect(g.labelsInGutter, isFalse);
        for (var i = 0; i < 8; i++) {
          final rank = _label(tester, 'coord-rank-${8 - i}');
          final sq = _squareRect(g, i, 0);
          expect(rank.left - sq.left, lessThan(g.square * 0.2));
          expect(rank.top - sq.top, lessThan(g.square * 0.2));
          expect(rank.height, lessThanOrEqualTo(g.fontSize.ceil() + 0.01));

          final file = _label(tester, 'coord-file-${kFiles[i]}');
          final fsq = _squareRect(g, 7, i);
          expect(file.left - fsq.left, lessThan(g.square * 0.2));
          expect(fsq.bottom - file.bottom, lessThan(g.square * 0.2));
          expect(file.bottom, lessThanOrEqualTo(fsq.bottom));
        }
      }
    });

    testWidgets('corner glyphs stay inside the rounded clip', (tester) async {
      for (final r in [10.0, 14.0]) {
        for (final side in [336.0, 360.0, 480.0]) {
          final g =
              await _pump(tester, side: side, flipped: false, clipRadius: r);
          final fs = g.fontSize;
          bool insideArc(Offset p, Offset center) =>
              (p - center).distance <= r + 0.01;
          final eight = _label(tester, 'coord-rank-8');
          final eightInk = eight.topLeft + Offset(0.05 * fs, 0.09 * fs);
          expect(insideArc(eightInk, Offset(r, r)), isTrue,
              reason: 'r=$r side=$side rank 8 at $eightInk');
          final a = _label(tester, 'coord-file-a');
          final aInk = a.bottomLeft + Offset(0.05 * fs, -0.2 * fs);
          expect(insideArc(aInk, Offset(r, g.side - r)), isTrue,
              reason: 'r=$r side=$side file a at $aInk');
          expect(eight.top, greaterThanOrEqualTo(g.inset));
          expect(a.left, greaterThanOrEqualTo(g.inset));
        }
      }
    });
  });

  testWidgets('small boards put labels in a rim outside the squares',
      (tester) async {
    for (final flipped in [false, true]) {
      final g = await _pump(tester, side: 200, flipped: flipped);
      expect(g.labelsInGutter, isTrue);
      for (var i = 0; i < 8; i++) {
        final file = flipped ? kFiles[7 - i] : kFiles[i];
        final f = _label(tester, 'coord-file-$file');
        expect(f.top, greaterThanOrEqualTo(g.boardSide - 0.01));
        final col = _squareRect(g, 7, i);
        expect(f.center.dx, inInclusiveRange(col.left, col.right));

        final rank = flipped ? i + 1 : 8 - i;
        final rk = _label(tester, 'coord-rank-$rank');
        expect(rk.right, lessThanOrEqualTo(g.gutter + 0.01));
        final row = _squareRect(g, i, 0);
        expect(rk.center.dy, inInclusiveRange(row.top, row.bottom));
      }
    }
  });

  test('label ink has WCAG AA contrast on every style and highlight', () {
    for (final raw in _styles) {
      final t = BoardStyleColors.fromRaw(raw);
      final marks = <Color?>[null, t.lastMove, t.selected, t.error];
      for (final square in ['a1', 'a2']) {
        for (final m in marks) {
          final bg = effectiveSquareColor(
            algebraic: square,
            themeColors: t,
            marks: [if (m != null) (square: square, color: m)],
          );
          final ratio = contrastRatio(bg, boardCoordinateInk(bg));
          expect(ratio, greaterThanOrEqualTo(4.5),
              reason: '$raw $square mark=$m ratio=${ratio.toStringAsFixed(2)}');
        }
      }
    }
  });

  test('corner insets are a no-op without clipping', () {
    final g = BoardGeometry.forSide(400);
    expect(g.topLeftRankTop, g.inset);
    expect(g.bottomLeftFileLeft, g.inset);
  });
}
