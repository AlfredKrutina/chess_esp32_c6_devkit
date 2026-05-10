import 'dart:math' as math;

import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';

/// True for Flutter desktop embedders (macOS, Windows, Linux). Web is treated as non-desktop here.
bool isDesktopEmbedder() {
  if (kIsWeb) return false;
  return switch (defaultTargetPlatform) {
    TargetPlatform.macOS ||
    TargetPlatform.windows ||
    TargetPlatform.linux =>
      true,
    _ => false,
  };
}

/// Use side navigation + content cap when desktop window is wide enough.
bool useDesktopNavigationShell(Size windowSize) {
  return isDesktopEmbedder() && windowSize.width >= 720;
}

/// Readable max width for tab content on large canvases (avoids stretched lists / huge chess row).
double desktopContentMaxWidth(double parentWidth) {
  if (parentWidth <= 560) return parentWidth;
  return math.min(parentWidth * 0.94, 1120);
}

/// Horizontal inset so cards don’t touch the rail / window edge on desktop.
EdgeInsets desktopContentPadding(bool desktopShell) {
  if (!desktopShell) return EdgeInsets.zero;
  return const EdgeInsets.symmetric(horizontal: 20);
}

/// Default max width for settings detail routes on desktop (narrow forms).
const double kDesktopSettingsDetailMaxWidth = 560;

/// Wider cap for dense sections (firmware, MQTT).
const double kDesktopSettingsWideDetailMaxWidth = 640;

/// Shared default for non-settings forms using the same constrained column.
const double kDesktopFormDetailMaxWidth = kDesktopSettingsDetailMaxWidth;

/// Max text/chat column width when coach panel is embedded beside the board (desktop).
const double kDesktopCoachPanelMaxWidth = 520;

/// Centers and constrains scrollable settings body on desktop embedders.
Widget desktopSettingsDetailBody(
  Widget child, {
  double maxWidth = kDesktopSettingsDetailMaxWidth,
}) {
  if (!isDesktopEmbedder()) return child;
  return Align(
    alignment: Alignment.topCenter,
    child: ConstrainedBox(
      constraints: BoxConstraints(maxWidth: maxWidth),
      child: child,
    ),
  );
}

/// Alias for routes outside Settings that share the same layout helper.
Widget desktopFormDetailBody(
  Widget child, {
  double maxWidth = kDesktopFormDetailMaxWidth,
}) =>
    desktopSettingsDetailBody(child, maxWidth: maxWidth);
