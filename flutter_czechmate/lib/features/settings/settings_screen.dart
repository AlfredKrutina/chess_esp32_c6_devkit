import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:package_info_plus/package_info_plus.dart';
import 'package:permission_handler/permission_handler.dart';
import 'package:url_launcher/url_launcher.dart';

import '../../app_navigation.dart';
import '../../app_providers.dart';
import '../../core/layout/form_factor.dart';
import '../../core/models/coach_ai_provider.dart';
import '../../core/utils/board_http_base_url.dart';
import '../connection/board_session_notifier.dart';
import '../connection/board_session_state.dart';
import '../game/state/game_ui_notifier.dart';
import '../help/help_screen.dart';
import '../onboarding/onboarding_screen.dart';
import 'board_device_features_view.dart';
import 'developer_settings_view.dart';
import 'home_assistant_mqtt_screen.dart';
import 'manual_connection_screen.dart';
import 'widgets/board_lamp_studio.dart';
import 'widgets/board_lamp_widgets.dart';
import 'widgets/firmware_update_section.dart';

class SettingsScreen extends ConsumerStatefulWidget {
  const SettingsScreen({super.key});

  @override
  ConsumerState<SettingsScreen> createState() => _SettingsScreenState();
}

/// Presets for Google AI Studio (`generativelanguage.googleapis.com`). Gemini Flash is usually the smoothest free-tier default; Gemma stays available if enabled for your project.
const _kGemmaPresetModels = [
  'gemini-2.5-flash',
  'gemini-2.5-flash-lite',
  'gemma-2-9b-it',
  'gemma-2-27b-it',
  'gemma-3-27b-it',
];

class _SettingsScreenState extends ConsumerState<SettingsScreen> {
  late final TextEditingController _url;
  late final TextEditingController _coachKey;
  late final TextEditingController _coachGeminiKey;
  late final TextEditingController _coachGemmaModelCustom;
  late final TextEditingController _coachOpenAiModel;
  late final TextEditingController _coachGroqKey;
  late final TextEditingController _coachGroqModel;
  late final TextEditingController _coachXaiKey;
  late final TextEditingController _coachXaiModel;
  late final TextEditingController _coachOllamaBase;
  late final TextEditingController _coachOllamaModel;
  List<CoachAiProviderId> _coachPriority = [];
  /// Either a preset from [_kGemmaPresetModels] or `__custom__`.
  String _gemmaPresetChoice = _kGemmaPresetModels.first;
  String _connectionMode = 'auto';
  int _devTitleTapCount = 0;
  DateTime? _devTitleLastTap;

  @override
  void initState() {
    super.initState();
    final p = ref.read(prefsRepositoryProvider);
    _url = TextEditingController(text: p.lastBoardBaseUrl ?? '');
    _coachKey = TextEditingController(text: p.coachApiKey ?? '');
    _coachGeminiKey = TextEditingController(text: p.coachGeminiApiKey ?? '');
    _coachPriority = List<CoachAiProviderId>.from(p.coachAiPriority);
    _coachOpenAiModel = TextEditingController(text: p.coachOpenAiModel);
    _coachGroqKey = TextEditingController(text: p.coachGroqApiKey ?? '');
    _coachGroqModel = TextEditingController(text: p.coachGroqModel);
    _coachXaiKey = TextEditingController(text: p.coachXaiApiKey ?? '');
    _coachXaiModel = TextEditingController(text: p.coachXaiModel);
    _coachOllamaBase = TextEditingController(text: p.coachOllamaBaseUrl);
    _coachOllamaModel = TextEditingController(text: p.coachOllamaModel);
    final savedModel = p.coachGemmaModelId;
    if (_kGemmaPresetModels.contains(savedModel)) {
      _gemmaPresetChoice = savedModel;
      _coachGemmaModelCustom = TextEditingController(text: savedModel);
    } else {
      _gemmaPresetChoice = '__custom__';
      _coachGemmaModelCustom = TextEditingController(text: savedModel);
    }
    _connectionMode = p.connectionMode;
  }

  @override
  void dispose() {
    _url.dispose();
    _coachKey.dispose();
    _coachGeminiKey.dispose();
    _coachGemmaModelCustom.dispose();
    _coachOpenAiModel.dispose();
    _coachGroqKey.dispose();
    _coachGroqModel.dispose();
    _coachXaiKey.dispose();
    _coachXaiModel.dispose();
    _coachOllamaBase.dispose();
    _coachOllamaModel.dispose();
    super.dispose();
  }

  Future<void> _openCoachDocUrl(Uri uri) async {
    if (await canLaunchUrl(uri)) {
      await launchUrl(uri, mode: LaunchMode.externalApplication);
    }
  }

  String _resolvedGemmaModelId() {
    if (_gemmaPresetChoice != '__custom__') return _gemmaPresetChoice;
    return _coachGemmaModelCustom.text.trim();
  }

  /// Label above coach credential fields (avoids crowded floating labels in [ExpansionTile]).
  Widget _coachFieldLabel(BuildContext context, String text) {
    final theme = Theme.of(context);
    return Padding(
      padding: const EdgeInsets.only(bottom: 8),
      child: Text(
        text,
        style: theme.textTheme.titleSmall?.copyWith(
          fontWeight: FontWeight.w600,
          color: theme.colorScheme.onSurface,
        ),
      ),
    );
  }

  InputDecoration _coachOutlineDecoration({
    String? hintText,
    String? helperText,
  }) {
    return InputDecoration(
      hintText: hintText,
      helperText: helperText,
      border: const OutlineInputBorder(),
      filled: true,
      contentPadding: const EdgeInsets.symmetric(horizontal: 16, vertical: 14),
      helperMaxLines: 4,
    );
  }

  /// Vertical spacing between form controls inside Coach [ExpansionTile]s.
  Widget _coachExpansionFields(List<Widget> fields) {
    return Padding(
      padding: const EdgeInsets.fromLTRB(16, 4, 16, 18),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          for (var i = 0; i < fields.length; i++) ...[
            if (i > 0) const SizedBox(height: 18),
            fields[i],
          ],
        ],
      ),
    );
  }

  String _linkTierLabel(BoardSessionState s) {
    if (s.transport == BoardTransport.none) return 'Disconnected';
    if (s.busy) return 'Connecting…';
    if (s.snapshot == null) return 'No board response yet';
    return 'Connected (live)';
  }

  Color? _linkTierColor(BoardSessionState s, ColorScheme cs) {
    if (s.transport == BoardTransport.none) return cs.error;
    if (s.busy) return cs.tertiary;
    if (s.snapshot == null) return cs.outline;
    return cs.primary;
  }

  void _onAppBarTitleTapForDevUnlock() {
    final now = DateTime.now();
    if (_devTitleLastTap == null ||
        now.difference(_devTitleLastTap!) > const Duration(milliseconds: 600)) {
      _devTitleTapCount = 1;
    } else {
      _devTitleTapCount++;
    }
    _devTitleLastTap = now;
    if (_devTitleTapCount >= 7) {
      _devTitleTapCount = 0;
      ref
          .read(sharedPreferencesProvider)
          .setBool('czechmate.developerModeUnlocked', true);
      if (!mounted) return;
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(
            content: Text('🛠️ Developer mode unlocked.'),
            duration: Duration(seconds: 2)),
      );
    }
  }

  @override
  Widget build(BuildContext context) {
    final prefs = ref.watch(prefsRepositoryProvider);
    final ui = ref.watch(gameUiNotifierProvider);
    final session = ref.watch(boardSessionNotifierProvider);
    final devMode = ref
            .watch(sharedPreferencesProvider)
            .getBool('czechmate.developerModeUnlocked') ??
        false;
    final cs = Theme.of(context).colorScheme;

    return Scaffold(
      appBar: AppBar(
        title: GestureDetector(
          onTap: _onAppBarTitleTapForDevUnlock,
          behavior: HitTestBehavior.opaque,
          child: const Text('Settings'),
        ),
      ),
      body: Theme(
        data: Theme.of(context).copyWith(
          listTileTheme: ListTileThemeData(
            contentPadding:
                const EdgeInsets.symmetric(horizontal: 16, vertical: 10),
            minVerticalPadding: 12,
            shape: Theme.of(context).listTileTheme.shape,
          ),
        ),
        child: ListView(
          padding: EdgeInsets.fromLTRB(
            isDesktopEmbedder() ? 24 : 16,
            20,
            isDesktopEmbedder() ? 24 : 16,
            32,
          ),
          children: [
          Card(
            child: Padding(
              padding: const EdgeInsets.all(16),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  Text('Overview',
                      style: Theme.of(context).textTheme.titleMedium),
                  const SizedBox(height: 8),
                  Text(
                    'Game and board options apply to the Play tab and the Game controls panel — one set of toggles.',
                    style: Theme.of(context).textTheme.bodyMedium,
                  ),
                  const SizedBox(height: 12),
                  OutlinedButton.icon(
                    onPressed: () => ref
                        .read(mainNavTabIndexProvider.notifier)
                        .state = AppMainTab.game,
                    icon: const Icon(Icons.grid_on),
                    label: const Text('Go to Play tab'),
                  ),
                  if (session.lastError != null) ...[
                    const SizedBox(height: 12),
                    Material(
                      color: cs.errorContainer,
                      borderRadius: BorderRadius.circular(12),
                      child: ListTile(
                        leading: Icon(Icons.warning_amber_rounded,
                            color: cs.onErrorContainer),
                        title: Text('Last error',
                            style: TextStyle(
                                color: cs.onErrorContainer,
                                fontWeight: FontWeight.w600)),
                        subtitle: Text('${session.lastError}',
                            style: TextStyle(color: cs.onErrorContainer)),
                      ),
                    ),
                  ],
                ],
              ),
            ),
          ),
          Card(
            child: Padding(
              padding: const EdgeInsets.all(16),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  Text('Board connection',
                      style: Theme.of(context).textTheme.titleMedium),
                  const SizedBox(height: 8),
                  ListTile(
                    title: Text(_linkTierLabel(session)),
                    subtitle: Text(
                      'Type: ${switch (session.transport) {
                        BoardTransport.wifi => 'Wi‑Fi',
                        BoardTransport.ble => 'Bluetooth',
                        BoardTransport.mock => 'Demo (mock)',
                        BoardTransport.none => '—',
                      }}',
                    ),
                    leading:
                        Icon(Icons.link, color: _linkTierColor(session, cs)),
                  ),
                  const SizedBox(height: 8),
                  Column(
                    crossAxisAlignment: CrossAxisAlignment.stretch,
                    children: [
                      OutlinedButton.icon(
                        onPressed: () => pushBoardDiscoveryRoute(context),
                        icon: const Icon(Icons.settings_ethernet),
                        label: const Text('Manage board'),
                      ),
                      const SizedBox(height: 10),
                      if (prefs.lastBleRemoteId != null &&
                          prefs.lastBleRemoteId!.isNotEmpty) ...[
                        OutlinedButton.icon(
                          onPressed: session.busy
                              ? null
                              : () async {
                                  try {
                                    await ref
                                        .read(boardSessionNotifierProvider
                                            .notifier)
                                        .reconnectSavedBle();
                                    if (context.mounted) {
                                      ScaffoldMessenger.of(context)
                                          .showSnackBar(
                                        const SnackBar(
                                          content: Text(
                                            'Reconnecting to the last paired Bluetooth board…',
                                          ),
                                        ),
                                      );
                                    }
                                  } catch (e) {
                                    if (context.mounted) {
                                      ScaffoldMessenger.of(context)
                                          .showSnackBar(
                                        SnackBar(content: Text('$e')),
                                      );
                                    }
                                  }
                                },
                          icon: const Icon(Icons.bluetooth),
                          label: const Text('Reconnect last BLE board'),
                        ),
                        const SizedBox(height: 10),
                      ],
                      OutlinedButton.icon(
                        onPressed: session.busy
                            ? null
                            : () => ref
                                .read(boardSessionNotifierProvider.notifier)
                                .disconnect(),
                        icon: const Icon(Icons.link_off),
                        label: const Text('Disconnect session'),
                      ),
                      const SizedBox(height: 10),
                      OutlinedButton.icon(
                        onPressed: () => Navigator.push(
                          context,
                          MaterialPageRoute<void>(
                            builder: (_) => const ManualConnectionScreen(),
                          ),
                        ),
                        icon: const Icon(Icons.edit),
                        label: const Text('Manual connection'),
                      ),
                    ],
                  ),
                  const SizedBox(height: 8),
                  const BoardLampQuickStrip(),
                ],
              ),
            ),
          ),
          Card(
            child: Padding(
              padding: const EdgeInsets.all(16),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  Text('Board appearance',
                      style: Theme.of(context).textTheme.titleMedium),
                  const SizedBox(height: 8),
                  SingleChildScrollView(
                    scrollDirection: Axis.horizontal,
                    child: SegmentedButton<String>(
                      segments: const [
                        ButtonSegment(
                            value: 'boardOnly', label: Text('Board only')),
                        ButtonSegment(
                            value: 'standard', label: Text('Standard')),
                      ],
                      selected: {ui.layoutMode},
                      onSelectionChanged: (s) {
                        if (s.isEmpty) return;
                        ref
                            .read(gameUiNotifierProvider.notifier)
                            .setLayoutMode(s.first);
                      },
                    ),
                  ),
                  const SizedBox(height: 8),
                  Text(
                      'Board scale ${(ui.boardZoomStorage * 100).round()}%'),
                  Slider(
                    min: 0.7,
                    max: 1.5,
                    divisions: 16,
                    value: ui.boardZoomStorage.clamp(0.7, 1.5),
                    onChanged:
                        ref.read(gameUiNotifierProvider.notifier).setBoardZoom,
                  ),
                  DropdownButtonFormField<String>(
                    value: ui.boardStyleRaw,
                    decoration: const InputDecoration(
                        labelText: 'Board colors',
                        border: OutlineInputBorder()),
                    items: const [
                      DropdownMenuItem(
                          value: 'wooden', child: Text('Wooden (default)')),
                      DropdownMenuItem(
                          value: 'modernDark', child: Text('Modern Dark')),
                      DropdownMenuItem(
                          value: 'iceBlue', child: Text('Ice Blue')),
                      DropdownMenuItem(
                          value: 'forestGreen', child: Text('Forest Green')),
                      DropdownMenuItem(
                          value: 'marbleGray', child: Text('Marble Gray')),
                      DropdownMenuItem(
                          value: 'midnight', child: Text('Midnight')),
                      DropdownMenuItem(value: 'slate', child: Text('Slate')),
                      DropdownMenuItem(value: 'coral', child: Text('Coral')),
                    ],
                    onChanged: (v) {
                      if (v != null)
                        ref
                            .read(gameUiNotifierProvider.notifier)
                            .setBoardStyle(v);
                    },
                  ),
                  SwitchListTile(
                    title: const Text('Show coordinates (a–h, 1–8)'),
                    value: ui.showCoordinates,
                    onChanged: (_) => ref
                        .read(gameUiNotifierProvider.notifier)
                        .toggleCoordinates(),
                  ),
                  SwitchListTile(
                    title: const Text('Move animations'),
                    value: ui.moveAnimationsEnabled,
                    onChanged: (_) => ref
                        .read(gameUiNotifierProvider.notifier)
                        .toggleMoveAnimations(),
                  ),
                  SwitchListTile(
                    title: const Text('Flip perspective (Black at bottom)'),
                    value: ui.boardFlipped,
                    onChanged: (_) => ref
                        .read(gameUiNotifierProvider.notifier)
                        .toggleFlipped(),
                  ),
                  SwitchListTile(
                    title: const Text('Allow moves from app (remote)'),
                    value: ui.remoteMovesEnabled,
                    onChanged: (_) => ref
                        .read(gameUiNotifierProvider.notifier)
                        .toggleRemoteMoves(),
                  ),
                  SwitchListTile(
                    title:
                        const Text('Enable move evaluation & chart (Stockfish)'),
                    subtitle: const Text(
                        'When on, the Analysis tab fills the eval chart during play.'),
                    value: ui.moveEvaluationEnabled,
                    onChanged: (v) => ref
                        .read(gameUiNotifierProvider.notifier)
                        .setMoveEvaluationEnabled(v),
                  ),
                  OutlinedButton(
                    onPressed: () async {
                      await ref
                          .read(gameUiNotifierProvider.notifier)
                          .resetBoardDisplayDefaults();
                      if (context.mounted) setState(() {});
                    },
                    child: const Text('Reset board display defaults'),
                  ),
                ],
              ),
            ),
          ),
          const SizedBox(height: 6),
          Card(
            child: Padding(
              padding: const EdgeInsets.all(18),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  Text('App appearance',
                      style: Theme.of(context).textTheme.titleMedium),
                  const SizedBox(height: 10),
                  DropdownButtonFormField<String>(
                    value: prefs.appearance,
                    decoration: const InputDecoration(
                        labelText: 'Color scheme',
                        border: OutlineInputBorder(),
                        helperText:
                            'Light / Dark apply immediately. System follows macOS appearance.'),
                    items: const [
                      DropdownMenuItem(
                          value: 'system', child: Text('System')),
                      DropdownMenuItem(value: 'light', child: Text('Light')),
                      DropdownMenuItem(value: 'dark', child: Text('Dark')),
                      DropdownMenuItem(
                          value: 'oled', child: Text('OLED (true black)')),
                    ],
                    onChanged: (v) async {
                      if (v != null) {
                        await prefs.setAppearance(v);
                        ref.invalidate(prefsRepositoryProvider);
                        setState(() {});
                      }
                    },
                  ),
                  const SizedBox(height: 8),
                  SwitchListTile(
                    title: const Text('Haptic feedback'),
                    value: prefs.hapticsEnabled,
                    onChanged: (v) async {
                      await prefs.setHapticsEnabled(v);
                      setState(() {});
                    },
                  ),
                  SwitchListTile(
                    title: const Text('Sound effects'),
                    value: prefs.soundEffectsEnabled,
                    onChanged: (v) async {
                      await prefs.setSoundEffectsEnabled(v);
                      setState(() {});
                    },
                  ),
                  SwitchListTile(
                    title: const Text('Auto-open game summary after game'),
                    value: prefs.endgameReportAutoOpen,
                    onChanged: (v) async {
                      await prefs.setEndgameReportAutoOpen(v);
                      setState(() {});
                    },
                  ),
                ],
              ),
            ),
          ),

          Card(
            child: Padding(
              padding: const EdgeInsets.all(18),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  Text('Coach & AI',
                      style: Theme.of(context).textTheme.titleMedium),
                  const SizedBox(height: 8),
                  Text(
                    'Drag to set fallback order: the app tries the first provider, then the next if it fails. '
                    'Leave the list empty for offline tips only. Keys stay on this device.',
                    style: Theme.of(context).textTheme.bodySmall?.copyWith(
                          color: Theme.of(context).colorScheme.onSurfaceVariant,
                        ),
                  ),
                  const SizedBox(height: 8),
                  Wrap(
                    spacing: 10,
                    runSpacing: 10,
                    children: [
                      TextButton(
                        onPressed: () => setState(() => _coachPriority = []),
                        child: const Text('Offline only'),
                      ),
                      TextButton(
                        onPressed: () => setState(
                          () => _coachPriority = [CoachAiProviderId.openai],
                        ),
                        child: const Text('OpenAI only'),
                      ),
                      TextButton(
                        onPressed: () => setState(
                          () => _coachPriority = [CoachAiProviderId.googleGemini],
                        ),
                        child: const Text('Google only'),
                      ),
                      TextButton(
                        onPressed: () => setState(
                          () => _coachPriority = [
                            CoachAiProviderId.groq,
                            CoachAiProviderId.googleGemini,
                            CoachAiProviderId.openai,
                          ],
                        ),
                        child: const Text('Groq→Google→OpenAI'),
                      ),
                      TextButton(
                        onPressed: () => setState(
                          () => _coachPriority = [
                            CoachAiProviderId.ollama,
                            CoachAiProviderId.googleGemini,
                          ],
                        ),
                        child: const Text('Ollama→Google'),
                      ),
                    ],
                  ),
                  const SizedBox(height: 8),
                  Text(
                    'Priority (top = first try)',
                    style: Theme.of(context).textTheme.labelLarge,
                  ),
                  const SizedBox(height: 4),
                  if (_coachPriority.isEmpty)
                    Padding(
                      padding: const EdgeInsets.symmetric(vertical: 8),
                      child: Text(
                        'No cloud providers — Coach uses short on-device tips.',
                        style: Theme.of(context).textTheme.bodySmall?.copyWith(
                              color: Theme.of(context).colorScheme.onSurfaceVariant,
                            ),
                      ),
                    )
                  else
                    ReorderableListView(
                      shrinkWrap: true,
                      physics: const NeverScrollableScrollPhysics(),
                      onReorder: (oldIndex, newIndex) {
                        setState(() {
                          if (newIndex > oldIndex) newIndex -= 1;
                          final id = _coachPriority.removeAt(oldIndex);
                          _coachPriority.insert(newIndex, id);
                        });
                      },
                      children: [
                        ..._coachPriority.asMap().entries.map((entry) {
                          final index = entry.key;
                          final id = entry.value;
                          return ListTile(
                            key: ValueKey('${id.storageValue}_$index'),
                            contentPadding: const EdgeInsets.symmetric(
                              horizontal: 4,
                              vertical: 10,
                            ),
                            leading: const Icon(Icons.drag_handle),
                            title: Text(id.shortLabel),
                            trailing: IconButton(
                              icon: const Icon(Icons.close, size: 20),
                              onPressed: () => setState(
                                () => _coachPriority.removeAt(index),
                              ),
                              tooltip: 'Remove from chain',
                            ),
                          );
                        }),
                      ],
                    ),
                  const SizedBox(height: 8),
                  Align(
                    alignment: Alignment.centerLeft,
                    child: Text(
                      'Add provider',
                      style: Theme.of(context).textTheme.labelLarge,
                    ),
                  ),
                  const SizedBox(height: 4),
                  Wrap(
                    spacing: 10,
                    runSpacing: 10,
                    children: [
                      for (final id in CoachAiProviderId.values)
                        if (!_coachPriority.contains(id))
                          ActionChip(
                            padding: const EdgeInsets.symmetric(
                              horizontal: 12,
                              vertical: 10,
                            ),
                            label: Text('+ ${id.shortLabel}'),
                            onPressed: () =>
                                setState(() => _coachPriority.add(id)),
                          ),
                    ],
                  ),
                  const SizedBox(height: 12),
                  LayoutBuilder(
                    builder: (context, c) {
                      const items = [
                        DropdownMenuItem(
                          value: 1,
                          child: Text('1 – Beginner'),
                        ),
                        DropdownMenuItem(
                          value: 2,
                          child: Text('2 – Intermediate'),
                        ),
                        DropdownMenuItem(
                          value: 3,
                          child: Text('3 – Advanced'),
                        ),
                        DropdownMenuItem(
                          value: 4,
                          child: Text('4 – Expert'),
                        ),
                      ];
                      Future<void> onLevel(int? v) async {
                        if (v != null) await prefs.setCoachUserLevel(v);
                        setState(() {});
                      }

                      if (c.maxWidth < 520) {
                        return Column(
                          crossAxisAlignment: CrossAxisAlignment.stretch,
                          children: [
                            Text(
                              'Coach explanation level',
                              style: Theme.of(context)
                                  .textTheme
                                  .titleSmall
                                  ?.copyWith(fontWeight: FontWeight.w600),
                            ),
                            const SizedBox(height: 4),
                            Text(
                              '1 = Beginner, 4 = Expert',
                              style: Theme.of(context)
                                  .textTheme
                                  .bodySmall
                                  ?.copyWith(
                                    color: Theme.of(context)
                                        .colorScheme
                                        .onSurfaceVariant,
                                  ),
                            ),
                            const SizedBox(height: 10),
                            DropdownButtonFormField<int>(
                              value: prefs.coachUserLevel,
                              decoration: _coachOutlineDecoration(),
                              isExpanded: true,
                              items: items,
                              onChanged: (v) => onLevel(v),
                            ),
                          ],
                        );
                      }
                      return ListTile(
                        contentPadding: EdgeInsets.zero,
                        title: const Text('Coach explanation level'),
                        subtitle: const Text('1 = Beginner, 4 = Expert'),
                        trailing: DropdownButton<int>(
                          value: prefs.coachUserLevel,
                          items: items,
                          onChanged: (v) => onLevel(v),
                        ),
                      );
                    },
                  ),
                  Text(
                    'Credentials (fill what you use)',
                    style: Theme.of(context).textTheme.titleSmall,
                  ),
                  const SizedBox(height: 8),
                  ExpansionTile(
                    tilePadding: const EdgeInsets.symmetric(
                      horizontal: 16,
                      vertical: 10,
                    ),
                    childrenPadding: EdgeInsets.zero,
                    title: const Text('OpenAI'),
                    initiallyExpanded:
                        _coachPriority.contains(CoachAiProviderId.openai),
                    children: [
                      _coachExpansionFields([
                        _coachFieldLabel(context, 'API key'),
                        TextField(
                          controller: _coachKey,
                          decoration: _coachOutlineDecoration(
                            hintText: 'sk-…',
                            helperText:
                                'platform.openai.com — used only for Coach.',
                          ),
                          obscureText: true,
                          autocorrect: false,
                          enableSuggestions: false,
                        ),
                        _coachFieldLabel(context, 'Model id'),
                        TextField(
                          controller: _coachOpenAiModel,
                          decoration: _coachOutlineDecoration(
                            hintText: 'gpt-4o-mini',
                          ),
                        ),
                        Align(
                          alignment: Alignment.centerLeft,
                          child: TextButton.icon(
                            icon: const Icon(Icons.open_in_new, size: 18),
                            label: const Text('OpenAI keys'),
                            onPressed: () => _openCoachDocUrl(
                              Uri.parse('https://platform.openai.com/api-keys'),
                            ),
                          ),
                        ),
                      ]),
                    ],
                  ),
                  ExpansionTile(
                    tilePadding: const EdgeInsets.symmetric(
                      horizontal: 16,
                      vertical: 10,
                    ),
                    childrenPadding: EdgeInsets.zero,
                    title: const Text('Google AI Studio'),
                    initiallyExpanded: _coachPriority
                        .contains(CoachAiProviderId.googleGemini),
                    children: [
                      _coachExpansionFields([
                        _coachFieldLabel(context, 'API key'),
                        TextField(
                          controller: _coachGeminiKey,
                          decoration: _coachOutlineDecoration(
                            hintText: 'Paste Google AI Studio key',
                            helperText:
                                'Get a key at aistudio.google.com — Gemini / Gemma.',
                          ),
                          obscureText: true,
                          autocorrect: false,
                          enableSuggestions: false,
                        ),
                        _coachFieldLabel(context, 'Model id'),
                        DropdownButtonFormField<String>(
                          isExpanded: true,
                          value: _gemmaPresetChoice == '__custom__'
                              ? '__custom__'
                              : (_kGemmaPresetModels.contains(_gemmaPresetChoice)
                                  ? _gemmaPresetChoice
                                  : '__custom__'),
                          decoration: _coachOutlineDecoration(),
                          items: [
                            ..._kGemmaPresetModels.map(
                              (id) =>
                                  DropdownMenuItem(value: id, child: Text(id)),
                            ),
                            const DropdownMenuItem(
                              value: '__custom__',
                              child: Text('Custom…'),
                            ),
                          ],
                          onChanged: (v) {
                            if (v == null) return;
                            setState(() {
                              _gemmaPresetChoice = v;
                              if (v != '__custom__') {
                                _coachGemmaModelCustom.text = v;
                              }
                            });
                          },
                        ),
                        if (_gemmaPresetChoice == '__custom__') ...[
                          _coachFieldLabel(context, 'Custom model id'),
                          TextField(
                            controller: _coachGemmaModelCustom,
                            decoration: _coachOutlineDecoration(
                              hintText: 'e.g. gemini-2.5-flash',
                            ),
                          ),
                        ],
                        Align(
                          alignment: Alignment.centerLeft,
                          child: TextButton.icon(
                            icon: const Icon(Icons.open_in_new, size: 18),
                            label: const Text('Get Google AI key'),
                            onPressed: () => _openCoachDocUrl(
                              Uri.parse(
                                  'https://aistudio.google.com/app/apikey'),
                            ),
                          ),
                        ),
                      ]),
                    ],
                  ),
                  ExpansionTile(
                    tilePadding: const EdgeInsets.symmetric(
                      horizontal: 16,
                      vertical: 10,
                    ),
                    childrenPadding: EdgeInsets.zero,
                    title: const Text('Groq (OpenAI-compatible)'),
                    initiallyExpanded:
                        _coachPriority.contains(CoachAiProviderId.groq),
                    children: [
                      _coachExpansionFields([
                        _coachFieldLabel(context, 'API key'),
                        TextField(
                          controller: _coachGroqKey,
                          decoration: _coachOutlineDecoration(
                            hintText: 'Paste Groq API key',
                          ),
                          obscureText: true,
                          autocorrect: false,
                          enableSuggestions: false,
                        ),
                        _coachFieldLabel(context, 'Model id'),
                        TextField(
                          controller: _coachGroqModel,
                          decoration: _coachOutlineDecoration(
                            hintText: 'llama-3.3-70b-versatile',
                            helperText: 'Must match an enabled Groq model.',
                          ),
                        ),
                        Align(
                          alignment: Alignment.centerLeft,
                          child: TextButton.icon(
                            icon: const Icon(Icons.open_in_new, size: 18),
                            label: const Text('Groq console'),
                            onPressed: () => _openCoachDocUrl(
                              Uri.parse('https://console.groq.com/keys'),
                            ),
                          ),
                        ),
                      ]),
                    ],
                  ),
                  ExpansionTile(
                    tilePadding: const EdgeInsets.symmetric(
                      horizontal: 16,
                      vertical: 10,
                    ),
                    childrenPadding: EdgeInsets.zero,
                    title: const Text('xAI Grok'),
                    initiallyExpanded:
                        _coachPriority.contains(CoachAiProviderId.xaiGrok),
                    children: [
                      _coachExpansionFields([
                        _coachFieldLabel(context, 'API key'),
                        TextField(
                          controller: _coachXaiKey,
                          decoration: _coachOutlineDecoration(
                            hintText: 'Paste xAI API key',
                          ),
                          obscureText: true,
                          autocorrect: false,
                          enableSuggestions: false,
                        ),
                        _coachFieldLabel(context, 'Model id'),
                        TextField(
                          controller: _coachXaiModel,
                          decoration: _coachOutlineDecoration(
                            hintText: 'grok-2-latest',
                          ),
                        ),
                        Align(
                          alignment: Alignment.centerLeft,
                          child: TextButton.icon(
                            icon: const Icon(Icons.open_in_new, size: 18),
                            label: const Text('xAI console'),
                            onPressed: () => _openCoachDocUrl(
                              Uri.parse('https://console.x.ai/'),
                            ),
                          ),
                        ),
                      ]),
                    ],
                  ),
                  ExpansionTile(
                    tilePadding: const EdgeInsets.symmetric(
                      horizontal: 16,
                      vertical: 10,
                    ),
                    childrenPadding: EdgeInsets.zero,
                    title: const Text('Ollama (local)'),
                    initiallyExpanded:
                        _coachPriority.contains(CoachAiProviderId.ollama),
                    children: [
                      _coachExpansionFields([
                        _coachFieldLabel(context, 'OpenAI API base'),
                        TextField(
                          controller: _coachOllamaBase,
                          decoration: _coachOutlineDecoration(
                            hintText: 'http://127.0.0.1:11434/v1',
                            helperText:
                                'Zadej URL končící na /v1 (např. :11434/v1). Jen :11434 bez /v1 dřív nefungovalo — u portu 11434 to teď appka doplní.',
                          ),
                          keyboardType: TextInputType.url,
                          autocorrect: false,
                        ),
                        _coachFieldLabel(context, 'Model name'),
                        TextField(
                          controller: _coachOllamaModel,
                          decoration: _coachOutlineDecoration(
                            hintText: 'llama3.2',
                          ),
                          autocorrect: false,
                        ),
                      ]),
                    ],
                  ),
                  const SizedBox(height: 12),
                  FilledButton.tonal(
                    onPressed: () async {
                      await prefs.setCoachAiPriority(_coachPriority);
                      await prefs.syncCoachLlmBackendFromPriority();
                      await prefs.setCoachApiKey(_coachKey.text.trim());
                      await prefs.setCoachGeminiApiKey(_coachGeminiKey.text.trim());
                      await prefs.setCoachOpenAiModel(_coachOpenAiModel.text.trim());
                      await prefs.setCoachGroqApiKey(_coachGroqKey.text.trim());
                      await prefs.setCoachGroqModel(_coachGroqModel.text.trim());
                      await prefs.setCoachXaiApiKey(_coachXaiKey.text.trim());
                      await prefs.setCoachXaiModel(_coachXaiModel.text.trim());
                      await prefs.setCoachOllamaBaseUrl(_coachOllamaBase.text.trim());
                      await prefs.setCoachOllamaModel(_coachOllamaModel.text.trim());
                      final mid = _resolvedGemmaModelId();
                      if (_coachPriority.contains(CoachAiProviderId.googleGemini) &&
                          mid.isEmpty) {
                        if (context.mounted) {
                          ScaffoldMessenger.of(context).showSnackBar(
                            const SnackBar(
                              content: Text(
                                'Enter a Google AI model id (or pick a preset).',
                              ),
                            ),
                          );
                        }
                        return;
                      }
                      if (mid.isNotEmpty) {
                        await prefs.setCoachGemmaModelId(mid);
                      }
                      if (context.mounted) {
                        ScaffoldMessenger.of(context).showSnackBar(
                          const SnackBar(content: Text('Coach settings saved')),
                        );
                      }
                    },
                    child: const Text('Save coach settings'),
                  ),
                ],
              ),
            ),
          ),

          // ── BOARD CONNECTION ───────────────────────────────────────
          _SectionHeader('Board connection'),
          DropdownButtonFormField<String>(
            value: _connectionMode,
            decoration: const InputDecoration(
                labelText: 'Connection mode', border: OutlineInputBorder()),
            items: const [
              DropdownMenuItem(
                  value: 'auto', child: Text('Auto (BLE → Wi‑Fi)')),
              DropdownMenuItem(value: 'wifi_only', child: Text('Wi‑Fi only')),
              DropdownMenuItem(value: 'ble_only', child: Text('Bluetooth only')),
            ],
            onChanged: (v) {
              if (v == null) return;
              setState(() => _connectionMode = v);
              prefs.setConnectionMode(v);
            },
          ),
          const SizedBox(height: 8),
          TextField(
            controller: _url,
            decoration: const InputDecoration(
              labelText: 'Default board URL',
              border: OutlineInputBorder(),
            ),
            keyboardType: TextInputType.url,
          ),
          const SizedBox(height: 8),
          FilledButton(
            onPressed: () async {
              final n = normalizeBoardHttpBaseUrl(_url.text);
              if (n == null) {
                if (context.mounted) {
                  ScaffoldMessenger.of(context).showSnackBar(
                    const SnackBar(
                      content: Text(
                        'Invalid URL — include a hostname (e.g. 192.168.4.1 or http://…)',
                      ),
                    ),
                  );
                }
                return;
              }
              await prefs.setLastBoardBaseUrl(n);
              _url.text = n;
              if (context.mounted) {
                ScaffoldMessenger.of(context)
                    .showSnackBar(const SnackBar(content: Text('Saved')));
              }
            },
            child: const Text('Save URL'),
          ),
          SwitchListTile(
            title: const Text('Bluetooth only (do not switch to Wi‑Fi)'),
            value: prefs.preferBluetoothOnly,
            onChanged: (v) async {
              await prefs.setPreferBluetoothOnly(v);
              setState(() {});
            },
          ),
          SwitchListTile(
            title: const Text('WebSocket snapshot + REST watchdog'),
            subtitle: const Text(
                'After changing this, start a Wi‑Fi session again from Board discovery or Manual connection.'),
            value: prefs.useWebSocket,
            onChanged: (v) async {
              await prefs.setUseWebSocket(v);
              setState(() {});
            },
          ),

          const FirmwareUpdateSection(),

          Card(
            child: ExpansionTile(
              tilePadding: const EdgeInsets.symmetric(horizontal: 16),
              childrenPadding: const EdgeInsets.fromLTRB(16, 0, 16, 16),
              title: const Text('Smart home (MQTT)'),
              subtitle: const Text('Advanced Home Assistant integration'),
              children: [
                OutlinedButton.icon(
                  onPressed: () => Navigator.push(
                    context,
                    MaterialPageRoute<void>(
                        builder: (_) => const HomeAssistantMqttScreen()),
                  ),
                  icon: const Icon(Icons.home_work_outlined),
                  label: const Text('Home Assistant & MQTT (guide + form)'),
                ),
              ],
            ),
          ),

          Card(
            child: ExpansionTile(
              tilePadding: const EdgeInsets.symmetric(horizontal: 16),
              childrenPadding: const EdgeInsets.fromLTRB(16, 0, 16, 16),
              title: const Text('Board lamp'),
              subtitle: const Text('Advanced LED lamp behavior'),
              children: const [
                BoardLampStudioPanel(),
              ],
            ),
          ),

          // ── APP MODULES ───────────────────────────────────────────
          _SectionHeader('Modules & learning'),
          _NavButton(
              label: 'App tour (onboarding)',
              onTap: () => Navigator.push(
                  context,
                  MaterialPageRoute(
                      builder: (_) => OnboardingScreen(
                          onDone: () => Navigator.pop(context))))),
          _NavButton(
            label: 'Chess puzzles',
            onTap: () => ref.read(mainNavTabIndexProvider.notifier).state =
                AppMainTab.puzzle,
          ),
          _NavButton(
            label: 'Progress (learning & stats)',
            onTap: () {
              ref.read(progressSegmentProvider.notifier).state = 0;
              ref.read(mainNavTabIndexProvider.notifier).state =
                  AppMainTab.progress;
            },
          ),
          _NavButton(
              label: 'Help',
              onTap: () => Navigator.push(context,
                  MaterialPageRoute(builder: (_) => const HelpScreen()))),

          Card(
            child: ExpansionTile(
              tilePadding: const EdgeInsets.symmetric(horizontal: 16),
              childrenPadding: const EdgeInsets.fromLTRB(16, 0, 16, 16),
              title: const Text('Board memory & diagnostics'),
              subtitle: const Text('Advanced tools and service actions'),
              children: [
                _NavButton(
                  label: 'Board NVS rules (LED, bot)',
                  onTap: () => Navigator.push(
                      context,
                      MaterialPageRoute(
                          builder: (_) => const BoardDeviceFeaturesView())),
                ),
                if (devMode)
                  _NavButton(
                    label: 'Developer diagnostics',
                    color: Colors.brown,
                    onTap: () => Navigator.push(
                        context,
                        MaterialPageRoute(
                            builder: (_) => const DeveloperSettingsView())),
                  ),
              ],
            ),
          ),
          Card(
            child: ExpansionTile(
              tilePadding: const EdgeInsets.symmetric(horizontal: 16),
              childrenPadding: const EdgeInsets.fromLTRB(16, 0, 16, 16),
              title: const Text('About'),
              subtitle: const Text('Version, privacy, system info'),
              children: [
                FutureBuilder<PackageInfo>(
                  future: PackageInfo.fromPlatform(),
                  builder: (context, snap) {
                    final v = snap.data;
                    return ListTile(
                      title: const Text('czechmate'),
                      subtitle: Text(
                        v == null
                            ? 'Loading version…'
                            : 'Version ${v.version} (${v.buildNumber}) • Tap “Settings” in the title bar 7× to unlock developer mode.',
                      ),
                      leading: const Icon(Icons.info_outline),
                    );
                  },
                ),
                const ListTile(
                  leading: Icon(Icons.lock_outline),
                  title: Text('Privacy'),
                  subtitle: Text(
                    'This build does not send analytics. Traffic goes only to your board on the local network or over Bluetooth.',
                  ),
                ),
                const ListTile(
                  leading: Icon(Icons.cloud_outlined),
                  title: Text('iCloud'),
                  subtitle: Text(
                    'Optional puzzle sync via iCloud (CloudKit) is planned for a future release; this build does not sync.',
                  ),
                ),
                SwitchListTile(
                  secondary: const Icon(Icons.live_tv_outlined),
                  title: const Text('Stav časomíry mimo aplikaci'),
                  subtitle: const Text(
                    'iPhone: Live Activity (Lock Screen / Dynamic Island, iOS 16.2+). Android: probíhající notifikace (Android 13+ je potřeba povolit oznámení). Zapíná se z herní obrazovky.',
                  ),
                  value: prefs.liveActivityEnabled,
                  onChanged: (v) async {
                    if (v &&
                        !kIsWeb &&
                        defaultTargetPlatform == TargetPlatform.android) {
                      await Permission.notification.request();
                    }
                    await prefs.setLiveActivityEnabled(v);
                    if (v &&
                        !kIsWeb &&
                        defaultTargetPlatform == TargetPlatform.iOS) {
                      final allowed = await ref
                          .read(liveActivityServiceProvider)
                          .iosLiveActivitiesAllowedByUser();
                      if (!allowed && context.mounted) {
                        ScaffoldMessenger.of(context).showSnackBar(
                          const SnackBar(
                            content: Text(
                              'Live Activities jsou v systému vypnuté. Zapněte je v Nastavení → czechmate → Live Activities.',
                            ),
                          ),
                        );
                      }
                    }
                    if (!v) {
                      await ref.read(liveActivityServiceProvider).endAll();
                    }
                    setState(() {});
                  },
                ),
                if (!kIsWeb &&
                    defaultTargetPlatform == TargetPlatform.android)
                  SwitchListTile(
                    secondary: const Icon(Icons.watch_outlined),
                    title: const Text('Zrcadlit časomíru na Wear OS'),
                    subtitle: const Text(
                      'Google Data Layer — stejný stav jako probíhající notifikace; vyžaduje párování hodinek Wear OS.',
                    ),
                    value: prefs.wearDataLayerMirrorEnabled,
                    onChanged: (v) async {
                      if (v) {
                        await Permission.notification.request();
                      }
                      await prefs.setWearDataLayerMirrorEnabled(v);
                      setState(() {});
                    },
                  ),
                if (!kIsWeb &&
                    defaultTargetPlatform == TargetPlatform.iOS) ...[
                  SwitchListTile(
                    secondary: const Icon(Icons.watch_outlined),
                    title: const Text('Zrcadlit časomíru na Apple Watch'),
                    subtitle: const Text(
                      'WatchConnectivity — stejný stav jako Live Activity; na hodinkách je základní UI + Pauza/Pokračovat.',
                    ),
                    value: prefs.watchCompanionMirrorEnabled,
                    onChanged: (v) async {
                      await prefs.setWatchCompanionMirrorEnabled(v);
                      setState(() {});
                    },
                  ),
                ],
              ],
            ),
          ),

          // ── FACTORY RESET ─────────────────────────────────────────
          const Divider(height: 40),
          OutlinedButton(
            style: OutlinedButton.styleFrom(foregroundColor: Colors.red),
            onPressed: () async {
              final confirm = await showDialog<bool>(
                  context: context,
                  builder: (ctx) => AlertDialog(
                        title: const Text('Factory reset'),
                        content: const Text(
                            'Erase all board NVS (Wi‑Fi, passwords, MQTT, preferences)? The device will restart as a Wi‑Fi access point.'),
                        actions: [
                          TextButton(
                              onPressed: () => Navigator.pop(ctx, false),
                              child: const Text('Cancel')),
                          TextButton(
                              onPressed: () => Navigator.pop(ctx, true),
                              child: const Text('Erase',
                                  style: TextStyle(color: Colors.red))),
                        ],
                      ));
              if (confirm == true && context.mounted) {
                final baseUrl =
                    normalizeBoardHttpBaseUrl(prefs.lastBoardBaseUrl);
                if (baseUrl != null) {
                  try {
                    await ref
                        .read(boardApiClientProvider)
                        .postFactoryReset(baseUrl);
                    if (context.mounted)
                      ScaffoldMessenger.of(context).showSnackBar(
                          const SnackBar(content: Text('Command sent')));
                  } catch (e) {
                    if (context.mounted)
                      ScaffoldMessenger.of(context)
                          .showSnackBar(SnackBar(content: Text('Error: $e')));
                  }
                }
              }
            },
            child: const Text('Run board factory reset'),
          ),
          const SizedBox(height: 40),
        ],
      ),
      ),
    );
  }
}

class _SectionHeader extends StatelessWidget {
  const _SectionHeader(this.title);
  final String title;
  @override
  Widget build(BuildContext context) => Padding(
        padding: const EdgeInsets.only(top: 24, bottom: 8),
        child: Text(title,
            style: TextStyle(
                fontWeight: FontWeight.bold,
                color: Theme.of(context).colorScheme.primary)),
      );
}

class _NavButton extends StatelessWidget {
  const _NavButton({required this.label, required this.onTap, this.color});
  final String label;
  final VoidCallback onTap;
  final Color? color;
  @override
  Widget build(BuildContext context) => Padding(
        padding: const EdgeInsets.only(bottom: 8),
        child: OutlinedButton(
          style: color != null
              ? OutlinedButton.styleFrom(foregroundColor: color)
              : null,
          onPressed: onTap,
          child: Align(alignment: Alignment.centerLeft, child: Text(label)),
        ),
      );
}
