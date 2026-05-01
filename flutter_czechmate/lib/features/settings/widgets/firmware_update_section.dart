import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../../app_providers.dart';
import '../../../core/constants/app_environment.dart';
import '../../../core/models/board_firmware_models.dart';
import '../../../core/models/board_timer_state.dart';
import '../../../core/utils/board_http_base_url.dart';
import '../../connection/board_session_notifier.dart';
import '../../connection/board_session_state.dart';
import '../firmware_ota_runner.dart';
import '../firmware_update_availability.dart';

/// OTA přes URL: telefon stáhne jen `version.json`, `.bin` táhne sama deska (HTTPS).
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

  @override
  void initState() {
    super.initState();
    _manifestCtrl = TextEditingController();
    WidgetsBinding.instance.addPostFrameCallback((_) {
      final prefs = ref.read(prefsRepositoryProvider);
      final saved = prefs.firmwareManifestUrl;
      _manifestCtrl.text = (saved != null && saved.isNotEmpty)
          ? saved
          : prefs.firmwareManifestUrlEffective;
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
    if (mounted) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('URL manifestu uložena')),
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
          'Chybí HTTP adresa desky (např. http://192.168.4.1). Ulož ji v „Default board URL“. BLE bez IP nestačí.',
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
        title: const Text('Spustit aktualizaci desky?'),
        content: SingleChildScrollView(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            mainAxisSize: MainAxisSize.min,
            children: [
              Text('Nová verze: $remoteVersion → na desce: $boardVersion.'),
              if (changelog != null && changelog.trim().isNotEmpty) ...[
                const SizedBox(height: 12),
                Text(changelog.trim(), style: Theme.of(ctx).textTheme.bodySmall),
              ],
              const SizedBox(height: 12),
              const Text(
                'Potvrzením odešleš jen HTTPS odkaz — celý .bin si stáhne ESP. '
                'Potřebuješ Wi‑Fi STA na desce.',
              ),
            ],
          ),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx, false),
            child: const Text('Zrušit'),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(ctx, true),
            child: const Text('Pokračovat'),
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
        title: const Text('Naposledy potvrdit'),
        content: const Text(
          'Deska zapíše firmware do flash a restartuje se. Nepřerušuj napájení '
          'ani Wi‑Fi během stahování. Opravdu pokračovat?',
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx, false),
            child: const Text('Ne'),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(ctx, true),
            child: const Text('Ano, aktualizovat'),
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
              'OTA dokončena nebo spojení přerušeno — deska se může restartovat.',
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

    return Card(
      child: ExpansionTile(
        tilePadding: const EdgeInsets.symmetric(horizontal: 16),
        childrenPadding: const EdgeInsets.fromLTRB(16, 0, 16, 16),
        title: Text(
          updateAvailable ? 'Firmware — dostupná aktualizace ($remoteV)' : 'Firmware (OTA z internetu)',
          style: updateAvailable
              ? TextStyle(
                  fontWeight: FontWeight.w600,
                  color: Theme.of(context).colorScheme.primary,
                )
              : null,
        ),
        subtitle: Text(
          updateAvailable
              ? 'Ťukni níže na Aktualizovat nebo počkej na denní připomínku (${remindersOn ? "zapnuto" : "vypnuto"}).'
              : 'Aplikace jen předá odkaz — soubor .bin stahuje přímo ESP přes HTTPS.',
        ),
        children: [
          SwitchListTile(
            contentPadding: EdgeInsets.zero,
            title: const Text('Denní připomínky aktualizace'),
            subtitle: const Text(
              'Pokud je na serveru novější verze, aplikace se zeptá znovu každý den, dokud neaktualizuješ nebo nepřipomínky nevypneš.',
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
              labelText: 'URL manifestu (version.json)',
              hintText: 'https://…/version.json',
              helperText: 'Prázdné = výchozí manifest projektu na GitHub Pages',
              border: OutlineInputBorder(),
            ),
            keyboardType: TextInputType.url,
          ),
          const SizedBox(height: 8),
          Row(
            children: [
              FilledButton.tonal(
                onPressed: snap.loading || _otaBusy ? null : _saveManifestUrl,
                child: const Text('Uložit manifest URL'),
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
                    : const Text('Zkontrolovat'),
              ),
            ],
          ),
          const SizedBox(height: 12),
          Text(
            'Deska (HTTP): $boardV',
            style: Theme.of(context).textTheme.bodyMedium,
          ),
          Text(
            'V manifestu: $remoteV',
            style: Theme.of(context).textTheme.bodyMedium,
          ),
          if (_wifiStatus != null)
            Text(
              'Wi‑Fi STA: ${_wifiStatus!.staConnected ? "připojeno (${_wifiStatus!.staIp})" : "nepřipojeno"}',
              style: Theme.of(context).textTheme.bodySmall,
            ),
          if (!baseOk)
            Padding(
              padding: const EdgeInsets.only(top: 8),
              child: Text(
                'Nastav a ulož „Default board URL“ (AP nebo STA IP), jinak nelze číst verzi z desky.',
                style: TextStyle(color: Theme.of(context).colorScheme.error),
              ),
            ),
          if (updateAvailable && snap.manifest != null) ...[
            const SizedBox(height: 12),
            Material(
              color: Theme.of(context).colorScheme.primaryContainer.withValues(alpha: 0.35),
              borderRadius: BorderRadius.circular(12),
              child: ListTile(
                leading: Icon(Icons.system_update_alt,
                    color: Theme.of(context).colorScheme.primary),
                title: Text('Nová verze ${snap.manifest!.version}'),
                subtitle:
                    (snap.manifest!.changelog != null &&
                        snap.manifest!.changelog!.trim().isNotEmpty)
                    ? Text(snap.manifest!.changelog!)
                    : const Text('Stažení .bin proběhne na ESP přes HTTPS.'),
                trailing: FilledButton(
                  onPressed: snap.loading || _otaBusy
                      ? null
                      : () => _startOta(snap.manifest!.url, snap.manifest!),
                  child: const Text('Aktualizovat'),
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
            'Transport: ${session.transport.name}. '
            'OTA příkaz jde i přes BLE; průběh se čte přes HTTP.',
            style: Theme.of(context).textTheme.labelSmall,
          ),
        ],
      ),
    );
  }
}
