import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app_providers.dart';
import 'firmware_ota_runner.dart';
import 'firmware_update_availability.dart';

String _isoCalendarDay(DateTime d) =>
    '${d.year.toString().padLeft(4, '0')}-${d.month.toString().padLeft(2, '0')}-${d.day.toString().padLeft(2, '0')}';

/// Jednou za kalendářní den nabídne aktualizaci, dokud uživatel neaktualizuje nebo nevypne připomínky.
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
      builder: (ctx) => AlertDialog(
        title: const Text('Aktualizace firmwaru desky'),
        content: SingleChildScrollView(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            mainAxisSize: MainAxisSize.min,
            children: [
              Text(
                'Na serveru je verze ${manifest.version}, na desce máte $boardV.',
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
              const Text(
                'Soubor stáhne přímo deska přes HTTPS (Wi‑Fi). Telefon přenáší jen odkaz.',
              ),
            ],
          ),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx, _OfferChoice.disableReminders),
            child: const Text('Nepřipomínat'),
          ),
          TextButton(
            onPressed: () => Navigator.pop(ctx, _OfferChoice.later),
            child: const Text('Teď ne'),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(ctx, _OfferChoice.update),
            child: const Text('Aktualizovat'),
          ),
        ],
      ),
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
      builder: (ctx) => AlertDialog(
        title: const Text('Potvrdit aktualizaci'),
        content: const Text(
          'Opravdu spustit aktualizaci? Deska si stáhne firmware z internetu, '
          'projde zápis do flash a restartuje se. Nepřerušuj napájení.',
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx, false),
            child: const Text('Zrušit'),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(ctx, true),
            child: const Text('Ano, aktualizovat'),
          ),
        ],
      ),
    );

    if (confirmed != true) {
      return;
    }
    if (!context.mounted) {
      return;
    }

    var messenger = ScaffoldMessenger.maybeOf(context);
    messenger?.showSnackBar(
      const SnackBar(content: Text('Spouštím OTA na desce…')),
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
          content: Text('OTA dokončena nebo spojení přerušeno — deska se může restartovat.'),
        ),
      );
    }
    unawaited(ref.read(firmwareUpdateAvailabilityProvider.notifier).refresh());
  }
}

enum _OfferChoice { later, disableReminders, update }
