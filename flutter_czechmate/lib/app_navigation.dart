import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import 'app_providers.dart';
import 'features/connection/board_discovery_screen.dart';

/// Stejné pořadí jako iOS `AppMainTab` (`MainTabView.swift`).
class AppMainTab {
  AppMainTab._();
  static const int game = 0;
  static const int analysis = 1;
  static const int puzzle = 2;
  static const int progress = 3;
  static const int settings = 4;
}

/// Jako `BoardDiscoveryView` v sheetu z karty Hra — není spodní záložka.
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

/// After a successful connection from [BoardDiscoveryScreen], switch to Play and close the dialog.
void closeBoardDiscoveryAndFocusPlay(WidgetRef ref, BuildContext context) {
  ref.read(mainNavTabIndexProvider.notifier).state = AppMainTab.game;
  if (!context.mounted) return;
  final nav = Navigator.of(context);
  if (nav.canPop()) nav.pop();
}
