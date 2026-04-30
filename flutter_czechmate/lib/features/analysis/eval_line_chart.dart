import 'package:flutter/material.dart';

import '../../core/analysis/move_evaluation.dart';

/// Jednoduchý liniový graf evaluace (perspektiva bílého) — náhrada Swift `Chart`.
class EvalLineChart extends StatelessWidget {
  const EvalLineChart({
    super.key,
    required this.points,
    this.height = 180,
  });

  final List<({int moveIndex, double eval, MoveGrade? grade})> points;
  final double height;

  @override
  Widget build(BuildContext context) {
    if (points.isEmpty) return SizedBox(height: height);
    final ys = points.map((p) => p.eval).toList();
    var lo = ys.reduce((a, b) => a < b ? a : b);
    var hi = ys.reduce((a, b) => a > b ? a : b);
    lo = (lo < -0.25 ? lo : -0.25) - 0.4;
    hi = (hi > 0.25 ? hi : 0.25) + 0.4;
    final xMax = points.map((p) => p.moveIndex).reduce((a, b) => a > b ? a : b).clamp(8, 9999).toDouble();

    return SizedBox(
      height: height,
      child: CustomPaint(
        painter: _EvalChartPainter(
          points: points,
          yMin: lo,
          yMax: hi,
          xMax: xMax,
          accent: Theme.of(context).colorScheme.primary,
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
  });

  final List<({int moveIndex, double eval, MoveGrade? grade})> points;
  final double yMin;
  final double yMax;
  final double xMax;
  final Color accent;

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
    final padL = 36.0;
    final padR = 8.0;
    final padT = 8.0;
    final padB = 22.0;
    final w = size.width - padL - padR;
    final h = size.height - padT - padB;

    final span = (yMax - yMin).abs() < 1e-6 ? 1.0 : (yMax - yMin);
    double yAtEval(double ev) => padT + h * (1 - (ev - yMin) / span);
    final y0 = yAtEval(0).clamp(padT, padT + h);
    canvas.drawLine(
      Offset(padL, y0),
      Offset(padL + w, y0),
      Paint()
        ..color = Colors.grey.withValues(alpha: 0.35)
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
      canvas.drawCircle(o, 5, Paint()..color = Colors.white..style = PaintingStyle.stroke..strokeWidth = 1.2);
    }

    final tp = TextPainter(
      text: const TextSpan(text: '0', style: TextStyle(fontSize: 10, color: Colors.grey)),
      textDirection: TextDirection.ltr,
    )..layout();
    tp.paint(canvas, Offset(4, y0 - 6));
  }

  @override
  bool shouldRepaint(covariant _EvalChartPainter oldDelegate) =>
      oldDelegate.points != points || oldDelegate.yMin != yMin || oldDelegate.yMax != yMax;
}
