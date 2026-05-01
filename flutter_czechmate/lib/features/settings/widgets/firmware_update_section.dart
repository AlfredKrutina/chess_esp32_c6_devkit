import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../../app_providers.dart';
import '../../../core/constants/app_environment.dart';
import '../../../core/constants/firmware_defaults.dart';
import '../../../core/models/board_firmware_models.dart';
import '../../../core/models/board_timer_state.dart';
import '../../../core/utils/board_http_base_url.dart';
import '../../connection/board_session_notifier.dart';
import '../../connection/board_session_state.dart';
import '../firmware_ota_runner.dart';
import '../firmware_update_availability.dart';

/// HTTPS OTA: phone loads `version.json` only; the `.bin` is fetched by the ESP.
class FirmwareUpdateSection extends ConsumerStatefulWidget {
  const FirmwareUpdateSection({super.key});

  @override
  ConsumerState<FirmwareUpdateSection> createState() =>
      _FirmwareUpdateSectionState();
}

class _FirmwareUpdateSectionState extends ConsumerState<FirmwareUpdateSection> {
  late final TextEditingController _manifestCtrl;
  bool _ctrlReady = false;

  EspWifiStatus? _wifiStatus;
  String? _detail;
  bool _otaBusy = false;
  int _otaPercent = 0;

  static const String _otaDisabledHint =
      'This flash layout has no OTA slots (ota_0 + ota_1). '
      'Use USB/UART (esptool or idf.py flash) or rebuild firmware with a dual-OTA '
      'partition table (see project partitions CSV).';

  @override
  void initState() {
    super.initState();
    _manifestCtrl = TextEditingController();
    WidgetsBinding.instance.addPostFrameCallback((_) async {
      final prefs = ref.read(prefsRepositoryProvider);
      final saved = prefs.firmwareManifestUrl?.trim();
      if (saved != null && saved.isNotEmpty) {
        final fixed = normalizeFirmwareManifestUrl(saved);
        if (fixed != saved) {
          await prefs.setFirmwareManifestUrl(fixed);
        }
      }
      final prefsAfter = ref.read(prefsRepositoryProvider);
      _manifestCtrl.text = prefsAfter.firmwareManifestUrlEffective;
      setState(() => _ctrlReady = true);
      unawaited(_reloadWifi());
      unawaited(
        ref.read(firmwareUpdateAvailabilityProvider.notifier).refresh(),
      );
    });
  }

  @override
  void dispose() {
    _manifestCtrl.dispose();
    super.dispose();
  }

  String? _resolvedBoardHttpUrl(BoardSessionState session) {
    return resolveBoardHttpBaseUrl(
      wifiTransportActive: session.transport == BoardTransport.wifi,
      sessionWifiBaseUrl: session.wifiBaseUrl,
      prefsLastBoardBaseUrl: ref.read(prefsRepositoryProvider).lastBoardBaseUrl,
    );
  }

  Future<void> _reloadWifi() async {
    final session = ref.read(boardSessionNotifierProvider);
    final baseUrl = _resolvedBoardHttpUrl(session);
    if (baseUrl == null || baseUrl.isEmpty) {
      return;
    }
    try {
      final w =
          await ref.read(boardApiClientProvider).fetchWiFiStatus(baseUrl);
      if (mounted) {
        setState(() => _wifiStatus = w);
      }
    } catch (_) {}
  }

  Future<void> _saveManifestUrl() async {
    await ref
        .read(prefsRepositoryProvider)
        .setFirmwareManifestUrl(_manifestCtrl.text.trim());
    final effective = ref.read(prefsRepositoryProvider).firmwareManifestUrlEffective;
    if (mounted) {
      setState(() => _manifestCtrl.text = effective);
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Manifest URL saved')),
      );
    }
    unawaited(ref.read(firmwareUpdateAvailabilityProvider.notifier).refresh());
  }

  Future<void> _checkUpdate() async {
    final session = ref.read(boardSessionNotifierProvider);
    final baseUrl = _resolvedBoardHttpUrl(session);
    final typed = _manifestCtrl.text.trim();

    if (baseUrl == null || baseUrl.isEmpty) {
      setState(() => _detail =
          'Board HTTP address is missing (e.g. http://192.168.4.1). '
          'Set Default board URL in settings. Bluetooth alone does not provide an IP.',
        );
      return;
    }

    if (mounted) {
      setState(() => _detail = null);
    }

    await ref.read(firmwareUpdateAvailabilityProvider.notifier).refresh(
          manifestUrlOverride: typed.isEmpty ? null : typed,
        );
    final snap = ref.read(firmwareUpdateAvailabilityProvider);

    if (mounted) {
      setState(() => _detail = snap.error);
    }
    if (AppEnvironment.staging) {
      debugPrint(
        '[staging] firmware check manifest=${snap.manifest?.version} board=${snap.boardVersion}',
      );
    }
    await _reloadWifi();
  }

  Future<bool> _confirmOtaFromSettings({
    required String remoteVersion,
    required String boardVersion,
    String? changelog,
  }) async {
    final first = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Update board firmware?'),
        content: SingleChildScrollView(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            mainAxisSize: MainAxisSize.min,
            children: [
              Text('New version: $remoteVersion — on board: $boardVersion.'),
              if (changelog != null && changelog.trim().isNotEmpty) ...[
                const SizedBox(height: 12),
                Text(changelog.trim(), style: Theme.of(ctx).textTheme.bodySmall),
              ],
              const SizedBox(height: 12),
              const Text(
                'You only send an HTTPS link — the ESP downloads the full .bin. '
                'The board must be connected to Wi‑Fi as a station (STA). '
                'You can send the start command over Wi‑Fi HTTP or Bluetooth; '
                'progress is read over HTTP to the board IP.',
              ),
            ],
          ),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx, false),
            child: const Text('Cancel'),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(ctx, true),
            child: const Text('Continue'),
          ),
        ],
      ),
    );
    if (first != true || !mounted) {
      return false;
    }
    final second = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Final confirmation'),
        content: const Text(
          'The board will write firmware and reboot. Do not cut power or lose '
          'Wi‑Fi while downloading. Continue?',
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx, false),
            child: const Text('No'),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(ctx, true),
            child: const Text('Yes, update'),
          ),
        ],
      ),
    );
    return second == true;
  }

  Future<void> _startOta(String binUrl, FirmwareManifest meta) async {
    final snap = ref.read(firmwareUpdateAvailabilityProvider);
    final boardV = snap.boardVersion ?? '';
    if (!await _confirmOtaFromSettings(
      remoteVersion: meta.version,
      boardVersion: boardV,
      changelog: meta.changelog,
    )) {
      return;
    }

    setState(() {
      _otaBusy = true;
      _otaPercent = 0;
      _detail = null;
    });

    final err = await FirmwareOtaRunner.execute(
      ref: ref,
      binUrl: binUrl,
      onProgress: (p) {
        if (mounted) {
          setState(() => _otaPercent = p);
        }
      },
    );

    if (mounted) {
      setState(() => _otaBusy = false);
      if (err != null) {
        setState(() => _detail = err);
        ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text(err)));
      } else {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(
            content: Text(
              'OTA finished or connection dropped — the board may reboot.',
            ),
          ),
        );
      }
    }
    unawaited(ref.read(firmwareUpdateAvailabilityProvider.notifier).refresh());
  }

  @override
  Widget build(BuildContext context) {
    if (!_ctrlReady) {
      return const SizedBox.shrink();
    }

    final session = ref.watch(boardSessionNotifierProvider);
    final prefs = ref.watch(prefsRepositoryProvider);
    final snap = ref.watch(firmwareUpdateAvailabilityProvider);

    final baseOk = _resolvedBoardHttpUrl(session) != null;
    final boardV = snap.boardVersion ?? '—';
    final remoteV = snap.manifest?.version ?? '—';
    final updateAvailable = snap.updateAvailable;
    final remindersOn = prefs.firmwareUpdateRemindersEnabled;
    final otaCapable = snap.boardOtaSupported != false;

    final theme = Theme.of(context);
    return Padding(
      padding: const EdgeInsets.only(bottom: 8),
      child: Card(
        clipBehavior: Clip.antiAlias,
        margin: EdgeInsets.zero,
        child: Theme(
          data: theme.copyWith(dividerColor: Colors.transparent),
          child: ExpansionTile(
            tilePadding:
                const EdgeInsets.symmetric(horizontal: 16, vertical: 10),
            childrenPadding: EdgeInsets.zero,
            leading: Icon(
              Icons.system_update_alt_outlined,
              color: theme.colorScheme.primary,
            ),
            title: Text(
              updateAvailable
                  ? 'Firmware — update available ($remoteV)'
                  : 'Firmware (over-the-air)',
              style: updateAvailable
                  ? theme.textTheme.titleMedium?.copyWith(
                      fontWeight: FontWeight.w600,
                      color: theme.colorScheme.primary,
                    )
                  : theme.textTheme.titleMedium
                      ?.copyWith(fontWeight: FontWeight.w600),
            ),
            subtitle: Text(
              updateAvailable
                  ? 'Tap Update below or wait for the daily reminder (${remindersOn ? "on" : "off"}).'
                  : 'The app only forwards a link — the ESP downloads the .bin over HTTPS.',
              style: theme.textTheme.bodySmall?.copyWith(
                color: theme.colorScheme.onSurfaceVariant,
              ),
              maxLines: 2,
              overflow: TextOverflow.ellipsis,
            ),
            children: [
              Padding(
                padding: const EdgeInsets.fromLTRB(16, 0, 16, 16),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.stretch,
                  children: [
          SwitchListTile(
            contentPadding: EdgeInsets.zero,
            title: const Text('Daily update reminders'),
            subtitle: const Text(
              'If a newer version is on the server, ask again each day until you update or turn reminders off.',
            ),
            value: remindersOn,
            onChanged: (v) async {
              await prefs.setFirmwareUpdateRemindersEnabled(v);
              setState(() {});
            },
          ),
          TextField(
            controller: _manifestCtrl,
            decoration: const InputDecoration(
              labelText: 'Manifest URL (version.json)',
              hintText: 'https://…/version.json',
              helperText:
                  'Default URL is filled automatically. Clear the field and Save to use the built-in manifest only.',
              border: OutlineInputBorder(),
            ),
            keyboardType: TextInputType.url,
          ),
          const SizedBox(height: 8),
          Row(
            children: [
              FilledButton.tonal(
                onPressed: snap.loading || _otaBusy ? null : _saveManifestUrl,
                child: const Text('Save manifest URL'),
              ),
              const SizedBox(width: 8),
              FilledButton(
                onPressed: snap.loading || _otaBusy ? null : _checkUpdate,
                child: snap.loading
                    ? const SizedBox(
                        width: 20,
                        height: 20,
                        child: CircularProgressIndicator(strokeWidth: 2),
                      )
                    : const Text('Check for update'),
              ),
            ],
          ),
          const SizedBox(height: 12),
          Text(
            'Board (HTTP): $boardV',
            style: Theme.of(context).textTheme.bodyMedium,
          ),
          Text(
            'Manifest: $remoteV',
            style: Theme.of(context).textTheme.bodyMedium,
          ),
          if (_wifiStatus != null)
            Text(
              'Wi‑Fi STA: ${_wifiStatus!.staConnected ? "connected (${_wifiStatus!.staIp})" : "not connected"}',
              style: Theme.of(context).textTheme.bodySmall,
            ),
          if (!baseOk)
            Padding(
              padding: const EdgeInsets.only(top: 8),
              child: Text(
                'Set and save Default board URL (AP or STA IP) to read the board version.',
                style: TextStyle(color: Theme.of(context).colorScheme.error),
              ),
            ),
          if (snap.boardOtaSupported == false) ...[
            const SizedBox(height: 12),
            Material(
              color: Theme.of(context).colorScheme.errorContainer.withValues(alpha: 0.5),
              borderRadius: BorderRadius.circular(8),
              child: Padding(
                padding: const EdgeInsets.all(12),
                child: Text(
                  _otaDisabledHint,
                  style: Theme.of(context).textTheme.bodySmall,
                ),
              ),
            ),
          ],
          if (updateAvailable && snap.manifest != null) ...[
            const SizedBox(height: 12),
            Material(
              color: Theme.of(context).colorScheme.primaryContainer.withValues(alpha: 0.35),
              borderRadius: BorderRadius.circular(12),
              child: ListTile(
                leading: Icon(Icons.system_update_alt,
                    color: Theme.of(context).colorScheme.primary),
                title: Text('New version ${snap.manifest!.version}'),
                subtitle:
                    (snap.manifest!.changelog != null &&
                        snap.manifest!.changelog!.trim().isNotEmpty)
                    ? Text(snap.manifest!.changelog!)
                    : const Text('Download runs on the ESP over HTTPS.'),
                trailing: FilledButton(
                  onPressed: !otaCapable || snap.loading || _otaBusy
                      ? null
                      : () => _startOta(snap.manifest!.url, snap.manifest!),
                  child: const Text('Update'),
                ),
              ),
            ),
          ],
          if (_otaBusy) ...[
            const SizedBox(height: 8),
            LinearProgressIndicator(
              value: _otaPercent > 0 ? _otaPercent / 100 : null,
            ),
            Text(
              'OTA $_otaPercent %',
              style: Theme.of(context).textTheme.bodySmall,
            ),
          ],
          if (_detail != null) ...[
            const SizedBox(height: 8),
            Text(
              _detail!,
              style: TextStyle(color: Theme.of(context).colorScheme.error),
            ),
          ],
          const SizedBox(height: 8),
          Text(
            'Connection: ${session.transport.name}. '
            'Start command: Wi‑Fi HTTP or BLE. Download & flash: board via STA + HTTPS.',
            style: Theme.of(context).textTheme.labelSmall,
          ),
                  ],
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}
