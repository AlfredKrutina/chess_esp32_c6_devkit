import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app_providers.dart';
import '../../l10n/app_localizations.dart';
import 'firmware_ota_runner.dart';
import 'firmware_update_availability.dart';

String _isoCalendarDay(DateTime d) =>
    '${d.year.toString().padLeft(4, '0')}-${d.month.toString().padLeft(2, '0')}-${d.day.toString().padLeft(2, '0')}';

/// Once per calendar day, offer an update until the user updates or disables reminders.
class FirmwareUpdateDailyPrompt {
  FirmwareUpdateDailyPrompt._();

  static bool _offeredThisSession = false;

  static Future<void> tryShow(BuildContext context, WidgetRef ref) async {
    if (_offeredThisSession) {
      return;
    }
    if (!context.mounted) {
      return;
    }

    final prefs = ref.read(prefsRepositoryProvider);
    if (!prefs.firmwareUpdateRemindersEnabled) {
      return;
    }

    final snap = ref.read(firmwareUpdateAvailabilityProvider);
    if (snap.loading || !snap.updateAvailable || snap.manifest == null) {
      return;
    }
    if (snap.boardOtaSupported == false) {
      return;
    }

    final today = _isoCalendarDay(DateTime.now());
    if (prefs.firmwareReminderDismissDay == today) {
      return;
    }

    _offeredThisSession = true;

    final manifest = snap.manifest!;
    final boardV = snap.boardVersion ?? '';

    final choice = await showDialog<_OfferChoice>(
      context: context,
      barrierDismissible: false,
      builder: (ctx) {
        final l10n = AppLocalizations.of(ctx)!;
        return AlertDialog(
          title: Text(l10n.firmwareDialogTitle),
          content: SingleChildScrollView(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              mainAxisSize: MainAxisSize.min,
              children: [
                Text(
                  l10n.firmwareDialogVersions(manifest.version, boardV),
                ),
                if (manifest.changelog != null &&
                    manifest.changelog!.trim().isNotEmpty) ...[
                  const SizedBox(height: 12),
                  Text(
                    manifest.changelog!.trim(),
                    style: Theme.of(ctx).textTheme.bodySmall,
                  ),
                ],
                const SizedBox(height: 12),
                Text(l10n.firmwareDialogHttpsNote),
              ],
            ),
          ),
          actions: [
            TextButton(
              onPressed: () => Navigator.pop(ctx, _OfferChoice.disableReminders),
              child: Text(l10n.firmwareTurnOffReminders),
            ),
            TextButton(
              onPressed: () => Navigator.pop(ctx, _OfferChoice.later),
              child: Text(l10n.firmwareNotNow),
            ),
            FilledButton(
              onPressed: () => Navigator.pop(ctx, _OfferChoice.update),
              child: Text(l10n.firmwareUpdateAction),
            ),
          ],
        );
      },
    );

    if (!context.mounted) {
      return;
    }

    switch (choice) {
      case null:
        return;
      case _OfferChoice.later:
        await prefs.setFirmwareReminderDismissDay(today);
        return;
      case _OfferChoice.disableReminders:
        await prefs.setFirmwareUpdateRemindersEnabled(false);
        await prefs.setFirmwareReminderDismissDay(null);
        return;
      case _OfferChoice.update:
        break;
    }

    if (!context.mounted) {
      return;
    }
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (ctx) {
        final l10n = AppLocalizations.of(ctx)!;
        return AlertDialog(
          title: Text(l10n.firmwareDailySecondConfirmTitle),
          content: Text(l10n.firmwareDailySecondConfirmBody),
          actions: [
            TextButton(
              onPressed: () => Navigator.pop(ctx, false),
              child: Text(l10n.commonCancel),
            ),
            FilledButton(
              onPressed: () => Navigator.pop(ctx, true),
              child: Text(l10n.firmwareYesUpdate),
            ),
          ],
        );
      },
    );

    if (confirmed != true) {
      return;
    }
    if (!context.mounted) {
      return;
    }

    var messenger = ScaffoldMessenger.maybeOf(context);
    messenger?.showSnackBar(
      SnackBar(content: Text(AppLocalizations.of(context)!.firmwareStartingOtaSnack)),
    );

    final err = await FirmwareOtaRunner.execute(
      ref: ref,
      binUrl: manifest.url,
      onProgress: (_) {},
    );

    if (!context.mounted) {
      return;
    }
    messenger = ScaffoldMessenger.maybeOf(context);
    if (err != null) {
      messenger?.showSnackBar(SnackBar(content: Text(err)));
    } else {
      messenger?.showSnackBar(
        const SnackBar(
          content: Text(
            'OTA finished or connection dropped — the board may reboot.',
          ),
        ),
      );
    }
    unawaited(ref.read(firmwareUpdateAvailabilityProvider.notifier).refresh());
  }
}

enum _OfferChoice { later, disableReminders, update }
