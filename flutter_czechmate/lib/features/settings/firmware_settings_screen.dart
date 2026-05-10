import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../core/localization/context_l10n.dart';
import 'firmware_update_availability.dart';
import 'widgets/firmware_update_section.dart';

/// Firmware updates — full screen so the main Settings list stays flat.
class FirmwareSettingsScreen extends ConsumerWidget {
  const FirmwareSettingsScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final snap = ref.watch(firmwareUpdateAvailabilityProvider);
    final remoteV = snap.manifest?.version ?? '—';
    final updateAvailable = snap.updateAvailable;
    final showBleGit = snap.showBleGitFirmwareActions;
    final title = updateAvailable
        ? context.l10n.firmwareTileTitleUpdateAvailable(remoteV)
        : (showBleGit
            ? context.l10n.firmwareTileTitleGitBle(remoteV)
            : context.l10n.firmwareTileTitleDefault);

    return Scaffold(
      appBar: AppBar(title: Text(title)),
      body: ListView(
        padding: const EdgeInsets.fromLTRB(16, 16, 16, 32),
        children: const [
          FirmwareUpdateSection(),
        ],
      ),
    );
  }
}
