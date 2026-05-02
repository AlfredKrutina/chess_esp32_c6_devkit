import 'package:flutter/material.dart';

/// Named presets for eval / timing charts (report, analysis, PNG export).
enum ChartPalettePreset {
  theme,
  neon,
  sunset,
  ocean,
  custom;

  static ChartPalettePreset fromStorage(String? raw) {
    if (raw == null || raw.isEmpty) return neon;
    for (final v in ChartPalettePreset.values) {
      if (v.name == raw) return v;
    }
    return neon;
  }
}

@immutable
class ChartPaletteColors {
  const ChartPaletteColors({
    required this.evalLine,
    required this.cumulative,
    required this.barWhite,
    required this.barBlack,
  });

  final Color evalLine;
  final Color cumulative;
  final Color barWhite;
  final Color barBlack;

  /// Default vibrant coordinated palette.
  static const neon = ChartPaletteColors(
    evalLine: Color(0xFFFF2D95),
    cumulative: Color(0xFF00E5FF),
    barWhite: Color(0xFFFFEA00),
    barBlack: Color(0xFFB388FF),
  );

  static const sunset = ChartPaletteColors(
    evalLine: Color(0xFFFF6D00),
    cumulative: Color(0xFFFF4081),
    barWhite: Color(0xFFFFD740),
    barBlack: Color(0xFF7C4DFF),
  );

  static const ocean = ChartPaletteColors(
    evalLine: Color(0xFF00E676),
    cumulative: Color(0xFF2979FF),
    barWhite: Color(0xFF69F0AE),
    barBlack: Color(0xFF448AFF),
  );

  ChartPaletteColors copyWith({
    Color? evalLine,
    Color? cumulative,
    Color? barWhite,
    Color? barBlack,
  }) {
    return ChartPaletteColors(
      evalLine: evalLine ?? this.evalLine,
      cumulative: cumulative ?? this.cumulative,
      barWhite: barWhite ?? this.barWhite,
      barBlack: barBlack ?? this.barBlack,
    );
  }

  Map<String, int> toJson() => {
        'eval': _colorToArgb(evalLine),
        'cum': _colorToArgb(cumulative),
        'white': _colorToArgb(barWhite),
        'black': _colorToArgb(barBlack),
      };

  factory ChartPaletteColors.fromJson(Map<String, dynamic> m) {
    int argb(String k) {
      final v = m[k];
      if (v is int) return v;
      if (v is num) return v.toInt();
      return 0;
    }

    return ChartPaletteColors(
      evalLine: Color(argb('eval')),
      cumulative: Color(argb('cum')),
      barWhite: Color(argb('white')),
      barBlack: Color(argb('black')),
    );
  }

  static ChartPaletteColors forPreset(ChartPalettePreset preset, ColorScheme cs) {
    switch (preset) {
      case ChartPalettePreset.theme:
        final tertiary = cs.tertiary;
        final secondary = cs.secondary;
        return ChartPaletteColors(
          evalLine: cs.primary,
          cumulative: tertiary != cs.primary ? tertiary : secondary,
          barWhite: cs.primary,
          barBlack: secondary != cs.primary ? secondary : tertiary,
        );
      case ChartPalettePreset.neon:
        return neon;
      case ChartPalettePreset.sunset:
        return sunset;
      case ChartPalettePreset.ocean:
        return ocean;
      case ChartPalettePreset.custom:
        return neon;
    }
  }
}

int _colorToArgb(Color c) {
  final a = (c.a * 255.0).round().clamp(0, 255);
  final r = (c.r * 255.0).round().clamp(0, 255);
  final g = (c.g * 255.0).round().clamp(0, 255);
  final b = (c.b * 255.0).round().clamp(0, 255);
  return (a << 24) | (r << 16) | (g << 8) | b;
}
