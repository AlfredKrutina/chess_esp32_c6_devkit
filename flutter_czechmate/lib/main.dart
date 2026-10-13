import 'dart:async';
import 'dart:io' show Platform;
import 'dart:math' as math;

import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:flutter_localizations/flutter_localizations.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:shared_preferences/shared_preferences.dart';

import 'app_providers.dart';
import 'core/config/coach_env_config.dart';
import 'core/constants/app_environment.dart';
import 'core/theme/app_motion.dart';
import 'core/theme/app_theme.dart';
import 'core/services/prefs_repository.dart';
import 'core/localization/context_l10n.dart';
import 'core/localization/locale_bridge.dart';
import 'l10n/app_localizations.dart';
import 'core/services/watch_command_binding.dart';
import 'core/layout/form_factor.dart';
import 'app_navigation.dart';
import 'features/connection/board_session_notifier.dart';
import 'features/game/game_screen.dart';
import 'features/game/state/running_game.dart';
import 'features/onboarding/onboarding_screen.dart';
import 'features/learn/learn_hub_screen.dart';
import 'features/settings/firmware_update_availability.dart';
import 'features/settings/firmware_update_daily_prompt.dart';
import 'features/settings/settings_screen.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  if (AppEnvironment.staging) {
    FlutterError.onError = (details) {
      FlutterError.presentError(details);
      debugPrint('[staging][FlutterError] ${details.exceptionAsString()}');
      debugPrint('[staging][stack] ${details.stack}');
    };
    PlatformDispatcher.instance.onError = (error, stack) {
      debugPrint('[staging][async] $error');
      debugPrint(stack.toString());
      return false;
    };
  }
  await CoachEnvConfig.load();
  final prefs = await SharedPreferences.getInstance();
  await PrefsRepository(prefs).migrateLegacyConnectionModeToAutoIfNeeded();
  runApp(
    ProviderScope(
      overrides: [
        sharedPreferencesProvider.overrideWithValue(prefs),
      ],
      child: const CzechMateApp(),
    ),
  );
}

class CzechMateApp extends ConsumerWidget {
  const CzechMateApp({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final prefs = ref.watch(prefsRepositoryProvider);
    final appearance = prefs.appearance;

    final themeMode =
        appearance == 'light' ? ThemeMode.light : ThemeMode.dark;
    final lightTheme = AppTheme.light();
    final darkTheme = AppTheme.dark();

    return MaterialApp(
      onGenerateTitle: (ctx) =>
          AppLocalizations.of(ctx)?.appTitle ?? 'CzechMate',
      locale: switch (prefs.uiLanguage) {
        'cs' => const Locale('cs'),
        'en' => const Locale('en'),
        _ => null,
      },
      localizationsDelegates: const [
        AppLocalizations.delegate,
        GlobalMaterialLocalizations.delegate,
        GlobalWidgetsLocalizations.delegate,
        GlobalCupertinoLocalizations.delegate,
      ],
      supportedLocales: AppLocalizations.supportedLocales,
      localeListResolutionCallback: (locales, supported) {
        final device = locales?.first ?? const Locale('en');
        final resolved = resolvedLocaleForPrefs(prefs, device);
        for (final s in supported) {
          if (s.languageCode == resolved.languageCode) return s;
        }
        return supported.first;
      },
      theme: lightTheme,
      darkTheme: darkTheme,
      themeMode: themeMode,
      builder: (context, child) {
        if (child == null) return const SizedBox.shrink();
        if (!isDesktopEmbedder()) return child;
        final mq = MediaQuery.of(context);
        final scaled = mq.textScaler.clamp(
          minScaleFactor: 0.92,
          maxScaleFactor: 1.06,
        );
        return MediaQuery(
          data: mq.copyWith(textScaler: scaled),
          child: child,
        );
      },
      home: const _StartupRouter(),
    );
  }
}

class _StartupRouter extends ConsumerStatefulWidget {
  const _StartupRouter();
  @override
  ConsumerState<_StartupRouter> createState() => _StartupRouterState();
}

class _StartupRouterState extends ConsumerState<_StartupRouter> {
  bool _decided = false;
  bool _showOnboarding = false;

  @override
  void initState() {
    super.initState();
    final prefs = ref.read(sharedPreferencesProvider);
    final seen = prefs.getBool('czechmate.onboardingSeen') ?? false;
    _showOnboarding = !seen;
    _decided = true;
  }

  @override
  Widget build(BuildContext context) {
    late final Widget page;
    late final Key pageKey;
    if (!_decided) {
      pageKey = const ValueKey<String>('startup_loading');
      page = const Scaffold(
        body: Center(child: CircularProgressIndicator()),
      );
    } else if (_showOnboarding) {
      pageKey = const ValueKey<String>('startup_onboarding');
      page = OnboardingScreen(onDone: () async {
        await ref
            .read(sharedPreferencesProvider)
            .setBool('czechmate.onboardingSeen', true);
        if (mounted) setState(() => _showOnboarding = false);
      });
    } else {
      pageKey = const ValueKey<String>('startup_main');
      page = const _MainShell();
    }
    return AnimatedSwitcher(
      duration: AppMotion.crossFade + const Duration(milliseconds: 40),
      switchInCurve: AppMotion.standardCurve,
      switchOutCurve: AppMotion.reverseCurve,
      layoutBuilder: (currentChild, previousChildren) {
        return Stack(
          alignment: Alignment.center,
          children: <Widget>[
            ...previousChildren,
            if (currentChild != null) currentChild,
          ],
        );
      },
      child: KeyedSubtree(
        key: pageKey,
        child: page,
      ),
    );
  }
}

class _MainShell extends ConsumerStatefulWidget {
  const _MainShell();
  @override
  ConsumerState<_MainShell> createState() => _MainShellState();
}

class _MainShellState extends ConsumerState<_MainShell>
    with WidgetsBindingObserver {
  bool _autoFocusedRunningGame = false;

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addObserver(this);
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (AppEnvironment.staging) debugPrint('[staging] resume prefs');
      ref.read(boardSessionNotifierProvider.notifier).tryResumeFromPrefs();
      ensureWatchInboundBinding(ref);
      unawaited(_consumeAndroidNotificationClockActions());
      unawaited(_refreshFirmwareOfferAndMaybePrompt());
      if (!kIsWeb && (Platform.isIOS || Platform.isAndroid)) {
        unawaited(_primeHomeScreenWidget(ref));
      }
    });
  }

  Future<void> _primeHomeScreenWidget(WidgetRef r) async {
    final hw = r.read(boardHomeWidgetSyncProvider);
    await hw.ensureInitialized();
    final sess = r.read(boardSessionNotifierProvider);
    final live = r.read(liveActivityServiceProvider);
    await hw.syncPayload(live.extensionPayload(sess));
  }

  Future<void> _refreshFirmwareOfferAndMaybePrompt() async {
    await ref.read(firmwareUpdateAvailabilityProvider.notifier).refresh();
    if (!mounted) {
      return;
    }
    await FirmwareUpdateDailyPrompt.tryShow(context, ref);
  }

  @override
  void dispose() {
    WidgetsBinding.instance.removeObserver(this);
    super.dispose();
  }

  // Resume polling when app returns to foreground.
  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    if (state == AppLifecycleState.resumed) {
      ref.read(boardSessionNotifierProvider.notifier).tryResumeFromPrefs();
      unawaited(_consumeAndroidNotificationClockActions());
      unawaited(_refreshFirmwareOfferAndMaybePrompt());
    }
  }

  Future<void> _consumeAndroidNotificationClockActions() async {
    final cmd = await ref
        .read(liveActivityServiceProvider)
        .consumePendingAndroidClockAction();
    if (cmd == null) return;
    final notifier = ref.read(boardSessionNotifierProvider.notifier);
    switch (cmd) {
      case 'pause':
        await notifier.postTimerPause();
        break;
      case 'resume':
        await notifier.postTimerResume();
        break;
    }
  }

  @override
  Widget build(BuildContext context) {
    final l10n = context.l10n;
    ref.listen<FirmwareAvailState>(firmwareUpdateAvailabilityProvider,
        (prev, next) {
      if (next.loading ||
          !next.updateAvailable ||
          next.boardOtaSupported == false) {
        return;
      }
      WidgetsBinding.instance.addPostFrameCallback((_) {
        if (!context.mounted) {
          return;
        }
        unawaited(FirmwareUpdateDailyPrompt.tryShow(context, ref));
      });
    });

    ref.listen<int>(mainNavTabIndexProvider, (prev, next) {
      if (prev == next) return;
      unawaited(ref.read(prefsRepositoryProvider).setLastMainTab(next));
    });

    ref.listen<bool>(
      boardSessionNotifierProvider
          .select((s) => boardHasRunningGame(s.snapshot?.status)),
      (prev, next) {
        if (_autoFocusedRunningGame || !next) return;
        _autoFocusedRunningGame = true;
        if (AppEnvironment.staging) {
          debugPrint('[staging] running game on board -> Play tab');
        }
        ref.read(mainNavTabIndexProvider.notifier).state = AppMainTab.game;
      },
    );

    final tab = ref.watch(mainNavTabIndexProvider);
    final fwSnap = ref.watch(firmwareUpdateAvailabilityProvider);
    final fwBadge = fwSnap.updateAvailable && !fwSnap.loading;

    Widget settingsIcon(IconData icon) {
      if (!fwBadge) return Icon(icon);
      return Badge(
        backgroundColor: Theme.of(context).colorScheme.error,
        smallSize: 8,
        child: Icon(icon),
      );
    }

    const tabPages = [
      GameScreen(),
      TrainScreen(),
      SettingsScreen(),
    ];

    final items = <({IconData icon, IconData selected, String label})>[
      (
        icon: Icons.sports_esports_outlined,
        selected: Icons.sports_esports_rounded,
        label: l10n.navPlay
      ),
      (
        icon: Icons.extension_outlined,
        selected: Icons.extension_rounded,
        label: l10n.navTrain
      ),
      (
        icon: Icons.settings_outlined,
        selected: Icons.settings_rounded,
        label: l10n.navSettings
      ),
    ];

    void select(int i) {
      if (i == tab) return;
      ref.read(mainNavTabIndexProvider.notifier).state = i;
    }

    final size = MediaQuery.sizeOf(context);
    final desktopShell = useDesktopNavigationShell(size);
    final phoneLandscape = !desktopShell &&
        size.width > size.height &&
        size.height < 560;
    final indexed = _FadeIndexedStack(index: tab, children: tabPages);

    List<NavigationRailDestination> railDestinations() => [
          for (var i = 0; i < items.length; i++)
            NavigationRailDestination(
              icon: i == AppMainTab.settings
                  ? settingsIcon(items[i].icon)
                  : Icon(items[i].icon),
              selectedIcon: i == AppMainTab.settings
                  ? settingsIcon(items[i].selected)
                  : Icon(items[i].selected),
              label: Text(items[i].label),
            ),
        ];

    if (phoneLandscape) {
      return Scaffold(
        body: Row(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            SafeArea(
              right: false,
              child: NavigationRail(
                selectedIndex: tab,
                onDestinationSelected: select,
                minWidth: 64,
                labelType: NavigationRailLabelType.selected,
                groupAlignment: 0,
                destinations: railDestinations(),
              ),
            ),
            Expanded(
              child: MediaQuery.removePadding(
                context: context,
                removeLeft: true,
                child: indexed,
              ),
            ),
          ],
        ),
      );
    }

    if (!desktopShell) {
      return Scaffold(
        body: indexed,
        bottomNavigationBar: NavigationBar(
          selectedIndex: tab,
          onDestinationSelected: select,
          animationDuration: AppMotion.resolve(context, AppMotion.stateChange),
          destinations: [
            for (var i = 0; i < items.length; i++)
              NavigationDestination(
                tooltip: items[i].label,
                icon: i == AppMainTab.settings
                    ? settingsIcon(items[i].icon)
                    : Icon(items[i].icon),
                selectedIcon: i == AppMainTab.settings
                    ? settingsIcon(items[i].selected)
                    : Icon(items[i].selected),
                label: items[i].label,
              ),
          ],
        ),
      );
    }

    final railExtended = size.width >= 1280;
    return Scaffold(
      body: Row(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          NavigationRail(
            selectedIndex: tab,
            onDestinationSelected: select,
            extended: railExtended,
            minExtendedWidth: 212,
            labelType: railExtended
                ? NavigationRailLabelType.none
                : NavigationRailLabelType.all,
            destinations: railDestinations(),
          ),
          const VerticalDivider(width: 1, thickness: 1),
          Expanded(
            child: LayoutBuilder(
              builder: (context, constraints) {
                final maxW = desktopContentMaxWidth(constraints.maxWidth);
                final w = math.min(constraints.maxWidth, maxW);
                return Padding(
                  padding: desktopContentPadding(true),
                  child: Center(
                    child: SizedBox(
                      width: w,
                      height: constraints.maxHeight,
                      child: indexed,
                    ),
                  ),
                );
              },
            ),
          ),
        ],
      ),
    );
  }
}

/// Keeps every tab alive like [IndexedStack] and fades the newly selected tab
/// in. Reduced motion switches instantly.
class _FadeIndexedStack extends StatefulWidget {
  const _FadeIndexedStack({required this.index, required this.children});

  final int index;
  final List<Widget> children;

  @override
  State<_FadeIndexedStack> createState() => _FadeIndexedStackState();
}

class _FadeIndexedStackState extends State<_FadeIndexedStack>
    with SingleTickerProviderStateMixin {
  late final AnimationController _c = AnimationController(
    vsync: this,
    duration: AppMotion.crossFade,
    value: 1,
  );

  @override
  void didUpdateWidget(covariant _FadeIndexedStack oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.index != widget.index) {
      if (AppMotion.reduced(context)) {
        _c.value = 1;
      } else {
        _c.forward(from: 0);
      }
    }
  }

  @override
  void dispose() {
    _c.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final curved = CurvedAnimation(parent: _c, curve: AppMotion.standardCurve);
    return FadeTransition(
      opacity: Tween(begin: 0.4, end: 1.0).animate(curved),
      child: SlideTransition(
        position: Tween(begin: const Offset(0, 0.012), end: Offset.zero)
            .animate(curved),
        child: IndexedStack(index: widget.index, children: widget.children),
      ),
    );
  }
}
