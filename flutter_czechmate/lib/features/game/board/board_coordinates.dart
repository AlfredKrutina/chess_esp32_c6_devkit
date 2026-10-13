import 'dart:math' as math;

import 'package:flutter/material.dart';

import '../../../core/constants/board_styles.dart';
import '../../../core/utils/board_algebraic.dart';

/// A full-square tint drawn over the base square color (last move, selection, error…).
typedef BoardSquareMark = ({String square, Color color});

/// Below this square side a readable in-square label would overlap the piece
/// artwork (measured against `assets/pieces/*.png` at `kPieceFrameFraction`),
/// so labels move to an outer rim instead.
const double kInSquareCoordinateMinSquare = 42;

/// Layout of the board inside a square box: the 8×8 grid plus an optional
/// left/bottom rim for coordinates on small boards.
class BoardGeometry {
  const BoardGeometry._({
    required this.side,
    required this.gutter,
    required this.fontSize,
    required this.inset,
    required this.clipRadius,
  });

  /// [clipRadius] is the corner radius the host clips the board with; corner
  /// labels are pushed inside that arc.
  factory BoardGeometry.forSide(double side, {double clipRadius = 0}) {
    final sq = side / 8;
    if (sq >= kInSquareCoordinateMinSquare) {
      return BoardGeometry._(
        side: side,
        gutter: 0,
        fontSize: (sq * 0.22).clamp(8.0, 13.0),
        inset: math.max(sq * 0.05, 2.0),
        clipRadius: clipRadius,
      );
    }
    final fs = (side * 0.05).clamp(8.0, 10.0);
    return BoardGeometry._(
      side: side,
      gutter: (fs + 6).roundToDouble(),
      fontSize: fs,
      inset: 0,
      clipRadius: clipRadius,
    );
  }

  final double side;
  final double gutter;
  final double fontSize;
  final double inset;
  final double clipRadius;

  bool get labelsInGutter => gutter > 0;
  double get boardSide => side - gutter;
  double get square => boardSide / 8;
  Rect get boardRect => Rect.fromLTWH(gutter, 0, boardSide, boardSide);

  /// Top inset of the rank label in the top-left corner square, so its glyph
  /// stays inside the host's rounded clip.
  double get topLeftRankTop => _cornerInset(
        across: inset + _kInkLeft * fontSize,
        inkAlong: _kDigitInkTop * fontSize,
      );

  /// Left inset of the file label in the bottom-left corner square.
  double get bottomLeftFileLeft => _cornerInset(
        across: inset + _kLetterInkBottom * fontSize,
        inkAlong: _kInkLeft * fontSize,
      );

  /// [across]: glyph distance from the edge it hugs; [inkAlong]: blank space
  /// between the text box and the glyph along the shifted axis.
  double _cornerInset({required double across, required double inkAlong}) {
    final r = clipRadius;
    if (r <= across) return inset;
    final d = r - across;
    final needed = r - math.sqrt(r * r - d * d) + 0.5 - inkAlong;
    return math.max(inset, needed);
  }
}

// Approximate glyph offsets inside a `height: 1.0` text box (Roboto / SF):
// side bearing, gap above digits, gap below lowercase letters (descender room).
const double _kInkLeft = 0.05;
const double _kDigitInkTop = 0.09;
const double _kLetterInkBottom = 0.2;

/// Base square color with all [marks] for that square composited in paint order.
Color effectiveSquareColor({
  required String algebraic,
  required BoardStyleColors themeColors,
  List<BoardSquareMark> marks = const [],
}) {
  final file = algebraic.codeUnitAt(0) - 97;
  final rank = int.parse(algebraic.substring(1));
  var c = (file + rank).isOdd ? themeColors.dark : themeColors.light;
  for (final m in marks) {
    if (m.square == algebraic) c = Color.alphaBlend(m.color, c);
  }
  return c;
}

double contrastRatio(Color a, Color b) {
  final la = a.computeLuminance();
  final lb = b.computeLuminance();
  return (math.max(la, lb) + 0.05) / (math.min(la, lb) + 0.05);
}

/// Label color tinted from the square itself; whichever of the dark or light
/// variant contrasts more with [background]. Falls back to pure black/white on
/// mid-tone squares where a tinted ink cannot reach 4.5:1.
Color boardCoordinateInk(Color background) {
  final bg = background.withValues(alpha: 1);
  final darkWins = contrastRatio(bg, const Color(0xFF000000)) >=
      contrastRatio(bg, const Color(0xFFFFFFFF));
  final tinted = darkWins
      ? Color.lerp(bg, const Color(0xFF000000), 0.8)!
      : Color.lerp(bg, const Color(0xFFFFFFFF), 0.9)!;
  if (contrastRatio(bg, tinted) >= 4.5) return tinted;
  return darkWins ? const Color(0xFF000000) : const Color(0xFFFFFFFF);
}

/// File letters along the screen-bottom row and rank numbers along the
/// screen-left column, for either orientation (chess.com / diagram convention).
class BoardCoordinatesLayer extends StatelessWidget {
  const BoardCoordinatesLayer({
    super.key,
    required this.geometry,
    required this.flipped,
    required this.themeColors,
    this.marks = const [],
  });

  final BoardGeometry geometry;
  final bool flipped;
  final BoardStyleColors themeColors;
  final List<BoardSquareMark> marks;

  @override
  Widget build(BuildContext context) {
    final g = geometry;
    return IgnorePointer(
      child: ExcludeSemantics(
        child: SizedBox(
          width: g.side,
          height: g.side,
          child: Stack(
            clipBehavior: Clip.none,
            children:
                g.labelsInGutter ? _rimLabels(context) : _inSquareLabels(),
          ),
        ),
      ),
    );
  }

  List<Widget> _inSquareLabels() {
    final g = geometry;
    final sq = g.square;
    final out = <Widget>[];
    for (var sr = 0; sr < 8; sr++) {
      final alg = algebraicAt(flipped: flipped, screenRow: sr, screenCol: 0);
      out.add(Positioned(
        key: ValueKey('coord-rank-${alg.substring(1)}'),
        left: g.gutter + g.inset,
        top: sr * sq + (sr == 0 ? g.topLeftRankTop : g.inset),
        child: _label(alg.substring(1), _bg(alg)),
      ));
    }
    for (var sc = 0; sc < 8; sc++) {
      final alg = algebraicAt(flipped: flipped, screenRow: 7, screenCol: sc);
      out.add(Positioned(
        key: ValueKey('coord-file-${alg.substring(0, 1)}'),
        left: g.gutter + sc * sq + (sc == 0 ? g.bottomLeftFileLeft : g.inset),
        bottom: g.side - g.boardSide + g.inset,
        child: _label(alg.substring(0, 1), _bg(alg)),
      ));
    }
    return out;
  }

  List<Widget> _rimLabels(BuildContext context) {
    final g = geometry;
    final sq = g.square;
    final color = Theme.of(context).colorScheme.onSurfaceVariant;
    final style = TextStyle(
      fontSize: g.fontSize,
      fontWeight: FontWeight.w600,
      height: 1.0,
      color: color,
    );
    final out = <Widget>[];
    for (var sr = 0; sr < 8; sr++) {
      final rank = algebraicAt(flipped: flipped, screenRow: sr, screenCol: 0)
          .substring(1);
      out.add(Positioned(
        key: ValueKey('coord-rank-$rank'),
        left: 0,
        top: sr * sq,
        width: g.gutter,
        height: sq,
        child: Center(
          child: Text(rank, style: style, textScaler: TextScaler.noScaling),
        ),
      ));
    }
    for (var sc = 0; sc < 8; sc++) {
      final file = algebraicAt(flipped: flipped, screenRow: 7, screenCol: sc)
          .substring(0, 1);
      out.add(Positioned(
        key: ValueKey('coord-file-$file'),
        left: g.gutter + sc * sq,
        top: g.boardSide,
        width: sq,
        height: g.gutter,
        child: Center(
          child: Text(file, style: style, textScaler: TextScaler.noScaling),
        ),
      ));
    }
    return out;
  }

  Color _bg(String alg) => effectiveSquareColor(
        algebraic: alg,
        themeColors: themeColors,
        marks: marks,
      );

  Widget _label(String text, Color bg) {
    final fs = geometry.fontSize;
    return Text(
      text,
      textScaler: TextScaler.noScaling,
      style: TextStyle(
        fontSize: fs,
        fontWeight: FontWeight.w700,
        height: 1.0,
        color: boardCoordinateInk(bg),
        shadows: [
          Shadow(color: bg.withValues(alpha: 0.75), blurRadius: fs * 0.2),
        ],
      ),
    );
  }
}
