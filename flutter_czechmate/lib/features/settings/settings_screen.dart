import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app_providers.dart';
import '../../core/layout/form_factor.dart';
import '../../core/localization/context_l10n.dart';
import '../../l10n/app_localizations.dart';
import '../connection/board_session_notifier.dart';
import '../connection/board_session_state.dart';
import '../game/state/game_ui_notifier.dart';
import 'coach_ai_settings_screen.dart';
import 'firmware_update_availability.dart';
import 'firmware_settings_screen.dart';
import 'home_assistant_mqtt_screen.dart';
import 'settings_about_page.dart';
import 'settings_app_appearance_page.dart';
import 'settings_board_appearance_page.dart';
import 'settings_board_light_page.dart';
import 'settings_connection_page.dart';
import 'settings_diagnostics_page.dart';
import 'settings_factory_page.dart';
import 'settings_language_page.dart';
import 'settings_modules_page.dart';
import 'settings_overview_page.dart';

class SettingsScreen extends ConsumerStatefulWidget {
  const SettingsScreen({super.key});

  @override
  ConsumerState<SettingsScreen> createState() => _SettingsScreenState();
}

class _SettingsScreenState extends ConsumerState<SettingsScreen> {
  int _devTitleTapCount = 0;
  DateTime? _devTitleLastTap;

  String _linkTierLabel(BoardSessionState s, AppLocalizations l10n) {
    if (s.transport == BoardTransport.none) {
      return l10n.settingsLinkDisconnected;
    }
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
      unawaited(ref.read(prefsRepositoryProvider).setDeveloperModeUnlocked(true));
      if (!mounted) return;
      setState(() {});
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text(context.l10n.settingsDeveloperModeUnlockedSnack),
          duration: const Duration(seconds: 2),
        ),
      );
    }
  }

  Widget _destinationCard({
    required Widget leading,
    required String title,
    required String subtitle,
    required VoidCallback onTap,
    Color? titleColor,
    int subtitleMaxLines = 3,
  }) {
    final theme = Theme.of(context);
    final cs = theme.colorScheme;
    return Padding(
      padding: const EdgeInsets.only(bottom: 8),
      child: Card(
        margin: EdgeInsets.zero,
        clipBehavior: Clip.antiAlias,
        child: ListTile(
          contentPadding:
              const EdgeInsets.symmetric(horizontal: 16, vertical: 14),
          leading: leading,
          title: Text(
            title,
            style: theme.textTheme.titleMedium?.copyWith(
              fontWeight: FontWeight.w600,
              color: titleColor,
            ),
          ),
          subtitle: Text(
            subtitle,
            maxLines: subtitleMaxLines,
            overflow: TextOverflow.ellipsis,
            style: theme.textTheme.bodySmall?.copyWith(
              color: cs.onSurfaceVariant,
            ),
          ),
          trailing: const Icon(Icons.chevron_right),
          onTap: onTap,
        ),
      ),
    );
  }

  void _pushPage(Widget page) {
    Navigator.of(context).push<void>(
      MaterialPageRoute<void>(builder: (_) => page),
    );
  }

  @override
  Widget build(BuildContext context) {
    ref.watch(connectionModeUiRefreshProvider);
    final prefs = ref.watch(prefsRepositoryProvider);
    final ui = ref.watch(gameUiNotifierProvider);
    final session = ref.watch(boardSessionNotifierProvider);
    final firmwareSnap = ref.watch(firmwareUpdateAvailabilityProvider);
    final firmwareDevReflash = prefs.developerModeUnlocked &&
        firmwareSnap.sameSemverAsManifest &&
        firmwareSnap.hasBoardVersion &&
        !firmwareSnap.updateAvailable;
    final firmwareShowOtaGit =
        firmwareSnap.showOtaFromGitWithDeveloper(prefs.developerModeUnlocked);
    final firmwareHighlight = firmwareSnap.updateAvailable ||
        (firmwareShowOtaGit && !firmwareSnap.hasBoardVersion) ||
        firmwareDevReflash;
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
            _destinationCard(
              leading: const Icon(Icons.dashboard_outlined),
              title: l10n.settingsOverviewTitle,
              subtitle: session.lastError != null
                  ? l10n.settingsOverviewSubtitleError
                  : l10n.settingsOverviewSubtitleOk,
              onTap: () => _pushPage(const SettingsOverviewPage()),
            ),
            _destinationCard(
              leading: Icon(Icons.link, color: _linkTierColor(session, cs)),
              title: l10n.settingsConnectionTitle,
              subtitle: l10n.settingsConnectionSubtitle(
                _linkTierLabel(session, l10n),
                switch (session.transport) {
                  BoardTransport.wifi => l10n.transportWifi,
                  BoardTransport.ble => l10n.transportBluetooth,
                  BoardTransport.mock => l10n.transportShortDemo,
                  BoardTransport.none => l10n.transportShortDash,
                },
              ),
              onTap: () => _pushPage(const SettingsConnectionPage()),
            ),
            _destinationCard(
              leading: const Icon(Icons.grid_on_outlined),
              title: l10n.settingsBoardAppearanceTitle,
              subtitle: l10n.settingsBoardAppearanceSubtitle(
                ui.layoutMode == 'boardOnly'
                    ? l10n.settingsLayoutBoardOnlyShort
                    : l10n.settingsLayoutFullUiShort,
                '${(ui.boardZoomStorage * 100).round()}%',
                ui.boardStyleRaw,
              ),
              onTap: () => _pushPage(const SettingsBoardAppearancePage()),
            ),
            _destinationCard(
              leading: const Icon(Icons.palette_outlined),
              title: l10n.settingsAppAppearanceTitle,
              subtitle: switch (prefs.appearance) {
                'light' => l10n.settingsAppearanceLight,
                'dark' => l10n.settingsAppearanceDark,
                'oled' => l10n.settingsAppearanceOled,
                _ => l10n.settingsAppearanceSystem,
              },
              onTap: () => _pushPage(const SettingsAppAppearancePage()),
            ),
            _destinationCard(
              leading: const Icon(Icons.language_outlined),
              title: l10n.settingsLanguage,
              subtitle: switch (prefs.uiLanguage) {
                'cs' => l10n.languageCzech,
                'en' => l10n.languageEnglish,
                _ => l10n.languageSystem,
              },
              onTap: () => _pushPage(const SettingsLanguagePage()),
            ),
            _destinationCard(
              leading: const Icon(Icons.psychology_outlined),
              title: l10n.settingsCoachAiTitle,
              subtitle: coachAiNavSubtitle(l10n, prefs.coachAiPriority),
              onTap: () => _pushPage(const CoachAiSettingsScreen()),
            ),
            _destinationCard(
              leading: Icon(
                Icons.system_update_alt_outlined,
                color: firmwareHighlight ? cs.primary : null,
              ),
              title: firmwareSnap.updateAvailable
                  ? l10n.firmwareTileTitleUpdateAvailable(
                      firmwareSnap.manifest?.version ?? '—',
                    )
                  : (firmwareShowOtaGit
                      ? (firmwareDevReflash
                          ? l10n.firmwareTileTitleDeveloperReflash(
                              firmwareSnap.manifest?.version ?? '—',
                            )
                          : l10n.firmwareTileTitleGitBle(
                              firmwareSnap.manifest?.version ?? '—',
                            ))
                      : l10n.firmwareTileTitleDefault),
              subtitle: firmwareSnap.updateAvailable
                  ? '${l10n.firmwareTwoStepOtaHint} (${prefs.firmwareUpdateRemindersEnabled ? l10n.firmwareRemindersOnShort : l10n.firmwareRemindersOffShort}).'
                  : (firmwareShowOtaGit &&
                          !firmwareSnap.hasBoardVersion)
                      ? l10n.firmwareTileSubtitleBleGitOnly
                      : (firmwareDevReflash
                          ? l10n.firmwareTileSubtitleDeveloperReflash
                          : l10n.firmwareTileSubtitleIdle),
              titleColor: firmwareHighlight ? cs.primary : null,
              subtitleMaxLines: 4,
              onTap: () => _pushPage(const FirmwareSettingsScreen()),
            ),
            _destinationCard(
              leading: const Icon(Icons.home_work_outlined),
              title: l10n.settingsTileSmartHomeTitle,
              subtitle: l10n.settingsTileSmartHomeSubtitle,
              onTap: () => _pushPage(const HomeAssistantMqttScreen()),
            ),
            _destinationCard(
              leading: const Icon(Icons.lightbulb_outline),
              title: l10n.settingsTileBoardLightTitle,
              subtitle: l10n.settingsTileBoardLightSubtitle,
              onTap: () => _pushPage(const SettingsBoardLightPage()),
            ),
            _destinationCard(
              leading: const Icon(Icons.menu_book_outlined),
              title: l10n.settingsTileModulesTitle,
              subtitle: l10n.settingsTileModulesSubtitle,
              onTap: () => _pushPage(const SettingsModulesPage()),
            ),
            _destinationCard(
              leading: const Icon(Icons.memory_outlined),
              title: l10n.settingsTileNvsDiagTitle,
              subtitle: l10n.settingsTileNvsDiagSubtitle,
              onTap: () => _pushPage(const SettingsDiagnosticsPage()),
            ),
            _destinationCard(
              leading: const Icon(Icons.info_outline),
              title: l10n.settingsTileAboutTitle,
              subtitle: l10n.settingsTileAboutSubtitle,
              onTap: () => _pushPage(const SettingsAboutPage()),
            ),
            _destinationCard(
              leading: Icon(Icons.warning_amber_rounded, color: cs.error),
              title: l10n.settingsFactoryTileTitle,
              subtitle: l10n.settingsFactoryTileSubtitle,
              titleColor: cs.error,
              onTap: () => _pushPage(const SettingsFactoryPage()),
            ),
            const SizedBox(height: 40),
          ],
        ),
      ),
    );
  }
}
