import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app_providers.dart';
import '../../core/models/status_models.dart';
import '../../core/utils/board_http_base_url.dart';
import 'board_settings_error_message.dart';
import '../connection/board_session_notifier.dart';
import '../connection/board_session_state.dart';

/// Parita `HomeAssistantMqttSetupView.swift` (návod + broker + stav).
class HomeAssistantMqttScreen extends ConsumerStatefulWidget {
  const HomeAssistantMqttScreen({super.key});

  @override
  ConsumerState<HomeAssistantMqttScreen> createState() => _HomeAssistantMqttScreenState();
}

class _HomeAssistantMqttScreenState extends ConsumerState<HomeAssistantMqttScreen> {
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

  String _footer(BoardSessionState store) {
    if (store.transport == BoardTransport.mock) {
      return 'Ukázková šachovnice MQTT nepodporuje — připoj reálnou desku.';
    }
    if (!_supportsWrite) {
      return 'Nejdřív připoj šachovnici (Bluetooth nebo Wi‑Fi), pak můžeš uložit broker.';
    }
    if (!_supportsWifiCommands) {
      return 'Broker uložíš i přes Bluetooth; stav z desky (tlačítko výše) se načte až při HTTP přes Wi‑Fi.';
    }
    return 'Pokud MQTT zůstane offline, zkontroluj firewall na HA, heslo Mosquitto a že ESP je ve stejné síti jako broker.';
  }

  String? _resolvedBoardUrl() {
    final session = ref.read(boardSessionNotifierProvider);
    final prefs = ref.read(prefsRepositoryProvider);
    return resolveBoardHttpBaseUrl(
      wifiTransportActive: session.transport == BoardTransport.wifi,
      sessionWifiBaseUrl: session.wifiBaseUrl,
      prefsLastBoardBaseUrl: prefs.lastBoardBaseUrl,
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
        final msg = boardHttpSettingsUserMessage(e, session);
        ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text(msg)));
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
        ScaffoldMessenger.of(context).showSnackBar(const SnackBar(content: Text('MQTT uloženo na desku')));
      }
      await _refreshStatus();
    } catch (e) {
      if (mounted) {
        final session = ref.read(boardSessionNotifierProvider);
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text(boardHttpSettingsUserMessage(e, session))),
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
    final store = ref.watch(boardSessionNotifierProvider);
    return Scaffold(
      appBar: AppBar(title: const Text('Home Assistant a MQTT')),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          const Text('Jak na Home Assistant', style: TextStyle(fontWeight: FontWeight.bold)),
          const SizedBox(height: 8),
          Text(
            '1. Home Assistant musí běžet ve stejné Wi‑Fi síti jako šachovnice (ESP připojené jako klient STA k routeru).\n\n'
            '2. V Home Assistantu zapni MQTT broker — nejčastěji doplněk „Mosquitto broker“ (Settings → Add-ons). Poznamenej si port (obvykle 1883) a případné uživatelské jméno a heslo z konfigurace doplňku.\n\n'
            '3. Zjisti IP adresu nebo hostname počítače / Raspberry Pi, kde HA běží (např. 192.168.1.42 nebo homeassistant.local). Šachovnice musí na tuto adresu v síti dosáhnout.\n\n'
            '4. Níže zadej stejnou adresu jako „Broker (host)“ — jde o adresu MQTT serveru (u běžné instalace stejný stroj jako Home Assistant). Port nech 1883, pokud jsi v Mosquitto neměnil výchozí.\n\n'
            '5. Uložením odešleš nastavení do firmware desky přes aktuální spojení (Wi‑Fi nebo Bluetooth). Aplikace CZECHMATE sama k brokeru nepřipojuje — data posílá jen ESP.',
            style: Theme.of(context).textTheme.bodySmall?.copyWith(color: Theme.of(context).colorScheme.onSurfaceVariant),
          ),
          const Divider(height: 32),
          const Text('MQTT broker', style: TextStyle(fontWeight: FontWeight.bold)),
          const SizedBox(height: 8),
          TextField(
            controller: _host,
            decoration: const InputDecoration(
              labelText: 'Broker (host), např. IP Home Assistant',
              border: OutlineInputBorder(),
            ),
            keyboardType: TextInputType.url,
          ),
          const SizedBox(height: 8),
          TextField(
            controller: _port,
            decoration: const InputDecoration(labelText: 'Port', border: OutlineInputBorder()),
            keyboardType: TextInputType.number,
          ),
          const SizedBox(height: 8),
          TextField(
            controller: _user,
            decoration: const InputDecoration(labelText: 'Uživatel (volitelné)', border: OutlineInputBorder()),
          ),
          const SizedBox(height: 8),
          TextField(
            controller: _pass,
            decoration: const InputDecoration(labelText: 'Heslo (volitelné)', border: OutlineInputBorder()),
            obscureText: true,
          ),
          const SizedBox(height: 12),
          FilledButton.icon(
            onPressed: _busy || !_supportsWrite || _host.text.trim().isEmpty ? null : _save,
            icon: _busy ? const SizedBox(width: 18, height: 18, child: CircularProgressIndicator(strokeWidth: 2)) : const Icon(Icons.upload),
            label: const Text('Uložit na šachovnici'),
          ),
          const SizedBox(height: 8),
          OutlinedButton(
            onPressed: _busy || !_supportsWifiCommands ? null : _refreshStatus,
            child: const Text('Obnovit stav z desky'),
          ),
          Padding(
            padding: const EdgeInsets.only(top: 12),
            child: Text(_footer(store), style: Theme.of(context).textTheme.bodySmall),
          ),
          if (_status != null) ...[
            const Divider(height: 32),
            const Text('Stav na desce', style: TextStyle(fontWeight: FontWeight.bold)),
            ListTile(title: const Text('Režim'), subtitle: Text(_status!.mode)),
            ListTile(
              title: const Text('MQTT'),
              subtitle: Text(_status!.mqttConnected ? 'připojeno' : 'nepřipojeno'),
            ),
            ListTile(
              title: const Text('Wi‑Fi STA'),
              subtitle: Text(_status!.wifiConnected ? 'OK' : 'bez spojení'),
            ),
          ],
        ],
      ),
    );
  }
}
