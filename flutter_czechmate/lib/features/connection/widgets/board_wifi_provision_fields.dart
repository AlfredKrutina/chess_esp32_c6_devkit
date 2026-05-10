import 'package:flutter/material.dart';

import '../../../core/localization/context_l10n.dart';

/// Společná pole SSID/heslo + akce pro provisioning STA přes BLE (sheet i Nastavení).
class BoardWifiProvisionFields extends StatelessWidget {
  const BoardWifiProvisionFields({
    super.key,
    required this.ssidController,
    required this.pwdController,
    required this.onSendCredentials,
    required this.onUsePhoneSsid,
    this.onScanBoardNetworks,
    this.scanBusy = false,
    this.sendBusy = false,
    this.surveyPhoneVisible,
    this.surveyDisplaySsidForChip,
    this.actionsEnabled = true,
    this.denseSubtitle = false,

    /// `false` ve spodním listu po Objevování — nadpis je už v sheetu.
    this.showSectionHeader = true,
  });

  final TextEditingController ssidController;
  final TextEditingController pwdController;
  final VoidCallback onSendCredentials;
  final VoidCallback onUsePhoneSsid;
  final VoidCallback? onScanBoardNetworks;
  final bool scanBusy;
  final bool sendBusy;

  /// `null` — ještě bez výsledku skenu; `true` / `false` — viditelnost aktuálního SSID pole.
  final bool? surveyPhoneVisible;

  /// SSID zobrazený v chipu „viditelná“ (aktuální síť zařízení / pole).
  final String? surveyDisplaySsidForChip;
  final bool actionsEnabled;
  final bool denseSubtitle;
  final bool showSectionHeader;

  @override
  Widget build(BuildContext context) {
    final l10n = context.l10n;
    final cs = Theme.of(context).colorScheme;
    final displaySsid =
        (surveyDisplaySsidForChip ?? ssidController.text).trim();
    final leadOrFirmwareSubtitle = denseSubtitle
        ? l10n.boardWifiProvisionSheetLead
        : l10n.firmwareWifiBleProvisionSubtitle;
    return FocusTraversalGroup(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          if (showSectionHeader) ...[
            Text(
              l10n.firmwareWifiBleProvisionTitle,
              style: Theme.of(context).textTheme.titleSmall,
            ),
            SizedBox(height: denseSubtitle ? 4 : 8),
            Text(
              leadOrFirmwareSubtitle,
              style: Theme.of(context).textTheme.bodySmall?.copyWith(
                    color: cs.onSurfaceVariant,
                  ),
            ),
            SizedBox(height: denseSubtitle ? 8 : 12),
          ] else ...[
            Text(
              leadOrFirmwareSubtitle,
              style: Theme.of(context).textTheme.bodySmall?.copyWith(
                    color: cs.onSurfaceVariant,
                  ),
            ),
            const SizedBox(height: 10),
          ],
          Text(
            l10n.boardWifiProvisionIosSsidHint,
            style: Theme.of(context).textTheme.bodySmall?.copyWith(
                  color: cs.onSurfaceVariant,
                ),
          ),
          const SizedBox(height: 12),
          if (surveyPhoneVisible != null)
            Padding(
              padding: const EdgeInsets.only(bottom: 10),
              child: Align(
                alignment: Alignment.centerLeft,
                child: Chip(
                  avatar: Icon(
                    surveyPhoneVisible!
                        ? Icons.check_circle_outline
                        : Icons.warning_amber_outlined,
                    size: 18,
                    color: surveyPhoneVisible! ? cs.primary : cs.tertiary,
                  ),
                  label: Text(
                    surveyPhoneVisible!
                        ? l10n.boardWifiProvisionVisibleYes(
                            displaySsid.isEmpty ? '…' : displaySsid,
                          )
                        : l10n.boardWifiProvisionVisibleNo,
                    style: Theme.of(context).textTheme.bodySmall,
                  ),
                ),
              ),
            ),
          TextField(
            controller: ssidController,
            enabled: actionsEnabled && !sendBusy,
            decoration: InputDecoration(
              labelText: l10n.firmwareWifiBleSsidLabel,
              border: const OutlineInputBorder(),
            ),
          ),
          const SizedBox(height: 8),
          TextField(
            controller: pwdController,
            enabled: actionsEnabled && !sendBusy,
            obscureText: true,
            decoration: InputDecoration(
              labelText: l10n.firmwareWifiBlePasswordLabel,
              border: const OutlineInputBorder(),
            ),
          ),
          const SizedBox(height: 8),
          Wrap(
            spacing: 8,
            runSpacing: 8,
            children: [
              TextButton(
                onPressed: !actionsEnabled || sendBusy ? null : onUsePhoneSsid,
                child: Text(l10n.firmwareWifiBleUsePhoneSsidButton),
              ),
              if (onScanBoardNetworks != null)
                TextButton(
                  onPressed: !actionsEnabled || sendBusy || scanBusy
                      ? null
                      : onScanBoardNetworks,
                  child: Text(
                    scanBusy
                        ? l10n.boardWifiProvisionScanning
                        : l10n.boardWifiProvisionScanBoardButton,
                  ),
                ),
            ],
          ),
          const SizedBox(height: 8),
          FilledButton.tonal(
            onPressed: !actionsEnabled || sendBusy ? null : onSendCredentials,
            child: Text(
              l10n.firmwareWifiBleSendCredentials,
              textAlign: TextAlign.center,
            ),
          ),
        ],
      ),
    );
  }
}
