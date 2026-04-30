import 'dart:math' as math;

import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';

/// True for Flutter desktop embedders (macOS, Windows, Linux). Web is treated as non-desktop here.
bool isDesktopEmbedder() {
  if (kIsWeb) return false;
  return switch (defaultTargetPlatform) {
    TargetPlatform.macOS || TargetPlatform.windows || TargetPlatform.linux => true,
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
