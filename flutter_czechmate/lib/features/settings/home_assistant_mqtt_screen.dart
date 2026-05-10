import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:url_launcher/url_launcher.dart';

import '../../app_providers.dart';
import '../../core/layout/form_factor.dart';
import '../../core/localization/context_l10n.dart';
import '../../l10n/app_localizations.dart';
import '../../core/models/status_models.dart';
import '../../core/utils/board_http_base_url.dart';
import '../../core/widgets/glass_snackbar.dart';
import '../../core/widgets/pressable_scale.dart';
import 'board_settings_error_message.dart';
import '../connection/board_session_notifier.dart';
import '../connection/board_session_state.dart';

/// Project docs (MQTT / Home Assistant context) — opens in external browser.
final Uri kMqttHaDocumentationUri = Uri.parse(
  'https://alfredkrutina.github.io/chess_esp32_c6_devkit/',
);

Future<void> openMqttHaDocumentation() async {
  if (await canLaunchUrl(kMqttHaDocumentationUri)) {
    await launchUrl(
      kMqttHaDocumentationUri,
      mode: LaunchMode.externalApplication,
    );
  }
}

/// MQTT broker fields + board status.
class HomeAssistantMqttScreen extends ConsumerStatefulWidget {
  const HomeAssistantMqttScreen({super.key});

  @override
  ConsumerState<HomeAssistantMqttScreen> createState() =>
      _HomeAssistantMqttScreenState();
}

class _HomeAssistantMqttScreenState
    extends ConsumerState<HomeAssistantMqttScreen> {
  final _host = TextEditingController();
  final _port = TextEditingController(text: '1883');
  final _user = TextEditingController();
  final _pass = TextEditingController();
  ESPMQTTStatusJSON? _status;
  bool _busy = false;

  @override
  void dispose() {
    _host.dispose();
    _port.dispose();
    _user.dispose();
    _pass.dispose();
    super.dispose();
  }

  bool get _supportsWrite {
    final s = ref.read(boardSessionNotifierProvider);
    return s.transport != BoardTransport.none && s.snapshot != null && !s.busy;
  }

  bool get _supportsWifiCommands {
    final s = ref.read(boardSessionNotifierProvider);
    return s.transport == BoardTransport.wifi && s.wifiBaseUrl != null;
  }

  String _footer(AppLocalizations l, BoardSessionState store) {
    if (store.transport == BoardTransport.mock) {
      return l.haMqttFooterMock;
    }
    if (!_supportsWrite) {
      return l.haMqttFooterConnectFirst;
    }
    if (!_supportsWifiCommands) {
      return l.haMqttFooterBleSave;
    }
    return l.haMqttFooterTroubleshoot;
  }

  String? _resolvedBoardUrl() {
    final session = ref.read(boardSessionNotifierProvider);
    final prefs = ref.read(prefsRepositoryProvider);
    return resolveBoardHttpBaseUrl(
      wifiTransportActive: session.transport == BoardTransport.wifi,
      sessionWifiBaseUrl: session.wifiBaseUrl,
      prefsLastBoardBaseUrl: prefs.lastBoardBaseUrl,
      bleStaIp: session.bleStaIp,
    );
  }

  Future<void> _refreshStatus() async {
    final url = _resolvedBoardUrl();
    if (url == null || url.isEmpty) return;
    setState(() => _busy = true);
    try {
      final m = await ref.read(boardApiClientProvider).fetchMQTTStatus(url);
      if (mounted) {
        setState(() {
          _status = m;
          _host.text = m.host;
          _port.text = m.port > 0 ? '${m.port}' : '1883';
          _user.text = m.username;
        });
      }
    } catch (e) {
      if (mounted) {
        final session = ref.read(boardSessionNotifierProvider);
        final msg = boardHttpSettingsUserMessage(context.l10n, e, session);
        showAppSnackBar(context, msg, errorStyle: true);
      }
    } finally {
      if (mounted) setState(() => _busy = false);
    }
  }

  Future<void> _save() async {
    final url = _resolvedBoardUrl();
    if (url == null || url.isEmpty) return;
    setState(() => _busy = true);
    try {
      final p = int.tryParse(_port.text.trim()) ?? 1883;
      await ref.read(boardApiClientProvider).postMQTTConfig(
            url,
            _host.text.trim(),
            p,
            username: _user.text.trim().isEmpty ? null : _user.text.trim(),
            password: _pass.text.isEmpty ? null : _pass.text,
          );
      if (mounted) {
        showAppSnackBar(context, context.l10n.haMqttSavedSnack);
      }
      await _refreshStatus();
    } catch (e) {
      if (mounted) {
        final session = ref.read(boardSessionNotifierProvider);
        showAppSnackBar(
          context,
          boardHttpSettingsUserMessage(context.l10n, e, session),
          errorStyle: true,
        );
      }
    } finally {
      if (mounted) setState(() => _busy = false);
    }
  }

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addPostFrameCallback((_) => _refreshStatus());
  }

  @override
  Widget build(BuildContext context) {
    final l10n = context.l10n;
    final store = ref.watch(boardSessionNotifierProvider);
    final cs = Theme.of(context).colorScheme;
    return Scaffold(
      appBar: AppBar(
        title: Text(l10n.haMqttTitle),
        actions: [
          IconButton(
            tooltip: l10n.haMqttRefreshFromBoard,
            onPressed: _busy || !_supportsWifiCommands ? null : _refreshStatus,
            icon: _busy
                ? const SizedBox(
                    width: 22,
                    height: 22,
                    child: CircularProgressIndicator(strokeWidth: 2),
                  )
                : const Icon(Icons.refresh),
          ),
        ],
      ),
      body: desktopSettingsDetailBody(
        FocusTraversalGroup(
          child: ListView(
            padding: const EdgeInsets.all(16),
            children: [
              Text(
                l10n.haMqttFormLead,
                style: Theme.of(context).textTheme.bodyMedium,
              ),
              const SizedBox(height: 8),
              Align(
                alignment: Alignment.centerLeft,
                child: TextButton.icon(
                  onPressed: openMqttHaDocumentation,
                  icon: Icon(Icons.open_in_new, size: 18, color: cs.primary),
                  label: Text(l10n.haMqttSetupHelpLink),
                ),
              ),
              const SizedBox(height: 12),
              Text(
                l10n.haMqttBrokerHeader,
                style: const TextStyle(fontWeight: FontWeight.bold),
              ),
              const SizedBox(height: 8),
              TextField(
                controller: _host,
                decoration: InputDecoration(
                  labelText: l10n.haMqttHostFieldLabel,
                  border: const OutlineInputBorder(),
                ),
                keyboardType: TextInputType.url,
              ),
              const SizedBox(height: 8),
              TextField(
                controller: _port,
                decoration: InputDecoration(
                  labelText: l10n.haMqttPortFieldLabel,
                  border: const OutlineInputBorder(),
                ),
                keyboardType: TextInputType.number,
              ),
              const SizedBox(height: 8),
              TextField(
                controller: _user,
                decoration: InputDecoration(
                  labelText: l10n.haMqttUserFieldLabel,
                  border: const OutlineInputBorder(),
                ),
              ),
              const SizedBox(height: 8),
              TextField(
                controller: _pass,
                decoration: InputDecoration(
                  labelText: l10n.haMqttPasswordFieldLabel,
                  border: const OutlineInputBorder(),
                ),
                obscureText: true,
              ),
              const SizedBox(height: 12),
              PressableScale(
                child: FilledButton.icon(
                  onPressed:
                      _busy || !_supportsWrite || _host.text.trim().isEmpty
                          ? null
                          : _save,
                  icon: _busy
                      ? const SizedBox(
                          width: 18,
                          height: 18,
                          child: CircularProgressIndicator(strokeWidth: 2),
                        )
                      : const Icon(Icons.upload),
                  label: Text(l10n.haMqttSaveToBoard),
                ),
              ),
              Padding(
                padding: const EdgeInsets.only(top: 12),
                child: Text(_footer(l10n, store),
                    style: Theme.of(context).textTheme.bodySmall),
              ),
              if (_status != null) ...[
                const Divider(height: 32),
                Text(
                  l10n.haMqttBoardStatusHeader,
                  style: const TextStyle(fontWeight: FontWeight.bold),
                ),
                ListTile(
                    title: Text(l10n.haMqttModeTile),
                    subtitle: Text(_status!.mode)),
                ListTile(
                  title: Text(l10n.haMqttMqttTile),
                  subtitle: Text(_status!.mqttConnected
                      ? l10n.haMqttStateConnected
                      : l10n.haMqttStateDisconnected),
                ),
                ListTile(
                  title: Text(l10n.haMqttWifiStaTile),
                  subtitle: Text(_status!.wifiConnected
                      ? l10n.haMqttWifiOk
                      : l10n.haMqttWifiNoLink),
                ),
              ],
            ],
          ),
        ),
        maxWidth: kDesktopSettingsWideDetailMaxWidth,
      ),
    );
  }
}
