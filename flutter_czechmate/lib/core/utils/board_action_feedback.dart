import 'package:flutter/material.dart';

import '../widgets/glass_snackbar.dart';

/// SnackBar after BLE/Wi‑Fi actions from UI (timer, LEDs, refresh).
Future<void> runBoardCommandWithSnackBar(
  BuildContext context,
  Future<void> Function() action, {
  required String successMessage,
}) async {
  try {
    await action();
    if (!context.mounted) return;
    showAppSnackBar(context, successMessage);
  } catch (e) {
    if (!context.mounted) return;
    showAppSnackBar(
      context,
      'Could not send command. Check Bluetooth/Wi‑Fi and the board URL in Settings. '
      'Details: $e',
      errorStyle: true,
    );
  }
}
