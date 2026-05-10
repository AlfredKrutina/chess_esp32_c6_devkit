import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../../core/localization/context_l10n.dart';
import '../../../core/platform/host_runtime_permissions.dart';
import '../../../core/widgets/app_modal_sheet.dart';
import '../../../core/widgets/glass_snackbar.dart';
import '../../../core/utils/phone_wifi_info.dart';
import '../../../core/utils/user_facing_error_message.dart';
import '../board_session_notifier.dart';
import '../board_session_state.dart';
import 'board_wifi_provision_fields.dart';

/// Spodní list: provisioning domácí Wi‑Fi přes BLE po připojení k desce.
Future<void> showBoardWifiProvisionSheet(BuildContext context) async {
  await showAppModalBottomSheet<void>(
    context: context,
    isScrollControlled: true,
    showDragHandle: true,
    escapeToDismiss: false,
    builder: (ctx) {
      return Padding(
        padding: EdgeInsets.only(
          bottom: MediaQuery.viewInsetsOf(ctx).bottom,
        ),
        child: const _BoardWifiProvisionSheetBody(),
      );
    },
  );
}

class _BoardWifiProvisionSheetBody extends ConsumerStatefulWidget {
  const _BoardWifiProvisionSheetBody();

  @override
  ConsumerState<_BoardWifiProvisionSheetBody> createState() =>
      _BoardWifiProvisionSheetBodyState();
}

class _BoardWifiProvisionSheetBodyState
    extends ConsumerState<_BoardWifiProvisionSheetBody> {
  late final TextEditingController _ssid;
  late final TextEditingController _pwd;
  bool _scanBusy = false;
  bool _sendBusy = false;
  bool? _surveyVisible;
  String? _compareSsid;

  @override
  void initState() {
    super.initState();
    _ssid = TextEditingController();
    _pwd = TextEditingController();
    WidgetsBinding.instance.addPostFrameCallback((_) async {
      if (defaultTargetPlatform == TargetPlatform.android ||
          defaultTargetPlatform == TargetPlatform.iOS) {
        await ensureWifiSsidReadPermissions();
      }
      final guess = await PhoneWifiInfo.tryCurrentWifiSsid();
      if (!mounted) return;
      setState(() {
        _compareSsid = guess;
        if (guess != null && _ssid.text.isEmpty) {
          _ssid.text = guess;
        }
      });
    });
  }

  @override
  void dispose() {
    _ssid.dispose();
    _pwd.dispose();
    super.dispose();
  }

  String get _ssidForSurveyCompare =>
      _ssid.text.trim().isNotEmpty ? _ssid.text.trim() : (_compareSsid ?? '');

  Future<void> _scan() async {
    setState(() {
      _scanBusy = true;
      _surveyVisible = null;
    });
    try {
      final r = await ref
          .read(boardSessionNotifierProvider.notifier)
          .fetchWifiSurveyOverBle();
      if (!mounted) return;
      if (!r.ok) {
        setState(() => _surveyVisible = null);
        showAppSnackBar(
          context,
          context.l10n.boardWifiProvisionSurveyFailed,
          errorStyle: true,
        );
        return;
      }
      final surveySsid = _ssidForSurveyCompare;
      setState(() {
        _surveyVisible =
            surveySsid.isNotEmpty ? r.hasSsid(surveySsid) : false;
      });
    } catch (e) {
      if (mounted) {
        final l10n = context.l10n;
        showAppSnackBar(
          context,
          userFacingErrorSummary(l10n, e),
          errorStyle: true,
        );
      }
    } finally {
      if (mounted) setState(() => _scanBusy = false);
    }
  }

  Future<void> _send() async {
    final ssid = _ssid.text.trim();
    if (ssid.isEmpty) {
      showAppSnackBar(
        context,
        context.l10n.errWifiSsidEmpty,
        errorStyle: true,
      );
      return;
    }
    setState(() => _sendBusy = true);
    try {
      await ref
          .read(boardSessionNotifierProvider.notifier)
          .provisionStaWifiOverBle(
            ssid: ssid,
            password: _pwd.text,
          );
      if (!mounted) return;
      showAppSnackBar(context, context.l10n.boardWifiProvisionDoneSnack);
      await Future<void>.delayed(const Duration(seconds: 12));
      if (!mounted) return;
      final s = ref.read(boardSessionNotifierProvider);
      if (s.transport == BoardTransport.ble &&
          s.bleGattConnected &&
          !s.bleStaConnected) {
        showAppSnackBar(
          context,
          context.l10n.boardWifiProvisionStaTimeoutHint,
        );
      }
    } catch (e) {
      if (mounted) {
        final l10n = context.l10n;
        showAppSnackBar(
          context,
          userFacingErrorSummary(l10n, e),
          errorStyle: true,
        );
      }
    } finally {
      if (mounted) setState(() => _sendBusy = false);
    }
  }

  Future<void> _fillPhoneSsid() async {
    final l10n = context.l10n;
    if (defaultTargetPlatform == TargetPlatform.android ||
        defaultTargetPlatform == TargetPlatform.iOS) {
      final ok = await ensureWifiSsidReadPermissions();
      if (!mounted) return;
      if (!ok) {
        showAppSnackBar(context, l10n.onboardingPermDeniedSnack,
            errorStyle: true);
        return;
      }
    }
    final s = await PhoneWifiInfo.tryCurrentWifiSsid();
    if (!mounted) return;
    setState(() {
      _compareSsid = s;
      if (s != null) {
        _ssid.text = s;
      }
      _surveyVisible = null;
    });
  }

  @override
  Widget build(BuildContext context) {
    final session = ref.watch(boardSessionNotifierProvider);
    final bleOk =
        session.transport == BoardTransport.ble && session.bleGattConnected;
    final l10n = context.l10n;

    return SafeArea(
      child: SingleChildScrollView(
        padding: const EdgeInsets.fromLTRB(20, 8, 20, 24),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Text(
              l10n.boardWifiProvisionSheetTitle,
              style: Theme.of(context).textTheme.titleLarge?.copyWith(
                    fontWeight: FontWeight.w600,
                  ),
            ),
            const SizedBox(height: 16),
            if (!bleOk)
              Padding(
                padding: const EdgeInsets.only(bottom: 12),
                child: Text(
                  l10n.errWifiProvNeedsBle,
                  style: TextStyle(color: Theme.of(context).colorScheme.error),
                ),
              ),
            BoardWifiProvisionFields(
              ssidController: _ssid,
              pwdController: _pwd,
              onSendCredentials: bleOk && !_sendBusy ? _send : () {},
              onUsePhoneSsid: bleOk && !_sendBusy ? _fillPhoneSsid : () {},
              onScanBoardNetworks: bleOk ? _scan : null,
              scanBusy: _scanBusy,
              sendBusy: _sendBusy,
              surveyPhoneVisible: _surveyVisible,
              surveyDisplaySsidForChip: _ssidForSurveyCompare.isNotEmpty
                  ? _ssidForSurveyCompare
                  : _compareSsid,
              actionsEnabled: bleOk,
              denseSubtitle: true,
              showSectionHeader: false,
            ),
            if (session.bleStaConnected)
              Padding(
                padding: const EdgeInsets.only(top: 12),
                child: Text(
                  l10n.firmwareBleStaOk(
                    session.bleStaSsid ?? '—',
                    session.bleStaIp ?? '—',
                  ),
                  style: Theme.of(context).textTheme.bodySmall,
                ),
              ),
          ],
        ),
      ),
    );
  }
}
