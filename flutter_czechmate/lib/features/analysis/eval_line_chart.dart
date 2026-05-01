import 'package:flutter/material.dart';

import '../../core/analysis/move_evaluation.dart';

/// Jednoduchý liniový graf evaluace (perspektiva bílého) — náhrada Swift `Chart`.
class EvalLineChart extends StatelessWidget {
  const EvalLineChart({
    super.key,
    required this.points,
    this.height = 180,
    this.accentColor,
    this.plotBackgroundColor,
    this.axisColor,
  });

  final List<({int moveIndex, double eval, MoveGrade? grade})> points;
  final double height;
  /// When set (e.g. export), overrides theme primary for the main line.
  final Color? accentColor;
  /// Opaque panel behind the plot (transparent PNG export).
  final Color? plotBackgroundColor;
  /// Horizontal axis at y=0 and label tint.
  final Color? axisColor;

  @override
  Widget build(BuildContext context) {
    if (points.isEmpty) return SizedBox(height: height);
    final ys = points.map((p) => p.eval).toList();
    var lo = ys.reduce((a, b) => a < b ? a : b);
    var hi = ys.reduce((a, b) => a > b ? a : b);
    lo = (lo < -0.25 ? lo : -0.25) - 0.4;
    hi = (hi > 0.25 ? hi : 0.25) + 0.4;
    final xMax = points.map((p) => p.moveIndex).reduce((a, b) => a > b ? a : b).clamp(8, 9999).toDouble();
    final accent = accentColor ?? Theme.of(context).colorScheme.primary;
    final axis = axisColor ?? Colors.grey.withValues(alpha: 0.35);
    final dotRing =
        plotBackgroundColor != null ? Colors.white.withValues(alpha: 0.85) : Colors.white;

    return SizedBox(
      height: height,
      child: CustomPaint(
        painter: _EvalChartPainter(
          points: points,
          yMin: lo,
          yMax: hi,
          xMax: xMax,
          accent: accent,
          plotBackgroundColor: plotBackgroundColor,
          axisColor: axis,
          dotRingColor: dotRing,
        ),
        child: Container(),
      ),
    );
  }
}

class _EvalChartPainter extends CustomPainter {
  _EvalChartPainter({
    required this.points,
    required this.yMin,
    required this.yMax,
    required this.xMax,
    required this.accent,
    required this.plotBackgroundColor,
    required this.axisColor,
    required this.dotRingColor,
  });

  final List<({int moveIndex, double eval, MoveGrade? grade})> points;
  final double yMin;
  final double yMax;
  final double xMax;
  final Color accent;
  final Color? plotBackgroundColor;
  final Color axisColor;
  final Color dotRingColor;

  Color _gradeColor(MoveGrade? g) {
    switch (g) {
      case MoveGrade.best:
      case MoveGrade.good:
        return const Color(0xFF2E7D32);
      case MoveGrade.inaccuracy:
        return const Color(0xFFE65100);
      case MoveGrade.mistake:
        return const Color(0xFFC62828);
      case MoveGrade.blunder:
        return const Color(0xFFB71C1C);
      case MoveGrade.error:
      default:
        return accent;
    }
  }

  @override
  void paint(Canvas canvas, Size size) {
    const padL = 36.0;
    const padR = 8.0;
    const padT = 8.0;
    const padB = 22.0;
    final w = size.width - padL - padR;
    final h = size.height - padT - padB;

    final span = (yMax - yMin).abs() < 1e-6 ? 1.0 : (yMax - yMin);
    double yAtEval(double ev) => padT + h * (1 - (ev - yMin) / span);
    final y0 = yAtEval(0).clamp(padT, padT + h);

    if (plotBackgroundColor != null) {
      final bg = RRect.fromRectAndRadius(
        Rect.fromLTWH(0, 0, size.width, size.height),
        const Radius.circular(10),
      );
      canvas.drawRRect(bg, Paint()..color = plotBackgroundColor!);
    }

    canvas.drawLine(
      Offset(padL, y0),
      Offset(padL + w, y0),
      Paint()
        ..color = axisColor
        ..strokeWidth = 1
        ..strokeCap = StrokeCap.round,
    );

    Offset proj(int xi, double ev) {
      final x = padL + w * (xi / xMax);
      final y = yAtEval(ev);
      return Offset(x, y);
    }

    final path = Path();
    for (var i = 0; i < points.length; i++) {
      final p = proj(points[i].moveIndex, points[i].eval);
      if (i == 0) {
        path.moveTo(p.dx, p.dy);
      } else {
        path.lineTo(p.dx, p.dy);
      }
    }
    canvas.drawPath(
      path,
      Paint()
        ..color = accent.withValues(alpha: 0.85)
        ..style = PaintingStyle.stroke
        ..strokeWidth = 2.5
        ..strokeCap = StrokeCap.round
        ..strokeJoin = StrokeJoin.round,
    );

    for (final p in points) {
      final o = proj(p.moveIndex, p.eval);
      canvas.drawCircle(o, 5, Paint()..color = _gradeColor(p.grade));
      canvas.drawCircle(
        o,
        5,
        Paint()
          ..color = dotRingColor
          ..style = PaintingStyle.stroke
          ..strokeWidth = 1.2,
      );
    }

    final tp = TextPainter(
      text: TextSpan(
        text: '0',
        style: TextStyle(fontSize: 10, color: axisColor),
      ),
      textDirection: TextDirection.ltr,
    )..layout();
    tp.paint(canvas, Offset(4, y0 - 6));
  }

  @override
  bool shouldRepaint(covariant _EvalChartPainter oldDelegate) =>
      oldDelegate.points != points ||
      oldDelegate.yMin != yMin ||
      oldDelegate.yMax != yMax ||
      oldDelegate.accent != accent ||
      oldDelegate.plotBackgroundColor != plotBackgroundColor ||
      oldDelegate.axisColor != axisColor ||
      oldDelegate.dotRingColor != dotRingColor;
}
