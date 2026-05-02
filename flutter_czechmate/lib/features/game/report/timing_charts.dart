import 'package:flutter/material.dart';

import '../../../core/utils/game_end_report_timing.dart';

/// Kumulativní „uběhlý čas“ v minutách po půltazích — Area + čára (Swift `AreaMark` + `LineMark`).
class CumulativePlayedTimeChart extends StatelessWidget {
  const CumulativePlayedTimeChart({
    super.key,
    required this.points,
    required this.accent,
    this.height = 200,
    this.backgroundColor,
    this.axisColor,
  });

  final List<GameEndCumulativePoint> points;
  final Color accent;
  final double height;
  final Color? backgroundColor;
  final Color? axisColor;

  @override
  Widget build(BuildContext context) {
    if (points.isEmpty) return SizedBox(height: height);
    return SizedBox(
      height: height,
      child: CustomPaint(
        painter: _CumulativePainter(
          points: points,
          accent: accent,
          backgroundColor: backgroundColor,
          axisColor: axisColor ?? Colors.grey.withValues(alpha: 0.35),
        ),
        child: const SizedBox.expand(),
      ),
    );
  }
}

class _CumulativePainter extends CustomPainter {
  _CumulativePainter({
    required this.points,
    required this.accent,
    required this.backgroundColor,
    required this.axisColor,
  });

  final List<GameEndCumulativePoint> points;
  final Color accent;
  final Color? backgroundColor;
  final Color axisColor;

  @override
  void paint(Canvas canvas, Size size) {
    if (backgroundColor != null) {
      canvas.drawRRect(
        RRect.fromRectAndRadius(
          Rect.fromLTWH(0, 0, size.width, size.height),
          const Radius.circular(10),
        ),
        Paint()..color = backgroundColor!,
      );
    }

    const padL = 40.0;
    const padR = 8.0;
    const padT = 12.0;
    const padB = 28.0;
    final w = size.width - padL - padR;
    final h = size.height - padT - padB;
    final sorted = List<GameEndCumulativePoint>.from(points)
      ..sort((a, b) => a.plyIndex.compareTo(b.plyIndex));
    if (sorted.isEmpty) return;
    final pts = sorted;
    final xs = pts.map((p) => p.plyIndex.toDouble()).toList();
    final ys = pts.map((p) => p.totalSeconds / 60.0).toList();
    final xMax = xs.reduce((a, b) => a > b ? a : b).clamp(1, 9999);
    var yMax = ys.reduce((a, b) => a > b ? a : b);
    if (yMax < 0.5) yMax = 0.5;

    Offset proj(int i) {
      final x = padL + w * (pts[i].plyIndex / xMax);
      final y = padT + h * (1 - (ys[i] / yMax));
      return Offset(x, y);
    }

    final fill = Path()..moveTo(proj(0).dx, padT + h);
    for (var i = 0; i < pts.length; i++) {
      final p = proj(i);
      fill.lineTo(p.dx, p.dy);
    }
    fill.lineTo(proj(pts.length - 1).dx, padT + h);
    fill.close();
    canvas.drawPath(
      fill,
      Paint()..color = accent.withValues(alpha: 0.18),
    );

    final line = Path()..moveTo(proj(0).dx, proj(0).dy);
    for (var i = 1; i < pts.length; i++) {
      line.lineTo(proj(i).dx, proj(i).dy);
    }
    canvas.drawPath(
      line,
      Paint()
        ..color = accent
        ..style = PaintingStyle.stroke
        ..strokeWidth = 2.5
        ..strokeCap = StrokeCap.round,
    );

    canvas.drawLine(
      Offset(padL, padT + h),
      Offset(padL + w, padT + h),
      Paint()
        ..color = axisColor
        ..strokeWidth = 1,
    );
  }

  @override
  bool shouldRepaint(covariant _CumulativePainter oldDelegate) =>
      oldDelegate.points != points ||
      oldDelegate.accent != accent ||
      oldDelegate.backgroundColor != backgroundColor ||
      oldDelegate.axisColor != axisColor;
}

/// Sloupce času na tah (s) — horizontální scroll jako na iOS.
class TimePerMoveBarChart extends StatelessWidget {
  const TimePerMoveBarChart({
    super.key,
    required this.points,
    required this.whiteColor,
    required this.blackColor,
    this.outerHeight = 220,
    this.plotHeight = 200,
    this.backgroundColor,
  });

  final List<GameEndThinkPlyPoint> points;
  final Color whiteColor;
  final Color blackColor;
  final double outerHeight;
  final double plotHeight;
  final Color? backgroundColor;

  @override
  Widget build(BuildContext context) {
    final bars = points.where((p) => p.secondsFromPrevious != null).toList();
    if (bars.isEmpty) return SizedBox(height: outerHeight);
    final maxS = bars.map((p) => p.secondsFromPrevious!).reduce((a, b) => a > b ? a : b).clamp(0.5, 9999.0);
    const barW = 12.0;
    const gap = 2.0;
    final chartW = bars.length * (barW + gap) + 40;
    return SizedBox(
      height: outerHeight,
      child: SingleChildScrollView(
        scrollDirection: Axis.horizontal,
        child: SizedBox(
          width: chartW.clamp(320, 2000),
          height: plotHeight,
          child: CustomPaint(
            painter: _BarPainter(
              points: bars,
              maxSeconds: maxS,
              barW: barW,
              gap: gap,
              whiteColor: whiteColor,
              blackColor: blackColor,
              backgroundColor: backgroundColor,
            ),
            child: const SizedBox.expand(),
          ),
        ),
      ),
    );
  }
}

class _BarPainter extends CustomPainter {
  _BarPainter({
    required this.points,
    required this.maxSeconds,
    required this.barW,
    required this.gap,
    required this.whiteColor,
    required this.blackColor,
    required this.backgroundColor,
  });

  final List<GameEndThinkPlyPoint> points;
  final double maxSeconds;
  final double barW;
  final double gap;
  final Color whiteColor;
  final Color blackColor;
  final Color? backgroundColor;

  @override
  void paint(Canvas canvas, Size size) {
    if (backgroundColor != null) {
      canvas.drawRRect(
        RRect.fromRectAndRadius(
          Rect.fromLTWH(0, 0, size.width, size.height),
          const Radius.circular(10),
        ),
        Paint()..color = backgroundColor!,
      );
    }

    const padL = 32.0;
    const padB = 24.0;
    final h = size.height - padB;
    for (var i = 0; i < points.length; i++) {
      final p = points[i];
      final sec = p.secondsFromPrevious ?? 0;
      final bh = h * (sec / maxSeconds);
      final x = padL + i * (barW + gap);
      final y = h - bh;
      final paint = Paint()
        ..color = p.isWhite ? whiteColor.withValues(alpha: 0.92) : blackColor.withValues(alpha: 0.88);
      canvas.drawRRect(
        RRect.fromRectAndRadius(Rect.fromLTWH(x, y, barW, bh), const Radius.circular(2)),
        paint,
      );
    }
  }

  @override
  bool shouldRepaint(covariant _BarPainter oldDelegate) =>
      oldDelegate.points != points ||
      oldDelegate.maxSeconds != maxSeconds ||
      oldDelegate.whiteColor != whiteColor ||
      oldDelegate.blackColor != blackColor ||
      oldDelegate.backgroundColor != backgroundColor;
}
