import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../../app_providers.dart';
import '../../../core/utils/board_http_base_url.dart';
import 'board_settings_error_message.dart';
import '../../../core/models/status_models.dart';
import '../connection/board_session_notifier.dart';
import '../connection/board_session_state.dart';

class BoardDeviceFeaturesView extends ConsumerStatefulWidget {
  const BoardDeviceFeaturesView({super.key});

  @override
  ConsumerState<BoardDeviceFeaturesView> createState() => _BoardDeviceFeaturesViewState();
}

class _BoardDeviceFeaturesViewState extends ConsumerState<BoardDeviceFeaturesView> {
  BoardUISettingsEnvelope? _settings;
  bool _isLoading = false;
  /// Proč se NVS blob nenačetl (HTTP URL, BLE bez LAN, …) — ne „deska odpojená“ kvůli špatné diagnostice.
  String? _emptyStateDetail;

  @override
  void initState() {
    super.initState();
    _fetch();
  }

  String _botFooterText() {
    final session = ref.watch(boardSessionNotifierProvider);
    final mock = ref.watch(prefsRepositoryProvider).useMockBoard;
    final wifiCmd = session.transport == BoardTransport.wifi && session.wifiBaseUrl != null;
    if (mock) {
      return 'Ukázková deska: NVS zápis se neprovede na reálný hardware.';
    }
    if (wifiCmd) {
      return 'Wi‑Fi session je aktivní — tlačítkem „Uložit“ odešleš bot režim do NVS na ESP.';
    }
    if (session.transport == BoardTransport.ble) {
      return 'BLE: podle firmware lze posílat NVS/UI blob; pro jistotu ověř stav na webu desky.';
    }
    return 'Pro spolehlivý zápis bota do NVS připoj desku (Wi‑Fi doporučeno).';
  }

  String _httpUnavailableExplanation(BoardSessionState session) {
    if (session.transport == BoardTransport.ble && session.bleGattConnected) {
      return 'Bluetooth je připojený, ale načtení NVS z této obrazovky probíhá přes HTTP. '
          'V Nastavení ulož platnou adresu desky (STA IP, např. http://192.168.x.x), '
          'nebo aktivuj Wi‑Fi session na tuto IP.';
    }
    if (session.transport == BoardTransport.wifi &&
        (session.wifiBaseUrl == null || session.wifiBaseUrl!.trim().isEmpty)) {
      return 'Wi‑Fi session bez platné základní URL. Obnov připojení na záložce Hra → správa šachovnice.';
    }
    return 'Chybí platná HTTP adresa desky. V Nastavení vyplň „Default board URL“ (kompletní http://…) '
        'a zkus znovu (ikonka ↻).';
  }

  Future<void> _fetch() async {
    final session = ref.read(boardSessionNotifierProvider);
    final prefs = ref.read(prefsRepositoryProvider);
    final baseUrl = resolveBoardHttpBaseUrl(
      wifiTransportActive: session.transport == BoardTransport.wifi,
      sessionWifiBaseUrl: session.wifiBaseUrl,
      prefsLastBoardBaseUrl: prefs.lastBoardBaseUrl,
    );
    if (baseUrl == null || baseUrl.isEmpty) {
      if (mounted) {
        setState(() {
          _emptyStateDetail = _httpUnavailableExplanation(session);
          _isLoading = false;
        });
      }
      return;
    }
    if (mounted) setState(() => _emptyStateDetail = null);
    setState(() => _isLoading = true);
    try {
      final env = await ref.read(boardApiClientProvider).fetchBoardUISettings(baseUrl);
      if (mounted) setState(() => _settings = env);
    } catch (e) {
      if (mounted) {
        final msg = boardHttpSettingsUserMessage(e, session);
        setState(() => _emptyStateDetail = msg);
        ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text(msg)));
      }
    } finally {
      if (mounted) setState(() => _isLoading = false);
    }
  }

  Future<void> _save() async {
    if (_settings == null) return;
    final session = ref.read(boardSessionNotifierProvider);
    final prefs = ref.read(prefsRepositoryProvider);
    final baseUrl = resolveBoardHttpBaseUrl(
      wifiTransportActive: session.transport == BoardTransport.wifi,
      sessionWifiBaseUrl: session.wifiBaseUrl,
      prefsLastBoardBaseUrl: prefs.lastBoardBaseUrl,
    );
    if (baseUrl == null || baseUrl.isEmpty) return;
    setState(() => _isLoading = true);
    try {
      final phone = ref.read(prefsRepositoryProvider);
      final merged = BoardUISettingsEnvelope(
        version: _settings!.version,
        prefs: BoardUIPrefsPayload.fromJson({
          ..._settings!.prefs.toJson(),
          'chessHintDepth': phone.hintDepth,
          'chessEvaluateMove': phone.moveEvaluationEnabled,
        }),
      );
      await ref.read(boardApiClientProvider).postBoardUISettings(baseUrl, merged);
      if (mounted) ScaffoldMessenger.of(context).showSnackBar(const SnackBar(content: Text('Uloženo do NVS desky')));
    } catch (e) {
      if (mounted) {
        final msg = boardHttpSettingsUserMessage(e, session);
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text(msg), backgroundColor: Colors.red),
        );
      }
    } finally {
      if (mounted) setState(() => _isLoading = false);
      _fetch();
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Paměť šachovnice (NVS)'),
        actions: [
          IconButton(
            icon: const Icon(Icons.refresh),
            onPressed: _isLoading ? null : _fetch,
            tooltip: 'Obnovit z desky',
          )
        ],
      ),
      body: _isLoading && _settings == null
          ? const Center(child: CircularProgressIndicator())
          : _settings == null
              ? Center(
                  child: Padding(
                    padding: const EdgeInsets.all(24),
                    child: Text(
                      _emptyStateDetail ??
                          'Data z desky se nepodařilo načíst. Klepnutím na ↻ to zkus znovu.',
                      textAlign: TextAlign.center,
                      style: Theme.of(context).textTheme.bodyLarge,
                    ),
                  ),
                )
              : ListView(
                  padding: const EdgeInsets.all(16),
                  children: [
                    Text(
                      'Při uložení na desku se z aplikace sloučí Stockfish hloubka (${ref.read(prefsRepositoryProvider).hintDepth}) '
                      'a přepínač eval (${ref.read(prefsRepositoryProvider).moveEvaluationEnabled}) do NVS payloadu (stejně jako iOS).',
                      style: Theme.of(context).textTheme.bodySmall,
                    ),
                    const SizedBox(height: 12),
                    const Text('Šachová Nápověda na desce (LED)', style: TextStyle(fontWeight: FontWeight.bold, fontSize: 16)),
                    const SizedBox(height: 8),
                    SwitchListTile(
                      title: const Text('Eval mód (Počítat nejlepší tahy)'),
                      value: _settings!.prefs.chessEvaluateMove,
                      onChanged: (v) {
                        setState(() {
                          _settings = BoardUISettingsEnvelope(
                            version: _settings!.version,
                            prefs: BoardUIPrefsPayload.fromJson({..._settings!.prefs.toJson(), 'chessEvaluateMove': v})
                          );
                        });
                      },
                    ),
                    ListTile(
                      title: const Text('Stockfish Hloubka D1-D18'),
                      trailing: Text('${_settings!.prefs.chessHintDepth}'),
                      subtitle: Slider(
                        min: 1, max: 18, divisions: 17,
                        value: _settings!.prefs.chessHintDepth.toDouble(),
                        onChanged: (v) {
                          setState(() {
                            _settings = BoardUISettingsEnvelope(
                              version: _settings!.version,
                              prefs: BoardUIPrefsPayload.fromJson({..._settings!.prefs.toJson(), 'chessHintDepth': v.round()})
                            );
                          });
                        },
                      ),
                    ),
                    SwitchListTile(
                      title: const Text('Zobrazit LED Odměny (Best Move)'),
                      value: _settings!.prefs.chessHintAwardBest,
                      onChanged: (v) {
                        setState(() {
                          _settings = BoardUISettingsEnvelope(
                            version: _settings!.version,
                            prefs: BoardUIPrefsPayload.fromJson({..._settings!.prefs.toJson(), 'chessHintAwardBest': v})
                          );
                        });
                      },
                    ),
                    SwitchListTile(
                      title: const Text('Zobrazit LED Odměny (Good Move)'),
                      value: _settings!.prefs.chessHintAwardGood,
                      onChanged: (v) {
                        setState(() {
                          _settings = BoardUISettingsEnvelope(
                            version: _settings!.version,
                            prefs: BoardUIPrefsPayload.fromJson({..._settings!.prefs.toJson(), 'chessHintAwardGood': v})
                          );
                        });
                      },
                    ),
                    SwitchListTile(
                      title: const Text('Zobrazit LED Odměny (Capture)'),
                      value: _settings!.prefs.chessHintAwardCapture,
                      onChanged: (v) {
                        setState(() {
                          _settings = BoardUISettingsEnvelope(
                            version: _settings!.version,
                            prefs: BoardUIPrefsPayload.fromJson({..._settings!.prefs.toJson(), 'chessHintAwardCapture': v})
                          );
                        });
                      },
                    ),
                    SwitchListTile(
                      title: const Text('Zobrazit Hint Statistiku v UART'),
                      value: _settings!.prefs.chessShowHintStats,
                      onChanged: (v) {
                        setState(() {
                          _settings = BoardUISettingsEnvelope(
                            version: _settings!.version,
                            prefs: BoardUIPrefsPayload.fromJson({..._settings!.prefs.toJson(), 'chessShowHintStats': v})
                          );
                        });
                      },
                    ),
                    SwitchListTile(
                      title: const Text('LED Cíl u Bota až po zvednutí (Lift)'),
                      value: _settings!.prefs.chessBotLedTargetOnlyAfterLift,
                      onChanged: (v) {
                        setState(() {
                          _settings = BoardUISettingsEnvelope(
                            version: _settings!.version,
                            prefs: BoardUIPrefsPayload.fromJson({..._settings!.prefs.toJson(), 'chessBotLedTargetOnlyAfterLift': v})
                          );
                        });
                      },
                    ),
                    SwitchListTile(
                      title: const Text('Výukový mód (Tutorials Enabled)'),
                      value: _settings!.prefs.chessTutorialsEnabled,
                      onChanged: (v) {
                        setState(() {
                          _settings = BoardUISettingsEnvelope(
                            version: _settings!.version,
                            prefs: BoardUIPrefsPayload.fromJson({..._settings!.prefs.toJson(), 'chessTutorialsEnabled': v})
                          );
                        });
                      },
                    ),
                    SwitchListTile(
                      title: const Text('Potvrzovat novou hru LED tlačítkem'),
                      value: _settings!.prefs.chessConfirmNewGame,
                      onChanged: (v) {
                        setState(() {
                          _settings = BoardUISettingsEnvelope(
                            version: _settings!.version,
                            prefs: BoardUIPrefsPayload.fromJson({..._settings!.prefs.toJson(), 'chess_confirm_new_game': v})
                          );
                        });
                      },
                    ),
                    ListTile(
                      title: const Text('Limit nápověd (0 = neomezeno)'),
                      trailing: Text('${_settings!.prefs.chessHintLimit}'),
                      subtitle: Slider(
                        min: 0, max: 99, divisions: 99,
                        value: _settings!.prefs.chessHintLimit.toDouble(),
                        onChanged: (v) {
                          setState(() {
                            _settings = BoardUISettingsEnvelope(
                              version: _settings!.version,
                              prefs: BoardUIPrefsPayload.fromJson({..._settings!.prefs.toJson(), 'chessHintLimit': v.round()})
                            );
                          });
                        },
                      ),
                    ),
                    ListTile(
                      title: const Text('Typ nápovědy (MoveHintTier)'),
                      subtitle: const Text('H1 = jen nejlepší tah, H2 = top-3, H3 = vše dle Stockfish'),
                      trailing: DropdownButton<String>(
                        value: _settings!.prefs.moveHintTier,
                        items: const [
                          DropdownMenuItem(value: 'H1', child: Text('H1 – Nejlepší')),
                          DropdownMenuItem(value: 'H2', child: Text('H2 – Top 3')),
                          DropdownMenuItem(value: 'H3', child: Text('H3 – Vše')),
                        ],
                        onChanged: (v) {
                          if (v == null) return;
                          setState(() {
                            _settings = BoardUISettingsEnvelope(
                              version: _settings!.version,
                              prefs: BoardUIPrefsPayload.fromJson({..._settings!.prefs.toJson(), 'moveHintTier': v}),
                            );
                          });
                        },
                      ),
                    ),
                    Padding(
                      padding: const EdgeInsets.only(bottom: 8),
                      child: Text(
                        'H1–H3 v telefonu (`czechmate.moveHintTier`) je samostatné od LED úrovně na desce; při POST výše se do NVS propíše i hloubka/eval z diagnostiky aplikace.',
                        style: Theme.of(context).textTheme.bodySmall,
                      ),
                    ),
                    const SizedBox(height: 8),
                    // LED Guidance level (1-5) — direct POST to /api/settings/led_guidance
                    _LedGuidanceSection(baseUrlGetter: () => ref.read(prefsRepositoryProvider).lastBoardBaseUrl),
                    // Guided capture — direct POST to /api/settings/guided_hints
                    _GuidedCaptureSection(baseUrlGetter: () => ref.read(prefsRepositoryProvider).lastBoardBaseUrl),
                    const Divider(height: 32),
                    const Text('Nastavení Oponenta (Bota)', style: TextStyle(fontWeight: FontWeight.bold, fontSize: 16)),
                    ListTile(
                      title: const Text('Režim Bota'),
                      trailing: DropdownButton<String>(
                        value: _settings!.prefs.botSettings.mode,
                        items: const [
                          DropdownMenuItem(value: 'pvp', child: Text('Hráč vs Hráč (Vypnut)')),
                          DropdownMenuItem(value: 'bot', child: Text('Hráč vs Bot')),
                        ],
                        onChanged: (v) {
                          if (v == null) return;
                          setState(() {
                            final botmap = _settings!.prefs.botSettings.toJson();
                            botmap['mode'] = v;
                            _settings = BoardUISettingsEnvelope(
                              version: _settings!.version,
                              prefs: BoardUIPrefsPayload.fromJson({..._settings!.prefs.toJson(), 'botSettings': botmap})
                            );
                          });
                        },
                      ),
                    ),
                    ListTile(
                      title: const Text('Síla Bota (Lvl)'),
                      trailing: DropdownButton<String>(
                        value: _settings!.prefs.botSettings.strength,
                        items: ['6','8','10','12','14','16'].map((lvl) => 
                           DropdownMenuItem(value: lvl, child: Text(lvl))
                        ).toList(),
                        onChanged: (v) {
                          if (v == null) return;
                          setState(() {
                            final botmap = _settings!.prefs.botSettings.toJson();
                            botmap['strength'] = v;
                            _settings = BoardUISettingsEnvelope(
                              version: _settings!.version,
                              prefs: BoardUIPrefsPayload.fromJson({..._settings!.prefs.toJson(), 'botSettings': botmap})
                            );
                          });
                        },
                      ),
                    ),
                    ListTile(
                      title: const Text('Za koho hraje Bot'),
                      trailing: DropdownButton<String>(
                        value: _settings!.prefs.botSettings.side,
                        items: const [
                          DropdownMenuItem(value: 'white', child: Text('Bílý')),
                          DropdownMenuItem(value: 'black', child: Text('Černý')),
                        ],
                        onChanged: (v) {
                          if (v == null) return;
                          setState(() {
                            final botmap = _settings!.prefs.botSettings.toJson();
                            botmap['side'] = v;
                            _settings = BoardUISettingsEnvelope(
                              version: _settings!.version,
                              prefs: BoardUIPrefsPayload.fromJson({..._settings!.prefs.toJson(), 'botSettings': botmap})
                            );
                          });
                        },
                      ),
                    ),
                    Padding(
                      padding: const EdgeInsets.only(top: 4, bottom: 8),
                      child: Text(
                        _botFooterText(),
                        style: Theme.of(context).textTheme.bodySmall,
                      ),
                    ),
                    const Divider(height: 32),
                    const Text('Demo režim', style: TextStyle(fontWeight: FontWeight.bold, fontSize: 16)),
                    const _DemoNvsSection(),
                    const SizedBox(height: 24),
                    FilledButton.icon(
                      onPressed: _isLoading ? null : _save,
                      icon: _isLoading ? const SizedBox(width: 16, height: 16, child: CircularProgressIndicator(strokeWidth: 2, color: Colors.white)) : const Icon(Icons.upload),
                      label: const Text('Uložit do NVS Desky'),
                    )
                  ],
                ),
    );
  }
}

// ── Demo (HTTP) — `fetchDemoStatus` / `postDemoConfig` / `postDemoStart` ─────

class _DemoNvsSection extends ConsumerStatefulWidget {
  const _DemoNvsSection();

  @override
  ConsumerState<_DemoNvsSection> createState() => _DemoNvsSectionState();
}

class _DemoNvsSectionState extends ConsumerState<_DemoNvsSection> {
  bool _enabled = false;
  double _speedMs = 800;
  ESPDemoStatusJSON? _boardStat;
  bool _busy = false;

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addPostFrameCallback((_) => _refresh());
  }

  Future<void> _refresh() async {
    final session = ref.read(boardSessionNotifierProvider);
    final prefs = ref.read(prefsRepositoryProvider);
    final u = resolveBoardHttpBaseUrl(
      wifiTransportActive: session.transport == BoardTransport.wifi,
      sessionWifiBaseUrl: session.wifiBaseUrl,
      prefsLastBoardBaseUrl: prefs.lastBoardBaseUrl,
    );
    if (u == null || u.isEmpty) return;
    try {
      final s = await ref.read(boardApiClientProvider).fetchDemoStatus(u);
      if (mounted) {
        setState(() {
          _boardStat = s;
          _enabled = s.enabled;
        });
      }
    } catch (_) {}
  }

  Future<void> _run(Future<void> Function(String u) fn) async {
    final session = ref.read(boardSessionNotifierProvider);
    final prefs = ref.read(prefsRepositoryProvider);
    final u = resolveBoardHttpBaseUrl(
      wifiTransportActive: session.transport == BoardTransport.wifi,
      sessionWifiBaseUrl: session.wifiBaseUrl,
      prefsLastBoardBaseUrl: prefs.lastBoardBaseUrl,
    );
    if (u == null || u.isEmpty) return;
    setState(() => _busy = true);
    try {
      await fn(u);
      await _refresh();
    } catch (e) {
      if (mounted) ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text('$e')));
    } finally {
      if (mounted) setState(() => _busy = false);
    }
  }

  @override
  Widget build(BuildContext context) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        if (_boardStat != null)
          Text('Stav na desce: demo ${_boardStat!.enabled ? "zapnuto" : "vypnuto"}',
              style: Theme.of(context).textTheme.bodySmall),
        SwitchListTile(
          title: const Text('Demo zapnuto (konfigurace)'),
          value: _enabled,
          onChanged: _busy ? null : (v) => setState(() => _enabled = v),
        ),
        Text('Rychlost animace: ${_speedMs.round()} ms'),
        Slider(
          min: 200,
          max: 5000,
          divisions: 48,
          value: _speedMs.clamp(200, 5000),
          onChanged: _busy ? null : (v) => setState(() => _speedMs = v),
        ),
        FilledButton.tonal(
          onPressed: _busy
              ? null
              : () => _run((u) => ref.read(boardApiClientProvider).postDemoConfig(
                    u,
                    enabled: _enabled,
                    speedMs: _speedMs.round(),
                  )),
          child: const Text('Odeslat konfiguraci demo'),
        ),
        const SizedBox(height: 8),
        OutlinedButton(
          onPressed: _busy ? null : () => _run((u) => ref.read(boardApiClientProvider).postDemoStart(u)),
          child: const Text('Spustit demo'),
        ),
      ],
    );
  }
}

// ── LED Guidance Level (1-5) widget ──────────────────────────────────────────

class _LedGuidanceSection extends ConsumerStatefulWidget {
  const _LedGuidanceSection({required this.baseUrlGetter});
  final String? Function() baseUrlGetter;

  @override
  ConsumerState<_LedGuidanceSection> createState() => _LedGuidanceSectionState();
}

class _LedGuidanceSectionState extends ConsumerState<_LedGuidanceSection> {
  int _level = 3;
  bool _busy = false;

  @override
  void initState() {
    super.initState();
    // pre-fill from live snapshot
    WidgetsBinding.instance.addPostFrameCallback((_) {
      final snap = ref.read(boardSessionNotifierProvider).snapshot;
      if (snap?.status.ledGuidanceLevel != null) {
        setState(() => _level = snap!.status.ledGuidanceLevel!.clamp(1, 5));
      }
    });
  }

  Future<void> _post() async {
    final session = ref.read(boardSessionNotifierProvider);
    final baseUrl = resolveBoardHttpBaseUrl(
      wifiTransportActive: session.transport == BoardTransport.wifi,
      sessionWifiBaseUrl: session.wifiBaseUrl,
      prefsLastBoardBaseUrl: widget.baseUrlGetter(),
    );
    if (baseUrl == null || baseUrl.isEmpty) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text(
              boardHttpSettingsUserMessage(
                ArgumentError('Invalid argument(s): No host specified in URI'),
                session,
              ),
            ),
          ),
        );
      }
      return;
    }
    setState(() => _busy = true);
    try {
      await ref.read(boardApiClientProvider).postLedGuidanceLevel(baseUrl, _level);
      if (mounted) ScaffoldMessenger.of(context).showSnackBar(const SnackBar(content: Text('LED guidance odeslána')));
    } catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text(boardHttpSettingsUserMessage(e, session))),
        );
      }
    } finally {
      if (mounted) setState(() => _busy = false);
    }
  }

  @override
  Widget build(BuildContext context) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        const Divider(height: 24),
        Text('LED nápověda (úroveň $_level / 5)', style: const TextStyle(fontWeight: FontWeight.bold)),
        Slider(
          min: 1, max: 5, divisions: 4,
          value: _level.toDouble(),
          label: _level.toString(),
          onChanged: (v) => setState(() => _level = v.round()),
        ),
        FilledButton.tonal(
          onPressed: _busy ? null : _post,
          child: const Text('Odeslat LED úroveň'),
        ),
      ],
    );
  }
}

// ── Guided Capture Hints toggle ───────────────────────────────────────────────

class _GuidedCaptureSection extends ConsumerStatefulWidget {
  const _GuidedCaptureSection({required this.baseUrlGetter});
  final String? Function() baseUrlGetter;

  @override
  ConsumerState<_GuidedCaptureSection> createState() => _GuidedCaptureSectionState();
}

class _GuidedCaptureSectionState extends ConsumerState<_GuidedCaptureSection> {
  bool _enabled = false;
  bool _busy = false;

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addPostFrameCallback((_) {
      final snap = ref.read(boardSessionNotifierProvider).snapshot;
      if (snap?.status.guidedCaptureHintsEnabled != null) {
        setState(() => _enabled = snap!.status.guidedCaptureHintsEnabled!);
      }
    });
  }

  Future<void> _post(bool v) async {
    final session = ref.read(boardSessionNotifierProvider);
    final baseUrl = resolveBoardHttpBaseUrl(
      wifiTransportActive: session.transport == BoardTransport.wifi,
      sessionWifiBaseUrl: session.wifiBaseUrl,
      prefsLastBoardBaseUrl: widget.baseUrlGetter(),
    );
    if (baseUrl == null || baseUrl.isEmpty) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text(
              boardHttpSettingsUserMessage(
                ArgumentError('Invalid argument(s): No host specified in URI'),
                session,
              ),
            ),
          ),
        );
      }
      return;
    }
    setState(() { _enabled = v; _busy = true; });
    try {
      await ref.read(boardApiClientProvider).postGuidedCaptureHints(baseUrl, v);
    } catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text(boardHttpSettingsUserMessage(e, session))),
        );
      }
      setState(() => _enabled = !v);
    } finally {
      if (mounted) setState(() => _busy = false);
    }
  }

  @override
  Widget build(BuildContext context) {
    return SwitchListTile(
      title: const Text('Navigovaná braní (Guided Capture Hints)'),
      subtitle: const Text('LED zvýrazňují možné braní oponentovy figurky'),
      value: _enabled,
      onChanged: _busy ? null : _post,
    );
  }
}

