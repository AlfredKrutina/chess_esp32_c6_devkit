import 'dart:async';

import 'package:connectivity_plus/connectivity_plus.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app_navigation.dart';
import '../../app_providers.dart';
import '../../core/layout/form_factor.dart';
import '../../core/localization/context_l10n.dart';
import '../../core/widgets/glass_snackbar.dart';
import '../../core/widgets/quiet_list.dart';
import '../coach/coach_level_selector.dart';
import '../connection/board_session_notifier.dart';
import '../connection/board_session_state.dart';
import '../game/state/game_ui_notifier.dart';
import '../help/help_screen.dart';
import '../onboarding/onboarding_screen.dart';
import 'app_update_banner.dart';
import 'coach_ai_settings_screen.dart';
import 'firmware_settings_screen.dart';
import 'firmware_update_availability.dart';
import 'home_assistant_mqtt_screen.dart';
import 'settings_about_page.dart';
import 'settings_board_appearance_page.dart';
import 'settings_board_light_page.dart';
import 'settings_connection_page.dart';
import 'settings_diagnostics_page.dart';
import 'settings_factory_page.dart';

/// Four groups: Board, App, Coach and AI, Advanced.
class SettingsScreen extends ConsumerStatefulWidget {
  const SettingsScreen({super.key});

  @override
  ConsumerState<SettingsScreen> createState() => _SettingsScreenState();
}

class _SettingsScreenState extends ConsumerState<SettingsScreen> {
  int _devTitleTapCount = 0;
  DateTime? _devTitleLastTap;

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
      unawaited(
          ref.read(prefsRepositoryProvider).setDeveloperModeUnlocked(true));
      if (!mounted) return;
      setState(() {});
      showAppSnackBar(
        context,
        context.l10n.settingsDeveloperModeUnlockedSnack,
        duration: const Duration(seconds: 2),
      );
    }
  }

  void _pushPage(Widget page) {
    Navigator.of(context).push<void>(
      MaterialPageRoute<void>(builder: (_) => page),
    );
  }

  Future<void> _setAppearance(String v) async {
    await ref.read(prefsRepositoryProvider).setAppearance(v);
    ref.invalidate(prefsRepositoryProvider);
  }

  Future<void> _setLanguage(String v) async {
    await ref.read(prefsRepositoryProvider).setUiLanguage(v);
    ref.invalidate(prefsRepositoryProvider);
  }

  Future<void> _setFeedback(bool v) async {
    final p = ref.read(prefsRepositoryProvider);
    await p.setHapticsEnabled(v);
    await p.setSoundEffectsEnabled(v);
    ref.invalidate(prefsRepositoryProvider);
  }

  Future<void> _setAutoSummary(bool v) async {
    await ref.read(prefsRepositoryProvider).setEndgameReportAutoOpen(v);
    ref.invalidate(prefsRepositoryProvider);
  }

  @override
  Widget build(BuildContext context) {
    ref.listen<int>(mainNavTabIndexProvider, (previous, next) {
      if (next == AppMainTab.settings && previous != AppMainTab.settings) {
        ref.invalidate(appUpdateSuggestionProvider);
      }
    });
    ref.listen<AsyncValue<List<ConnectivityResult>>>(
      networkConnectivityProvider,
      (previous, next) {
        if (ref.read(mainNavTabIndexProvider) != AppMainTab.settings) return;
        final online = next.maybeWhen(
          data: (results) =>
              results.isNotEmpty &&
              results.any((r) => r != ConnectivityResult.none),
          orElse: () => false,
        );
        if (!online) return;
        final wasOffline = previous?.maybeWhen(
              data: (results) =>
                  results.isEmpty ||
                  results.every((r) => r == ConnectivityResult.none),
              orElse: () => true,
            ) ??
            true;
        if (wasOffline) ref.invalidate(appUpdateSuggestionProvider);
      },
    );

    ref.watch(connectionModeUiRefreshProvider);
    final prefs = ref.watch(prefsRepositoryProvider);
    final ui = ref.watch(gameUiNotifierProvider);
    final session = ref.watch(boardSessionNotifierProvider);
    final firmwareSnap = ref.watch(firmwareUpdateAvailabilityProvider);
    final appUpdateSnap = ref.watch(appUpdateSuggestionProvider);
    final appUpdateHighlight = appUpdateSnap.maybeWhen(
      data: (s) {
        if (s == null) return false;
        final lv = s.manifest.latestVersion.trim();
        return prefs.appUpdateBannerDismissedLatest != lv;
      },
      orElse: () => false,
    );
    final l10n = context.l10n;
    final language = switch (prefs.uiLanguage) {
      'en' => 'en',
      'cs' => 'cs',
      _ => Localizations.localeOf(context).languageCode == 'cs' ? 'cs' : 'en',
    };
    final connectionValue = switch (session.transport) {
      BoardTransport.none => l10n.settingsLinkDisconnected,
      _ when session.busy => l10n.settingsLinkConnecting,
      BoardTransport.wifi => l10n.transportWifi,
      BoardTransport.ble => l10n.transportBluetooth,
      BoardTransport.mock => l10n.uiDemo,
    };

    final hPad = isDesktopEmbedder() ? 24.0 : 16.0;

    return Scaffold(
      appBar: AppBar(
        title: GestureDetector(
          onTap: _onAppBarTitleTapForDevUnlock,
          behavior: HitTestBehavior.opaque,
          child: Text(l10n.navSettings),
        ),
      ),
      body: desktopSettingsDetailBody(
        ListView(
          padding: EdgeInsets.fromLTRB(hPad, 0, hPad, 40),
          children: [
            const AppUpdateSettingsCallout(),
            QuietGroup(
              title: l10n.settingsGroupBoard,
              children: [
                QuietRow(
                  icon: Icons.link_rounded,
                  title: l10n.uiConnection,
                  value: connectionValue,
                  onTap: () => _pushPage(const SettingsConnectionPage()),
                ),
                QuietRow(
                  icon: Icons.grid_on_outlined,
                  title: l10n.uiBoardDisplay,
                  onTap: () => _pushPage(const SettingsBoardAppearancePage()),
                ),
                QuietRow(
                  icon: Icons.lightbulb_outline,
                  title: l10n.settingsTileBoardLightTitle,
                  onTap: () => _pushPage(const SettingsBoardLightPage()),
                ),
                QuietRow(
                  icon: Icons.system_update_alt_outlined,
                  title: l10n.uiFirmware,
                  value: firmwareSnap.updateAvailable ? l10n.uiFirmwareNew : null,
                  badge: firmwareSnap.updateAvailable,
                  onTap: () => _pushPage(const FirmwareSettingsScreen()),
                ),
              ],
            ),
            QuietGroup(
              title: l10n.settingsGroupApp,
              children: [
                _InlineSegment<String>(
                  icon: Icons.contrast_rounded,
                  label: l10n.settingsAppearanceTitle,
                  value: prefs.appearance == 'light' ? 'light' : 'dark',
                  segments: [
                    ButtonSegment(
                      value: 'dark',
                      tooltip: l10n.uiThemeDark,
                      icon: const Icon(Icons.dark_mode_outlined, size: 18),
                    ),
                    ButtonSegment(
                      value: 'light',
                      tooltip: l10n.uiThemeLight,
                      icon: const Icon(Icons.light_mode_outlined, size: 18),
                    ),
                  ],
                  onChanged: _setAppearance,
                ),
                _InlineSegment<String>(
                  icon: Icons.translate_rounded,
                  label: l10n.settingsLanguage,
                  value: language,
                  segments: const [
                    ButtonSegment(value: 'cs', label: Text('CS')),
                    ButtonSegment(value: 'en', label: Text('EN')),
                  ],
                  onChanged: _setLanguage,
                ),
                QuietRow(
                  icon: Icons.vibration_rounded,
                  title: l10n.settingsFeedbackTitle,
                  trailing: Switch(
                    value: prefs.hapticsEnabled || prefs.soundEffectsEnabled,
                    onChanged: _setFeedback,
                  ),
                ),
                QuietRow(
                  icon: Icons.summarize_outlined,
                  title: l10n.uiGameSummary,
                  trailing: Switch(
                    value: prefs.endgameReportAutoOpen,
                    onChanged: _setAutoSummary,
                  ),
                ),
              ],
            ),
            QuietGroup(
              title: l10n.trainCoach,
              children: [
                QuietRow(
                  icon: Icons.psychology_outlined,
                  title: l10n.uiCoachAi,
                  onTap: () => _pushPage(const CoachAiSettingsScreen()),
                ),
                QuietRow(
                  icon: Icons.show_chart_rounded,
                  title: l10n.uiMoveEval,
                  trailing: Switch(
                    value: ui.moveEvaluationEnabled,
                    onChanged: (v) => ref
                        .read(gameUiNotifierProvider.notifier)
                        .setMoveEvaluationEnabled(v),
                  ),
                ),
                const Padding(
                  padding: EdgeInsets.fromLTRB(16, 4, 16, 14),
                  child: CoachExplanationLevelBar(),
                ),
              ],
            ),
            QuietGroup(
              title: l10n.uiGroupMore,
              children: [
                QuietRow(
                  icon: Icons.flag_circle_outlined,
                  title: l10n.helpBoardGuideTitle,
                  onTap: () =>
                      _pushPage(const OnboardingScreen(guideOnly: true)),
                ),
                QuietRow(
                  icon: Icons.help_outline,
                  title: l10n.settingsNavHelp,
                  onTap: () => _pushPage(const HelpScreen()),
                ),
                QuietRow(
                  icon: Icons.home_work_outlined,
                  title: l10n.settingsTileSmartHomeTitle,
                  onTap: () => _pushPage(const HomeAssistantMqttScreen()),
                ),
                QuietRow(
                  icon: Icons.memory_outlined,
                  title: l10n.uiDiagnostics,
                  onTap: () => _pushPage(const SettingsDiagnosticsPage()),
                ),
                QuietRow(
                  icon: Icons.info_outline,
                  title: l10n.settingsTileAboutTitle,
                  badge: appUpdateHighlight,
                  onTap: () => _pushPage(const SettingsAboutPage()),
                ),
              ],
            ),
            QuietGroup(
              children: [
                QuietRow(
                  icon: Icons.restart_alt_rounded,
                  title: l10n.settingsFactoryDialogTitle,
                  destructive: true,
                  onTap: () => _pushPage(const SettingsFactoryPage()),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }
}

/// Label on the left, compact segmented control on the right, one row.
class _InlineSegment<T> extends StatelessWidget {
  const _InlineSegment({
    required this.icon,
    required this.label,
    required this.value,
    required this.segments,
    required this.onChanged,
  });

  final IconData icon;
  final String label;
  final T value;
  final List<ButtonSegment<T>> segments;
  final ValueChanged<T> onChanged;

  @override
  Widget build(BuildContext context) {
    return QuietRow(
      icon: icon,
      title: label,
      trailing: SegmentedButton<T>(
        showSelectedIcon: false,
        style: const ButtonStyle(
          visualDensity: VisualDensity.compact,
          tapTargetSize: MaterialTapTargetSize.padded,
        ),
        segments: segments,
        selected: {value},
        onSelectionChanged: (s) {
          if (s.isNotEmpty) onChanged(s.first);
        },
      ),
    );
  }
}
