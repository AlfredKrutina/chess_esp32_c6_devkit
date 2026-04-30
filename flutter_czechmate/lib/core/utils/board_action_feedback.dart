import 'package:flutter/material.dart';

/// SnackBar after BLE/Wi‑Fi actions from UI (timer, LEDs, refresh).
Future<void> runBoardCommandWithSnackBar(
  BuildContext context,
  Future<void> Function() action, {
  required String successMessage,
}) async {
  try {
    await action();
    if (!context.mounted) return;
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(content: Text(successMessage)),
    );
  } catch (e) {
    if (!context.mounted) return;
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(
        content: Text(
          'Command failed. Check Bluetooth/Wi‑Fi connection and board URL in Settings. '
          'Details: $e',
        ),
      ),
    );
  }
}
