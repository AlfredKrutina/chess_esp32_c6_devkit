import 'package:flutter/material.dart';

import '../../../core/localization/context_l10n.dart';

/// Rozhodnutí uživatele když HTTPS OTA vyžaduje STA s internetem.
enum OtaHttpsStaGateChoice {
  abort,
  bleUpload,
  hotspotBle,
  wifiTips,
}

Future<OtaHttpsStaGateChoice?> showOtaHttpsStaGateDialog(
  BuildContext context, {
  required bool bleUploadAvailable,
}) {
  final l10n = context.l10n;
  return showDialog<OtaHttpsStaGateChoice>(
    context: context,
    builder: (ctx) => AlertDialog(
      title: Text(l10n.otaHttpsStaGateTitle),
      content: SingleChildScrollView(
        child: Text(l10n.otaHttpsStaGateBody),
      ),
      actions: [
        TextButton(
          onPressed: () => Navigator.pop(ctx, OtaHttpsStaGateChoice.abort),
          child: Text(l10n.otaHttpsStaGateCancel),
        ),
        if (bleUploadAvailable)
          TextButton(
            onPressed: () =>
                Navigator.pop(ctx, OtaHttpsStaGateChoice.bleUpload),
            child: Text(l10n.otaHttpsStaGateBleUpload),
          ),
        TextButton(
          onPressed: () => Navigator.pop(ctx, OtaHttpsStaGateChoice.wifiTips),
          child: Text(l10n.otaHttpsStaGateWifiTips),
        ),
        TextButton(
          onPressed: () => Navigator.pop(ctx, OtaHttpsStaGateChoice.hotspotBle),
          child: Text(l10n.otaHttpsStaGateHotspotBle),
        ),
      ],
    ),
  );
}

Future<void> showOtaHttpsWifiTipsDialog(BuildContext context) {
  final l10n = context.l10n;
  return showDialog<void>(
    context: context,
    builder: (ctx) => AlertDialog(
      title: Text(l10n.otaHttpsWifiTipsTitle),
      content: SingleChildScrollView(
        child: Text(l10n.otaHttpsWifiTipsBody),
      ),
      actions: [
        TextButton(
          onPressed: () => Navigator.pop(ctx),
          child: Text(l10n.commonOk),
        ),
      ],
    ),
  );
}
