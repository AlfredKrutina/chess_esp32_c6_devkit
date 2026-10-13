import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import 'app_providers.dart';
import 'features/analysis/analysis_screen.dart';
import 'features/connection/board_discovery_screen.dart';

/// Bottom navigation order. Analysis is a second layer of Play, not a tab.
class AppMainTab {
  AppMainTab._();
  static const int game = 0;
  static const int train = 1;
  static const int settings = 2;
  static const int count = 3;
}

/// Like `BoardDiscoveryView` in the sheet from the Game tab — not the bottom tab.
Future<void> pushBoardDiscoveryRoute(
  BuildContext context, {
  bool autoStartBleScan = true,
}) {
  return Navigator.of(context).push<void>(
    MaterialPageRoute<void>(
      fullscreenDialog: true,
      builder: (_) => BoardDiscoveryScreen(autoStartBleScan: autoStartBleScan),
    ),
  );
}

Future<void> pushAnalysisRoute(BuildContext context) {
  return Navigator.of(context).push<void>(
    MaterialPageRoute<void>(builder: (_) => const AnalysisScreen()),
  );
}

/// Switches to Play and closes any pushed layers (analysis, settings pages).
void focusPlayTab(WidgetRef ref, BuildContext context) {
  ref.read(mainNavTabIndexProvider.notifier).state = AppMainTab.game;
  if (!context.mounted) return;
  Navigator.of(context).popUntil((route) => route.isFirst);
}

/// After a successful connection from [BoardDiscoveryScreen], switch to Play and close the dialog.
void closeBoardDiscoveryAndFocusPlay(WidgetRef ref, BuildContext context) {
  ref.read(mainNavTabIndexProvider.notifier).state = AppMainTab.game;
  if (!context.mounted) return;
  final nav = Navigator.of(context);
  if (nav.canPop()) nav.pop();
}
