import 'package:flutter/material.dart';

/// Scrollbar / tooltip / rail tweaks pro desktopové embeddery (Win / macOS / Linux).
ThemeData mergeDesktopEmbedderTheme(
  ThemeData base, {
  required bool desktopEmbedder,
}) {
  if (!desktopEmbedder) return base;
  final scheme = base.colorScheme;
  final rail = base.navigationRailTheme;
  return base.copyWith(
    navigationRailTheme: rail.copyWith(elevation: 0),
    scrollbarTheme: ScrollbarThemeData(
      thickness: WidgetStateProperty.all(9),
      radius: const Radius.circular(5),
      thumbVisibility: WidgetStateProperty.all(true),
      thumbColor: WidgetStateProperty.resolveWith((states) {
        if (states.contains(WidgetState.dragged)) {
          return scheme.primary.withValues(alpha: 0.55);
        }
        return scheme.primary.withValues(alpha: 0.35);
      }),
      crossAxisMargin: 4,
      mainAxisMargin: 3,
    ),
    tooltipTheme: const TooltipThemeData(
      waitDuration: Duration(milliseconds: 350),
      showDuration: Duration(seconds: 5),
    ),
  );
}
