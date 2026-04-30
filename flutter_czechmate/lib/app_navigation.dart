import 'package:flutter/material.dart';

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
Future<void> pushBoardDiscoveryRoute(BuildContext context) {
  return Navigator.of(context).push<void>(
    MaterialPageRoute<void>(
      fullscreenDialog: true,
      builder: (_) => const BoardDiscoveryScreen(),
    ),
  );
}
