import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../../app_providers.dart';
import '../../../core/localization/context_l10n.dart';
import '../../../core/localization/locale_bridge.dart';
import '../../../l10n/app_localizations.dart';
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

  String _botFooterText(AppLocalizations l) {
    final session = ref.watch(boardSessionNotifierProvider);
    final mock = ref.watch(prefsRepositoryProvider).useMockBoard;
    final wifiCmd = session.transport == BoardTransport.wifi && session.wifiBaseUrl != null;
    if (mock) {
      return l.boardNvsFooterMock;
    }
    if (wifiCmd) {
      return l.boardNvsFooterWifiActive;
    }
    if (session.transport == BoardTransport.ble) {
      return l.boardNvsFooterBle;
    }
    return l.boardNvsFooterGeneric;
  }

  String _httpUnavailableExplanation(AppLocalizations l, BoardSessionState session) {
    if (session.transport == BoardTransport.ble && session.bleGattConnected) {
      return l.boardNvsHttpBleExplain;
    }
    if (session.transport == BoardTransport.wifi &&
        (session.wifiBaseUrl == null || session.wifiBaseUrl!.trim().isEmpty)) {
      return l.boardNvsHttpWifiNoUrl;
    }
    return l.boardNvsHttpMissingUrl;
  }

  Future<void> _fetch() async {
    final session = ref.read(boardSessionNotifierProvider);
    final prefs = ref.read(prefsRepositoryProvider);
    final strings = appStringsForPrefs(prefs);
    final baseUrl = resolveBoardHttpBaseUrl(
      wifiTransportActive: session.transport == BoardTransport.wifi,
      sessionWifiBaseUrl: session.wifiBaseUrl,
      prefsLastBoardBaseUrl: prefs.lastBoardBaseUrl,
      bleStaIp: session.bleStaIp,
    );
    if (baseUrl == null || baseUrl.isEmpty) {
      if (mounted) {
        setState(() {
          _emptyStateDetail = _httpUnavailableExplanation(strings, session);
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
        final msg = boardHttpSettingsUserMessage(strings, e, session);
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
    final strings = appStringsForPrefs(prefs);
    final baseUrl = resolveBoardHttpBaseUrl(
      wifiTransportActive: session.transport == BoardTransport.wifi,
      sessionWifiBaseUrl: session.wifiBaseUrl,
      prefsLastBoardBaseUrl: prefs.lastBoardBaseUrl,
      bleStaIp: session.bleStaIp,
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
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text(strings.boardNvsSavedSnack)),
        );
      }
    } catch (e) {
      if (mounted) {
        final msg = boardHttpSettingsUserMessage(strings, e, session);
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
    final l10n = context.l10n;
    final prefsWatch = ref.watch(prefsRepositoryProvider);
    return Scaffold(
      appBar: AppBar(
        title: Text(l10n.boardNvsTitle),
        actions: [
          IconButton(
            icon: const Icon(Icons.refresh),
            onPressed: _isLoading ? null : _fetch,
            tooltip: l10n.boardNvsRefreshTooltip,
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
                      _emptyStateDetail ?? l10n.boardNvsFetchFailedFallback,
                      textAlign: TextAlign.center,
                      style: Theme.of(context).textTheme.bodyLarge,
                    ),
                  ),
                )
              : ListView(
                  padding: const EdgeInsets.all(16),
                  children: [
                    Text(
                      l10n.boardNvsMergeHint(
                        '${prefsWatch.hintDepth}',
                        prefsWatch.moveEvaluationEnabled
                            ? l10n.boardNvsEvalOn
                            : l10n.boardNvsEvalOff,
                      ),
                      style: Theme.of(context).textTheme.bodySmall,
                    ),
                    const SizedBox(height: 12),
                    Text(l10n.boardNvsLedHeader,
                        style: const TextStyle(fontWeight: FontWeight.bold, fontSize: 16)),
                    const SizedBox(height: 8),
                    SwitchListTile(
                      title: Text(l10n.boardNvsEvalMode),
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
                      title: Text(l10n.boardNvsStockfishDepth),
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
                      title: Text(l10n.boardNvsLedBest),
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
                      title: Text(l10n.boardNvsLedGood),
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
                      title: Text(l10n.boardNvsLedCapture),
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
                      title: Text(l10n.boardNvsUartStats),
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
                      title: Text(l10n.boardNvsLiftBeforeBotTarget),
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
                      title: Text(l10n.boardNvsTutorialMode),
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
                      title: Text(l10n.boardNvsConfirmNewGameLed),
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
                      title: Text(l10n.boardNvsHintLimit),
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
                      title: Text(l10n.boardNvsHintTierTitle),
                      subtitle: Text(l10n.boardNvsHintTierSubtitle),
                      trailing: DropdownButton<String>(
                        value: _settings!.prefs.moveHintTier,
                        items: [
                          DropdownMenuItem(value: 'H1', child: Text(l10n.boardNvsH1)),
                          DropdownMenuItem(value: 'H2', child: Text(l10n.boardNvsH2)),
                          DropdownMenuItem(value: 'H3', child: Text(l10n.boardNvsH3)),
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
                        l10n.boardNvsHintTierFootnote,
                        style: Theme.of(context).textTheme.bodySmall,
                      ),
                    ),
                    const SizedBox(height: 8),
                    // LED Guidance level (1-5) — direct POST to /api/settings/led_guidance
                    _LedGuidanceSection(baseUrlGetter: () => ref.read(prefsRepositoryProvider).lastBoardBaseUrl),
                    // Guided capture — direct POST to /api/settings/guided_hints
                    _GuidedCaptureSection(baseUrlGetter: () => ref.read(prefsRepositoryProvider).lastBoardBaseUrl),
                    const Divider(height: 32),
                    Text(l10n.boardNvsOpponentHeader,
                        style: const TextStyle(fontWeight: FontWeight.bold, fontSize: 16)),
                    ListTile(
                      title: Text(l10n.boardNvsBotMode),
                      trailing: DropdownButton<String>(
                        value: _settings!.prefs.botSettings.mode,
                        items: [
                          DropdownMenuItem(value: 'pvp', child: Text(l10n.boardNvsPvp)),
                          DropdownMenuItem(value: 'bot', child: Text(l10n.boardNvsPvb)),
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
                      title: Text(l10n.boardNvsBotStrength),
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
                      title: Text(l10n.boardNvsBotPlaysAs),
                      trailing: DropdownButton<String>(
                        value: _settings!.prefs.botSettings.side,
                        items: [
                          DropdownMenuItem(value: 'white', child: Text(l10n.colorWhite)),
                          DropdownMenuItem(value: 'black', child: Text(l10n.colorBlack)),
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
                        _botFooterText(l10n),
                        style: Theme.of(context).textTheme.bodySmall,
                      ),
                    ),
                    const Divider(height: 32),
                    Text(l10n.boardNvsDemoHeader,
                        style: const TextStyle(fontWeight: FontWeight.bold, fontSize: 16)),
                    const _DemoNvsSection(),
                    const SizedBox(height: 24),
                    FilledButton.icon(
                      onPressed: _isLoading ? null : _save,
                      icon: _isLoading ? const SizedBox(width: 16, height: 16, child: CircularProgressIndicator(strokeWidth: 2, color: Colors.white)) : const Icon(Icons.upload),
                      label: Text(l10n.boardNvsSaveToBoard),
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
      bleStaIp: session.bleStaIp,
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
      bleStaIp: session.bleStaIp,
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
    final l10n = context.l10n;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        if (_boardStat != null)
          Text(
            l10n.boardDemoStateLine(
              _boardStat!.enabled ? l10n.boardDemoOn : l10n.boardDemoOff,
            ),
            style: Theme.of(context).textTheme.bodySmall,
          ),
        SwitchListTile(
          title: Text(l10n.boardDemoEnabledConfig),
          value: _enabled,
          onChanged: _busy ? null : (v) => setState(() => _enabled = v),
        ),
        Text(l10n.boardDemoSpeedLabel('${_speedMs.round()}')),
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
          child: Text(l10n.boardDemoSendConfig),
        ),
        const SizedBox(height: 8),
        OutlinedButton(
          onPressed: _busy ? null : () => _run((u) => ref.read(boardApiClientProvider).postDemoStart(u)),
          child: Text(l10n.boardDemoStart),
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
      bleStaIp: session.bleStaIp,
    );
    if (baseUrl == null || baseUrl.isEmpty) {
      if (mounted) {
        final l10n = appStringsForPrefs(ref.read(prefsRepositoryProvider));
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text(
              boardHttpSettingsUserMessage(
                l10n,
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
      if (mounted) {
        final l10n = context.l10n;
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text(l10n.boardLedSentSnack)),
        );
      }
    } catch (e) {
      if (mounted) {
        final l10n = context.l10n;
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text(boardHttpSettingsUserMessage(l10n, e, session))),
        );
      }
    } finally {
      if (mounted) setState(() => _busy = false);
    }
  }

  @override
  Widget build(BuildContext context) {
    final l10n = context.l10n;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        const Divider(height: 24),
        Text(
          l10n.boardLedGuidanceTitle('$_level'),
          style: const TextStyle(fontWeight: FontWeight.bold),
        ),
        Slider(
          min: 1, max: 5, divisions: 4,
          value: _level.toDouble(),
          label: _level.toString(),
          onChanged: (v) => setState(() => _level = v.round()),
        ),
        FilledButton.tonal(
          onPressed: _busy ? null : _post,
          child: Text(l10n.boardLedSendLevel),
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
      bleStaIp: session.bleStaIp,
    );
    if (baseUrl == null || baseUrl.isEmpty) {
      if (mounted) {
        final l10n = appStringsForPrefs(ref.read(prefsRepositoryProvider));
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text(
              boardHttpSettingsUserMessage(
                l10n,
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
        final l10n = context.l10n;
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text(boardHttpSettingsUserMessage(l10n, e, session))),
        );
      }
      setState(() => _enabled = !v);
    } finally {
      if (mounted) setState(() => _busy = false);
    }
  }

  @override
  Widget build(BuildContext context) {
    final l10n = context.l10n;
    return SwitchListTile(
      title: Text(l10n.boardGuidedCaptureTitle),
      subtitle: Text(l10n.boardGuidedCaptureSubtitle),
      value: _enabled,
      onChanged: _busy ? null : _post,
    );
  }
}

