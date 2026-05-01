import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:package_info_plus/package_info_plus.dart';
import 'package:permission_handler/permission_handler.dart';
import 'package:url_launcher/url_launcher.dart';

import '../../app_navigation.dart';
import '../../app_providers.dart';
import '../../core/layout/form_factor.dart';
import '../../core/localization/context_l10n.dart';
import '../../l10n/app_localizations.dart';
import '../../core/models/coach_ai_provider.dart';
import '../../core/utils/board_http_base_url.dart';
import '../connection/board_session_notifier.dart';
import '../connection/board_session_state.dart';
import '../game/state/game_ui_notifier.dart';
import '../help/help_screen.dart';
import '../onboarding/onboarding_screen.dart';
import '../profile/user_profile_screen.dart';
import 'board_device_features_view.dart';
import 'developer_settings_view.dart';
import 'home_assistant_mqtt_screen.dart';
import 'manual_connection_screen.dart';
import 'widgets/board_lamp_studio.dart';
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

  String _coachChainSubtitle(AppLocalizations l10n) {
    if (_coachPriority.isEmpty) {
      return l10n.settingsCoachSubtitleOfflineTips;
    }
    return _coachPriority.map((e) => e.shortLabel).join(' → ');
  }

  /// Jednotná rozbalovací dlaždice jako „Světlo desky“.
  Widget _settingsTile({
    required String title,
    String? subtitle,
    Widget? leading,
    bool initiallyExpanded = false,
    bool maintainState = false,
    List<Widget> children = const [],
  }) {
    final theme = Theme.of(context);
    return Padding(
      padding: const EdgeInsets.only(bottom: 8),
      child: Card(
        clipBehavior: Clip.antiAlias,
        margin: EdgeInsets.zero,
        child: Theme(
          data: theme.copyWith(dividerColor: Colors.transparent),
          child: ExpansionTile(
            maintainState: maintainState,
            tilePadding:
                const EdgeInsets.symmetric(horizontal: 16, vertical: 10),
            childrenPadding: EdgeInsets.zero,
            initiallyExpanded: initiallyExpanded,
            leading: leading,
            title: Text(
              title,
              style: theme.textTheme.titleMedium?.copyWith(
                fontWeight: FontWeight.w600,
              ),
            ),
            subtitle: subtitle == null
                ? null
                : Text(
                    subtitle,
                    maxLines: 2,
                    overflow: TextOverflow.ellipsis,
                    style: theme.textTheme.bodySmall?.copyWith(
                      color: theme.colorScheme.onSurfaceVariant,
                    ),
                  ),
            children: [
              Padding(
                padding: const EdgeInsets.fromLTRB(16, 0, 16, 16),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.stretch,
                  children: children,
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }

  String _linkTierLabel(BoardSessionState s, AppLocalizations l10n) {
    if (s.transport == BoardTransport.none) return l10n.settingsLinkDisconnected;
    if (s.busy) return l10n.settingsLinkConnecting;
    if (s.snapshot == null) return l10n.settingsLinkNoResponseYet;
    return l10n.settingsLinkConnectedLive;
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
        SnackBar(
            content: Text(context.l10n.settingsDeveloperModeUnlockedSnack),
            duration: const Duration(seconds: 2)),
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
    final l10n = context.l10n;

    return Scaffold(
      appBar: AppBar(
        title: GestureDetector(
          onTap: _onAppBarTitleTapForDevUnlock,
          behavior: HitTestBehavior.opaque,
          child: Text(l10n.navSettings),
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
          _settingsTile(
            title: l10n.settingsOverviewTitle,
            subtitle: session.lastError != null
                ? l10n.settingsOverviewSubtitleError
                : l10n.settingsOverviewSubtitleOk,
            leading: const Icon(Icons.dashboard_outlined),
            children: [
                  Text(
                    l10n.settingsOverviewBody,
                    style: Theme.of(context).textTheme.bodyMedium,
                  ),
                  const SizedBox(height: 12),
                  OutlinedButton.icon(
                    onPressed: () => ref
                        .read(mainNavTabIndexProvider.notifier)
                        .state = AppMainTab.game,
                    icon: const Icon(Icons.grid_on),
                    label: Text(l10n.settingsGoToPlayTab),
                  ),
                  if (session.lastError != null) ...[
                    const SizedBox(height: 12),
                    Material(
                      color: cs.errorContainer,
                      borderRadius: BorderRadius.circular(12),
                      child: ListTile(
                        leading: Icon(Icons.warning_amber_rounded,
                            color: cs.onErrorContainer),
                        title: Text(l10n.lastErrorTitle,
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
          _settingsTile(
            title: l10n.settingsConnectionTitle,
            subtitle:
                l10n.settingsConnectionSubtitle(
              _linkTierLabel(session, l10n),
              switch (session.transport) {
                BoardTransport.wifi => l10n.transportWifi,
                BoardTransport.ble => l10n.transportBluetooth,
                BoardTransport.mock => l10n.transportShortDemo,
                BoardTransport.none => l10n.transportShortDash,
              },
            ),
            leading: Icon(Icons.link, color: _linkTierColor(session, cs)),
            children: [
                  Text(
                    l10n.settingsConnectionIntro,
                    style: Theme.of(context).textTheme.bodySmall?.copyWith(
                          color: Theme.of(context).colorScheme.onSurfaceVariant,
                        ),
                  ),
                  const SizedBox(height: 14),
                  FilledButton.icon(
                    onPressed: () => pushBoardDiscoveryRoute(context),
                    icon: const Icon(Icons.bluetooth_searching),
                    label: Text(l10n.discoveryFindBle),
                  ),
                  if (session.transport != BoardTransport.none) ...[
                    const SizedBox(height: 10),
                    OutlinedButton.icon(
                      onPressed: session.busy
                          ? null
                          : () => ref
                              .read(boardSessionNotifierProvider.notifier)
                              .disconnect(),
                      icon: const Icon(Icons.link_off),
                      label: Text(l10n.settingsDisconnect),
                    ),
                  ],
                  Theme(
                    data: Theme.of(context).copyWith(
                      dividerColor: Colors.transparent,
                    ),
                    child: ExpansionTile(
                      tilePadding: EdgeInsets.zero,
                      title: Text(l10n.settingsAdvanced),
                      subtitle: Text(
                        l10n.settingsAdvancedConnectionSubtitle,
                        style: Theme.of(context).textTheme.bodySmall?.copyWith(
                              color: Theme.of(context)
                                  .colorScheme
                                  .onSurfaceVariant,
                            ),
                      ),
                      children: [
                        Padding(
                          padding: const EdgeInsets.only(bottom: 12),
                          child: Column(
                            crossAxisAlignment: CrossAxisAlignment.stretch,
                            children: [
                              if (prefs.lastBleRemoteId != null &&
                                  prefs.lastBleRemoteId!.isNotEmpty)
                                OutlinedButton.icon(
                                  onPressed: session.busy
                                      ? null
                                      : () async {
                                          try {
                                            await ref
                                                .read(
                                                    boardSessionNotifierProvider
                                                        .notifier)
                                                .reconnectSavedBle();
                                            if (context.mounted) {
                                              ScaffoldMessenger.of(context)
                                                  .showSnackBar(
                                                SnackBar(
                                                  content: Text(
                                                    l10n.settingsReconnectingBle,
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
                                  label: Text(
                                      l10n.settingsReconnectSavedBleShort),
                                ),
                              if (prefs.lastBleRemoteId != null &&
                                  prefs.lastBleRemoteId!.isNotEmpty)
                                const SizedBox(height: 10),
                              OutlinedButton.icon(
                                onPressed: () => Navigator.push(
                                  context,
                                  MaterialPageRoute<void>(
                                    builder: (_) =>
                                        const ManualConnectionScreen(),
                                  ),
                                ),
                                icon: const Icon(Icons.edit),
                                label: Text(l10n.settingsManualEntry),
                              ),
                              const Divider(height: 28),
                              Text(
                                l10n.settingsSavedDefaultsTitle,
                                style: Theme.of(context)
                                    .textTheme
                                    .titleSmall
                                    ?.copyWith(fontWeight: FontWeight.w600),
                              ),
                              const SizedBox(height: 4),
                              Text(
                                l10n.settingsSavedDefaultsBody,
                                style: Theme.of(context)
                                    .textTheme
                                    .bodySmall
                                    ?.copyWith(
                                      color: Theme.of(context)
                                          .colorScheme
                                          .onSurfaceVariant,
                                    ),
                              ),
                              const SizedBox(height: 12),
                              DropdownButtonFormField<String>(
                                isExpanded: true,
                                value: _connectionMode,
                                decoration: InputDecoration(
                                  labelText: l10n.settingsConnectionModeLabel,
                                  border: const OutlineInputBorder(),
                                ),
                                items: [
                                  DropdownMenuItem(
                                    value: 'auto',
                                    child: Text(l10n.settingsConnectionModeAutoBleWifi),
                                  ),
                                  DropdownMenuItem(
                                    value: 'wifi_only',
                                    child: Text(l10n.connectionModeWifiOnly),
                                  ),
                                  DropdownMenuItem(
                                    value: 'ble_only',
                                    child: Text(l10n.connectionModeBleOnly),
                                  ),
                                ],
                                onChanged: (v) {
                                  if (v == null) {
                                    return;
                                  }
                                  setState(() => _connectionMode = v);
                                  prefs.setConnectionMode(v);
                                },
                              ),
                              const SizedBox(height: 12),
                              TextField(
                                controller: _url,
                                decoration: InputDecoration(
                                  labelText: l10n.settingsDefaultBoardUrl,
                                  border: const OutlineInputBorder(),
                                ),
                                keyboardType: TextInputType.url,
                              ),
                              const SizedBox(height: 10),
                              FilledButton(
                                onPressed: () async {
                                  final n =
                                      normalizeBoardHttpBaseUrl(_url.text);
                                  if (n == null) {
                                    if (context.mounted) {
                                      ScaffoldMessenger.of(context)
                                          .showSnackBar(
                                        SnackBar(
                                          content: Text(
                                            l10n.settingsInvalidUrlSnack,
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
                                        .showSnackBar(
                                      SnackBar(content: Text(l10n.settingsSavedSnack)),
                                    );
                                  }
                                },
                                child: Text(l10n.settingsSaveUrl),
                              ),
                              SwitchListTile(
                                contentPadding: EdgeInsets.zero,
                                title: Text(l10n.settingsBleOnlyTitle),
                                subtitle: Text(
                                  l10n.settingsBleOnlySubtitle,
                                ),
                                value: prefs.preferBluetoothOnly,
                                onChanged: (v) async {
                                  await prefs.setPreferBluetoothOnly(v);
                                  setState(() {});
                                },
                              ),
                              SwitchListTile(
                                contentPadding: EdgeInsets.zero,
                                title: Text(l10n.settingsWebSocketTitle),
                                subtitle: Text(
                                  l10n.settingsWebSocketSubtitle,
                                ),
                                value: prefs.useWebSocket,
                                onChanged: (v) async {
                                  await prefs.setUseWebSocket(v);
                                  setState(() {});
                                },
                              ),
                            ],
                          ),
                        ),
                      ],
                    ),
                  ),
                ],
          ),
          _settingsTile(
            title: l10n.settingsBoardAppearanceTitle,
            subtitle: l10n.settingsBoardAppearanceSubtitle(
              ui.layoutMode == 'boardOnly'
                  ? l10n.settingsLayoutBoardOnlyShort
                  : l10n.settingsLayoutFullUiShort,
              '${(ui.boardZoomStorage * 100).round()}%',
              ui.boardStyleRaw,
            ),
            leading: const Icon(Icons.grid_on_outlined),
            children: [
                  Text(
                    l10n.settingsPlayTabGamePanel,
                    style: Theme.of(context).textTheme.bodySmall?.copyWith(
                          color: Theme.of(context).colorScheme.onSurfaceVariant,
                        ),
                  ),
                  const SizedBox(height: 14),
                  Text(
                    l10n.settingsLayoutLabel,
                    style: Theme.of(context).textTheme.labelLarge?.copyWith(
                          fontWeight: FontWeight.w600,
                        ),
                  ),
                  const SizedBox(height: 8),
                  SizedBox(
                    width: double.infinity,
                    child: SegmentedButton<String>(
                      showSelectedIcon: false,
                      segments: [
                        ButtonSegment<String>(
                          value: 'boardOnly',
                          label: Text(l10n.settingsLayoutBoardSegment),
                          tooltip: l10n.settingsLayoutBoardTooltip,
                        ),
                        ButtonSegment<String>(
                          value: 'standard',
                          label: Text(l10n.settingsLayoutFullSegment),
                          tooltip: l10n.settingsLayoutFullTooltip,
                        ),
                      ],
                      selected: {ui.layoutMode},
                      onSelectionChanged: (s) {
                        if (s.isEmpty) {
                          return;
                        }
                        ref
                            .read(gameUiNotifierProvider.notifier)
                            .setLayoutMode(s.first);
                      },
                    ),
                  ),
                  const SizedBox(height: 14),
                  Row(
                    crossAxisAlignment: CrossAxisAlignment.baseline,
                    textBaseline: TextBaseline.alphabetic,
                    children: [
                      Text(
                        l10n.settingsBoardZoom,
                        style: Theme.of(context).textTheme.titleSmall,
                      ),
                      const Spacer(),
                      Text(
                        '${(ui.boardZoomStorage * 100).round()}%',
                        style: Theme.of(context).textTheme.titleSmall?.copyWith(
                              color: Theme.of(context).colorScheme.primary,
                              fontFeatures: const [
                                FontFeature.tabularFigures(),
                              ],
                            ),
                      ),
                    ],
                  ),
                  Slider(
                    min: 0.7,
                    max: 1.5,
                    divisions: 16,
                    value: ui.boardZoomStorage.clamp(0.7, 1.5),
                    onChanged:
                        ref.read(gameUiNotifierProvider.notifier).setBoardZoom,
                  ),
                  const SizedBox(height: 4),
                  Text(
                    l10n.settingsSquareColors,
                    style: Theme.of(context).textTheme.labelLarge?.copyWith(
                          fontWeight: FontWeight.w600,
                        ),
                  ),
                  const SizedBox(height: 8),
                  DropdownButtonFormField<String>(
                    isExpanded: true,
                    value: ui.boardStyleRaw,
                    decoration: InputDecoration(
                      labelText: l10n.settingsBoardThemeLabel,
                      border: const OutlineInputBorder(),
                    ),
                    items: [
                      DropdownMenuItem(
                          value: 'wooden', child: Text(l10n.boardStyleWooden)),
                      DropdownMenuItem(
                          value: 'modernDark',
                          child: Text(l10n.boardStyleModernDark)),
                      DropdownMenuItem(
                          value: 'iceBlue', child: Text(l10n.boardStyleIceBlue)),
                      DropdownMenuItem(
                          value: 'forestGreen',
                          child: Text(l10n.boardStyleForestGreen)),
                      DropdownMenuItem(
                          value: 'marbleGray',
                          child: Text(l10n.boardStyleMarbleGray)),
                      DropdownMenuItem(
                          value: 'midnight', child: Text(l10n.boardStyleMidnight)),
                      DropdownMenuItem(
                          value: 'slate', child: Text(l10n.boardStyleSlate)),
                      DropdownMenuItem(
                          value: 'coral', child: Text(l10n.boardStyleCoral)),
                    ],
                    onChanged: (v) {
                      if (v != null) {
                        ref
                            .read(gameUiNotifierProvider.notifier)
                            .setBoardStyle(v);
                      }
                    },
                  ),
                  const SizedBox(height: 12),
                  Text(
                    l10n.settingsBoardOptions,
                    style: Theme.of(context).textTheme.labelLarge?.copyWith(
                          fontWeight: FontWeight.w600,
                        ),
                  ),
                  const SizedBox(height: 4),
                  SwitchListTile(
                    contentPadding: EdgeInsets.zero,
                    title: Text(l10n.settingsCoordinatesTitle),
                    subtitle: Text(l10n.settingsCoordinatesSubtitle),
                    value: ui.showCoordinates,
                    onChanged: (_) => ref
                        .read(gameUiNotifierProvider.notifier)
                        .toggleCoordinates(),
                  ),
                  SwitchListTile(
                    contentPadding: EdgeInsets.zero,
                    title: Text(l10n.settingsPieceMotionTitle),
                    subtitle: Text(l10n.settingsPieceMotionSubtitle),
                    value: ui.moveAnimationsEnabled,
                    onChanged: (_) => ref
                        .read(gameUiNotifierProvider.notifier)
                        .toggleMoveAnimations(),
                  ),
                  SwitchListTile(
                    contentPadding: EdgeInsets.zero,
                    title: Text(l10n.settingsFlipBoardTitle),
                    subtitle: Text(l10n.settingsFlipBoardSubtitle),
                    value: ui.boardFlipped,
                    onChanged: (_) => ref
                        .read(gameUiNotifierProvider.notifier)
                        .toggleFlipped(),
                  ),
                  SwitchListTile(
                    contentPadding: EdgeInsets.zero,
                    title: Text(l10n.settingsRemoteMovesTitle),
                    subtitle: Text(l10n.settingsRemoteMovesSubtitle),
                    value: ui.remoteMovesEnabled,
                    onChanged: (_) => ref
                        .read(gameUiNotifierProvider.notifier)
                        .toggleRemoteMoves(),
                  ),
                  SwitchListTile(
                    contentPadding: EdgeInsets.zero,
                    title: Text(l10n.settingsLiveEvalTitle),
                    subtitle: Text(
                      l10n.settingsLiveEvalSubtitle,
                    ),
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
                    child: Text(l10n.settingsResetBoardDisplay),
                  ),
                ],
          ),
          _settingsTile(
            title: l10n.settingsAppAppearanceTitle,
            subtitle: switch (prefs.appearance) {
              'light' => l10n.settingsAppearanceLight,
              'dark' => l10n.settingsAppearanceDark,
              'oled' => l10n.settingsAppearanceOled,
              _ => l10n.settingsAppearanceSystem,
            },
            leading: const Icon(Icons.palette_outlined),
            children: [
                  DropdownButtonFormField<String>(
                    value: prefs.appearance,
                    decoration: InputDecoration(
                        labelText: l10n.settingsColorSchemeLabel,
                        border: const OutlineInputBorder(),
                        helperText: l10n.settingsColorSchemeHelper),
                    items: [
                      DropdownMenuItem(
                          value: 'system', child: Text(l10n.settingsThemeSystem)),
                      DropdownMenuItem(
                          value: 'light', child: Text(l10n.settingsThemeLight)),
                      DropdownMenuItem(
                          value: 'dark', child: Text(l10n.settingsThemeDark)),
                      DropdownMenuItem(
                          value: 'oled',
                          child: Text(l10n.settingsThemeOledBlack)),
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
                    title: Text(l10n.settingsHapticsTitle),
                    value: prefs.hapticsEnabled,
                    onChanged: (v) async {
                      await prefs.setHapticsEnabled(v);
                      setState(() {});
                    },
                  ),
                  SwitchListTile(
                    title: Text(l10n.settingsSoundTitle),
                    value: prefs.soundEffectsEnabled,
                    onChanged: (v) async {
                      await prefs.setSoundEffectsEnabled(v);
                      setState(() {});
                    },
                  ),
                  SwitchListTile(
                    title: Text(l10n.settingsAutoGameSummaryTitle),
                    value: prefs.endgameReportAutoOpen,
                    onChanged: (v) async {
                      await prefs.setEndgameReportAutoOpen(v);
                      setState(() {});
                    },
                  ),
                ],
          ),
          _settingsTile(
            title: l10n.settingsLanguage,
            subtitle: switch (prefs.uiLanguage) {
              'cs' => l10n.languageCzech,
              'en' => l10n.languageEnglish,
              _ => l10n.languageSystem,
            },
            leading: const Icon(Icons.language_outlined),
            children: [
              Text(
                l10n.languageDescription,
                style: Theme.of(context).textTheme.bodySmall?.copyWith(
                      color: Theme.of(context).colorScheme.onSurfaceVariant,
                    ),
              ),
              const SizedBox(height: 8),
              DropdownButtonFormField<String>(
                value: prefs.uiLanguage,
                decoration: InputDecoration(
                  labelText: l10n.settingsLanguage,
                  border: const OutlineInputBorder(),
                ),
                items: [
                  DropdownMenuItem(
                    value: 'system',
                    child: Text(l10n.languageSystem),
                  ),
                  DropdownMenuItem(
                    value: 'en',
                    child: Text(l10n.languageEnglish),
                  ),
                  DropdownMenuItem(
                    value: 'cs',
                    child: Text(l10n.languageCzech),
                  ),
                ],
                onChanged: (v) async {
                  if (v != null) {
                    await prefs.setUiLanguage(v);
                    ref.invalidate(prefsRepositoryProvider);
                    setState(() {});
                  }
                },
              ),
            ],
          ),

          _settingsTile(
            title: l10n.settingsCoachAiTitle,
            subtitle: _coachChainSubtitle(l10n),
            leading: const Icon(Icons.psychology_outlined),
            maintainState: true,
            children: [
                  Text(
                    l10n.settingsCoachPriorityIntro,
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
                        child: Text(l10n.settingsCoachOfflineOnly),
                      ),
                      TextButton(
                        onPressed: () => setState(
                          () => _coachPriority = [CoachAiProviderId.openai],
                        ),
                        child: Text(l10n.settingsCoachOpenAiOnly),
                      ),
                      TextButton(
                        onPressed: () => setState(
                          () => _coachPriority = [CoachAiProviderId.googleGemini],
                        ),
                        child: Text(l10n.settingsCoachGoogleOnly),
                      ),
                      TextButton(
                        onPressed: () => setState(
                          () => _coachPriority = [
                            CoachAiProviderId.groq,
                            CoachAiProviderId.googleGemini,
                            CoachAiProviderId.openai,
                          ],
                        ),
                        child: Text(l10n.settingsCoachGroqGoogleOpenAi),
                      ),
                      TextButton(
                        onPressed: () => setState(
                          () => _coachPriority = [
                            CoachAiProviderId.ollama,
                            CoachAiProviderId.googleGemini,
                          ],
                        ),
                        child: Text(l10n.settingsCoachOllamaGoogle),
                      ),
                    ],
                  ),
                  const SizedBox(height: 8),
                  Text(
                    l10n.settingsCoachPriorityTopLabel,
                    style: Theme.of(context).textTheme.labelLarge,
                  ),
                  const SizedBox(height: 4),
                  if (_coachPriority.isEmpty)
                    Padding(
                      padding: const EdgeInsets.symmetric(vertical: 8),
                      child: Text(
                        l10n.settingsCoachNoCloudProviders,
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
                              tooltip: l10n.settingsCoachRemoveFromChain,
                            ),
                          );
                        }),
                      ],
                    ),
                  const SizedBox(height: 8),
                  Align(
                    alignment: Alignment.centerLeft,
                    child: Text(
                      l10n.settingsCoachAddProvider,
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
                      final coachLevelItems = [
                        DropdownMenuItem(
                          value: 1,
                          child: Text(l10n.settingsCoachLevelBeginner),
                        ),
                        DropdownMenuItem(
                          value: 2,
                          child: Text(l10n.settingsCoachLevelIntermediate),
                        ),
                        DropdownMenuItem(
                          value: 3,
                          child: Text(l10n.settingsCoachLevelAdvanced),
                        ),
                        DropdownMenuItem(
                          value: 4,
                          child: Text(l10n.settingsCoachLevelExpert),
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
                              l10n.settingsCoachExplanationLevel,
                              style: Theme.of(context)
                                  .textTheme
                                  .titleSmall
                                  ?.copyWith(fontWeight: FontWeight.w600),
                            ),
                            const SizedBox(height: 4),
                            Text(
                              l10n.settingsCoachExplanationLevelSubtitle,
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
                              items: coachLevelItems,
                              onChanged: (v) => onLevel(v),
                            ),
                          ],
                        );
                      }
                      return ListTile(
                        contentPadding: EdgeInsets.zero,
                        title: Text(l10n.settingsCoachExplanationLevel),
                        subtitle: Text(l10n.settingsCoachExplanationLevelSubtitle),
                        trailing: DropdownButton<int>(
                          value: prefs.coachUserLevel,
                          items: coachLevelItems,
                          onChanged: (v) => onLevel(v),
                        ),
                      );
                    },
                  ),
                  Text(
                    l10n.settingsCoachCredentialsHeader,
                    style: Theme.of(context).textTheme.titleSmall,
                  ),
                  const SizedBox(height: 8),
                  ExpansionTile(
                    tilePadding: const EdgeInsets.symmetric(
                      horizontal: 16,
                      vertical: 10,
                    ),
                    childrenPadding: EdgeInsets.zero,
                    title: Text(l10n.settingsCoachOpenAiProvider),
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
                            label: Text(l10n.settingsCoachOpenAiKeysButton),
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
                    title: Text(l10n.settingsCoachGoogleAiStudio),
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
                              child: Text(l10n.settingsCoachCustomModel),
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
                            label: Text(l10n.settingsCoachGoogleKeyButton),
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
                    title: Text(l10n.settingsCoachGroqTitle),
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
                            label: Text(l10n.settingsCoachGroqConsoleButton),
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
                    title: Text(l10n.settingsCoachXaiTitle),
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
                            label: Text(l10n.settingsCoachXaiConsoleButton),
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
                    title: Text(l10n.settingsCoachOllamaTitle),
                    initiallyExpanded:
                        _coachPriority.contains(CoachAiProviderId.ollama),
                    children: [
                      _coachExpansionFields([
                        _coachFieldLabel(context, l10n.settingsCoachOpenAiBaseLabel),
                        TextField(
                          controller: _coachOllamaBase,
                          decoration: _coachOutlineDecoration(
                            hintText: 'http://127.0.0.1:11434/v1',
                            helperText: l10n.settingsCoachOllamaUrlHelper,
                          ),
                          keyboardType: TextInputType.url,
                          autocorrect: false,
                        ),
                        _coachFieldLabel(context, l10n.settingsCoachModelNameLabel),
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
                            SnackBar(
                              content: Text(l10n.settingsCoachEnterGeminiModelSnack),
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
                          SnackBar(content: Text(l10n.settingsCoachSavedSnack)),
                        );
                      }
                    },
                    child: Text(l10n.settingsCoachSaveButton),
                  ),
                ],
          ),

          const FirmwareUpdateSection(),

          _settingsTile(
            title: l10n.settingsTileSmartHomeTitle,
            subtitle: l10n.settingsTileSmartHomeSubtitle,
            leading: const Icon(Icons.home_work_outlined),
            children: [
              OutlinedButton.icon(
                onPressed: () => Navigator.push(
                  context,
                  MaterialPageRoute<void>(
                      builder: (_) => const HomeAssistantMqttScreen()),
                ),
                icon: const Icon(Icons.home_work_outlined),
                label: Text(l10n.settingsHaMqttOpenButton),
              ),
            ],
          ),

          _settingsTile(
            title: l10n.settingsTileBoardLightTitle,
            subtitle: l10n.settingsTileBoardLightSubtitle,
            leading: const Icon(Icons.lightbulb_outline),
            children: const [
              BoardLampStudioPanel(),
            ],
          ),

          _settingsTile(
            title: l10n.settingsTileModulesTitle,
            subtitle: l10n.settingsTileModulesSubtitle,
            leading: const Icon(Icons.school_outlined),
            children: [
              _NavButton(
                  label: l10n.settingsNavAppTour,
                  onTap: () => Navigator.push(
                      context,
                      MaterialPageRoute(
                          builder: (_) => OnboardingScreen(
                              onDone: () => Navigator.pop(context))))),
              _NavButton(
                label: l10n.settingsNavChessPuzzles,
                onTap: () => ref.read(mainNavTabIndexProvider.notifier).state =
                    AppMainTab.puzzle,
              ),
              _NavButton(
                label: l10n.settingsNavProfileElo,
                onTap: () => Navigator.push<void>(
                  context,
                  MaterialPageRoute<void>(
                    builder: (_) => const UserProfileScreen(),
                  ),
                ),
              ),
              _NavButton(
                label: l10n.settingsNavProgress,
                onTap: () {
                  ref.read(progressSegmentProvider.notifier).state = 0;
                  ref.read(mainNavTabIndexProvider.notifier).state =
                      AppMainTab.progress;
                },
              ),
          _NavButton(
            label: l10n.settingsNavHelp,
            onTap: () => Navigator.push(
              context,
              MaterialPageRoute(builder: (_) => const HelpScreen()),
            ),
          ),
            ],
          ),

          _settingsTile(
            title: l10n.settingsTileNvsDiagTitle,
            subtitle: l10n.settingsTileNvsDiagSubtitle,
            leading: const Icon(Icons.memory_outlined),
            children: [
              _NavButton(
                label: l10n.settingsNavBoardNvs,
                onTap: () => Navigator.push(
                    context,
                    MaterialPageRoute(
                        builder: (_) => const BoardDeviceFeaturesView())),
              ),
              if (devMode)
                _NavButton(
                  label: l10n.settingsNavDeveloperDiag,
                  color: Colors.brown,
                  onTap: () => Navigator.push(
                      context,
                      MaterialPageRoute(
                          builder: (_) => const DeveloperSettingsView())),
                ),
            ],
          ),
          _settingsTile(
            title: l10n.settingsTileAboutTitle,
            subtitle: l10n.settingsTileAboutSubtitle,
            leading: const Icon(Icons.info_outline),
            children: [
                FutureBuilder<PackageInfo>(
                  future: PackageInfo.fromPlatform(),
                  builder: (context, snap) {
                    final v = snap.data;
                    return ListTile(
                      title: const Text('czechmate'),
                      subtitle: Text(
                        v == null
                            ? l10n.settingsAboutVersionLoading
                            : l10n.settingsAboutVersionLine(v.version, v.buildNumber),
                      ),
                      leading: const Icon(Icons.info_outline),
                    );
                  },
                ),
                ListTile(
                  leading: const Icon(Icons.lock_outline),
                  title: Text(l10n.settingsPrivacyTitle),
                  subtitle: Text(l10n.settingsPrivacyBody),
                ),
                ListTile(
                  leading: const Icon(Icons.cloud_outlined),
                  title: Text(l10n.settingsIcloudTitle),
                  subtitle: Text(l10n.settingsIcloudBody),
                ),
                SwitchListTile(
                  secondary: const Icon(Icons.live_tv_outlined),
                  title: Text(l10n.settingsLiveActivityTitle),
                  subtitle: Text(l10n.settingsLiveActivitySubtitle),
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
                          SnackBar(
                            content: Text(l10n.settingsLiveActivityIosDisabledSnack),
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
                    title: Text(l10n.settingsWearMirrorTitle),
                    subtitle: Text(l10n.settingsWearMirrorSubtitle),
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
                    title: Text(l10n.settingsWatchMirrorTitle),
                    subtitle: Text(l10n.settingsWatchMirrorSubtitle),
                    value: prefs.watchCompanionMirrorEnabled,
                    onChanged: (v) async {
                      await prefs.setWatchCompanionMirrorEnabled(v);
                      setState(() {});
                    },
                  ),
                ],
            ],
          ),

          _settingsTile(
            title: l10n.settingsFactoryTileTitle,
            subtitle: l10n.settingsFactoryTileSubtitle,
            leading: Icon(Icons.warning_amber_rounded, color: cs.error),
            children: [
              OutlinedButton(
                style: OutlinedButton.styleFrom(foregroundColor: Colors.red),
                onPressed: () async {
                  final confirm = await showDialog<bool>(
                      context: context,
                      builder: (ctx) => AlertDialog(
                            title: Text(l10n.settingsFactoryDialogTitle),
                            content: Text(l10n.settingsFactoryDialogBody),
                            actions: [
                              TextButton(
                                  onPressed: () => Navigator.pop(ctx, false),
                                  child: Text(l10n.commonCancel)),
                              TextButton(
                                  onPressed: () => Navigator.pop(ctx, true),
                                  child: Text(l10n.settingsFactoryErase,
                                      style: const TextStyle(color: Colors.red))),
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
                        if (context.mounted) {
                          ScaffoldMessenger.of(context).showSnackBar(
                              SnackBar(content: Text(l10n.settingsFactoryCommandSent)));
                        }
                      } catch (e) {
                        if (context.mounted) {
                          ScaffoldMessenger.of(context).showSnackBar(
                              SnackBar(content: Text(l10n.settingsFactoryError('$e'))));
                        }
                      }
                    }
                  }
                },
                child: Text(l10n.settingsFactoryRunButton),
              ),
            ],
          ),
          const SizedBox(height: 40),
        ],
      ),
      ),
    );
  }
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
