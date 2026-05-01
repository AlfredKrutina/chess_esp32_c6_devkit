import 'dart:async';
import 'dart:math' as math;
import 'dart:ui' show PlatformDispatcher;

import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:shared_preferences/shared_preferences.dart';

import 'app_providers.dart';
import 'core/constants/app_environment.dart';
import 'core/services/watch_command_binding.dart';
import 'core/layout/form_factor.dart';
import 'features/analysis/analysis_screen.dart';
import 'features/connection/board_session_notifier.dart';
import 'features/game/game_screen.dart';
import 'features/onboarding/onboarding_screen.dart';
import 'features/progress/progress_screen.dart';
import 'features/puzzle/puzzle_screen.dart';
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
  final prefs = await SharedPreferences.getInstance();
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
    final appearance = ref.watch(prefsRepositoryProvider).appearance;

    ThemeMode themeMode;
    switch (appearance) {
      case 'light':
        themeMode = ThemeMode.light;
        break;
      case 'dark':
        themeMode = ThemeMode.dark;
        break;
      case 'oled':
        themeMode = ThemeMode.dark;
        break;
      default:
        themeMode = ThemeMode.system;
    }

    const seedColor = Color(0xFF2E4A3E);
    ThemeData buildBaseTheme(ColorScheme scheme,
        {Color? scaffoldBackgroundColor}) {
      const radius = 12.0;
      final roundedShape = RoundedRectangleBorder(
        borderRadius: BorderRadius.circular(radius),
      );
      final textTheme = ThemeData(
        colorScheme: scheme,
        useMaterial3: true,
      ).textTheme;
      return ThemeData(
        colorScheme: scheme,
        scaffoldBackgroundColor: scaffoldBackgroundColor,
        useMaterial3: true,
        visualDensity: VisualDensity.adaptivePlatformDensity,
        textTheme: textTheme.copyWith(
          headlineSmall: textTheme.headlineSmall
              ?.copyWith(fontWeight: FontWeight.w700, height: 1.15),
          titleLarge: textTheme.titleLarge
              ?.copyWith(fontWeight: FontWeight.w700, height: 1.2),
          titleMedium: textTheme.titleMedium
              ?.copyWith(fontWeight: FontWeight.w600, height: 1.25),
          bodyMedium: textTheme.bodyMedium?.copyWith(height: 1.35),
          bodySmall: textTheme.bodySmall?.copyWith(height: 1.3),
          labelSmall: textTheme.labelSmall?.copyWith(letterSpacing: 0.2),
        ),
        appBarTheme: AppBarTheme(
          centerTitle: false,
          elevation: 0,
          scrolledUnderElevation: 0,
          titleTextStyle: textTheme.titleLarge?.copyWith(
            color: scheme.onSurface,
            fontWeight: FontWeight.w700,
          ),
        ),
        cardTheme: CardThemeData(
          elevation: 0,
          margin: const EdgeInsets.symmetric(vertical: 8),
          shape: roundedShape,
        ),
        inputDecorationTheme: InputDecorationTheme(
          filled: true,
          fillColor: scheme.surfaceContainerHighest.withValues(alpha: 0.45),
          contentPadding:
              const EdgeInsets.symmetric(horizontal: 14, vertical: 14),
          border: OutlineInputBorder(
            borderRadius: BorderRadius.circular(radius),
            borderSide: BorderSide(color: scheme.outlineVariant),
          ),
          enabledBorder: OutlineInputBorder(
            borderRadius: BorderRadius.circular(radius),
            borderSide: BorderSide(color: scheme.outlineVariant),
          ),
          focusedBorder: OutlineInputBorder(
            borderRadius: BorderRadius.circular(radius),
            borderSide: BorderSide(color: scheme.primary, width: 1.4),
          ),
        ),
        listTileTheme: ListTileThemeData(
          contentPadding:
              const EdgeInsets.symmetric(horizontal: 16, vertical: 4),
          shape: roundedShape,
        ),
        dialogTheme: DialogThemeData(
          shape: roundedShape,
        ),
        dividerTheme: DividerThemeData(
          color: scheme.outlineVariant.withValues(alpha: 0.55),
          thickness: 1,
        ),
        filledButtonTheme: FilledButtonThemeData(
          style: FilledButton.styleFrom(
            shape: roundedShape,
            padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 12),
          ),
        ),
        outlinedButtonTheme: OutlinedButtonThemeData(
          style: OutlinedButton.styleFrom(
            shape: roundedShape,
            padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 12),
          ),
        ),
        textButtonTheme: TextButtonThemeData(
          style: TextButton.styleFrom(
            shape: roundedShape,
            padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 10),
          ),
        ),
        segmentedButtonTheme: SegmentedButtonThemeData(
          style: ButtonStyle(
            visualDensity: VisualDensity.compact,
            shape: WidgetStatePropertyAll(
              RoundedRectangleBorder(
                  borderRadius: BorderRadius.circular(radius)),
            ),
          ),
        ),
        navigationRailTheme: NavigationRailThemeData(
          backgroundColor: scheme.surfaceContainerLow,
          indicatorColor: scheme.secondaryContainer,
          selectedIconTheme: IconThemeData(color: scheme.onSecondaryContainer),
          selectedLabelTextStyle: TextStyle(
            color: scheme.onSecondaryContainer,
            fontWeight: FontWeight.w600,
          ),
        ),
      );
    }

    final lightTheme =
        buildBaseTheme(ColorScheme.fromSeed(seedColor: seedColor));
    final darkTheme = appearance == 'oled'
        ? buildBaseTheme(
            ColorScheme.fromSeed(
              seedColor: seedColor,
              brightness: Brightness.dark,
            ).copyWith(surface: Colors.black, onSurface: Colors.white),
            scaffoldBackgroundColor: Colors.black,
          )
        : buildBaseTheme(
            ColorScheme.fromSeed(
              seedColor: seedColor,
              brightness: Brightness.dark,
            ),
          );

    return MaterialApp(
      title: 'czechmate',
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
    if (!_decided) {
      return const Scaffold(body: Center(child: CircularProgressIndicator()));
    }
    if (_showOnboarding) {
      return OnboardingScreen(onDone: () async {
        await ref
            .read(sharedPreferencesProvider)
            .setBool('czechmate.onboardingSeen', true);
        if (mounted) setState(() => _showOnboarding = false);
      });
    }
    return const _MainShell();
  }
}

class _MainShell extends ConsumerStatefulWidget {
  const _MainShell();
  @override
  ConsumerState<_MainShell> createState() => _MainShellState();
}

class _MainShellState extends ConsumerState<_MainShell>
    with WidgetsBindingObserver {
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
    });
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
    final cmd =
        await ref.read(liveActivityServiceProvider).consumePendingAndroidClockAction();
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
    ref.listen<FirmwareAvailState>(firmwareUpdateAvailabilityProvider, (prev, next) {
      if (next.loading || !next.updateAvailable) {
        return;
      }
      WidgetsBinding.instance.addPostFrameCallback((_) {
        if (!context.mounted) {
          return;
        }
        unawaited(FirmwareUpdateDailyPrompt.tryShow(context, ref));
      });
    });

    final tab = ref.watch(mainNavTabIndexProvider);
    final fwSnap = ref.watch(firmwareUpdateAvailabilityProvider);
    final fwBadge = fwSnap.updateAvailable && !fwSnap.loading;

    Widget settingsIcon(IconData icon) {
      if (!fwBadge) {
        return Icon(icon);
      }
      return Badge(
        backgroundColor: Theme.of(context).colorScheme.error,
        smallSize: 8,
        child: Icon(icon),
      );
    }
    const tabPages = [
      GameScreen(),
      AnalysisScreen(),
      PuzzleScreen(),
      ProgressScreen(),
      SettingsScreen(),
    ];

    final size = MediaQuery.sizeOf(context);
    final desktopShell = useDesktopNavigationShell(size);

    final indexed = IndexedStack(
      index: tab,
      children: tabPages,
    );

    final destinations = <NavigationRailDestination>[
      const NavigationRailDestination(
        icon: Icon(Icons.grid_view_rounded),
        selectedIcon: Icon(Icons.grid_view_rounded),
        label: Text('Play'),
      ),
      const NavigationRailDestination(
        icon: Icon(Icons.show_chart_outlined),
        selectedIcon: Icon(Icons.show_chart),
        label: Text('Analysis'),
      ),
      const NavigationRailDestination(
        icon: Icon(Icons.extension_outlined),
        selectedIcon: Icon(Icons.extension),
        label: Text('Puzzle'),
      ),
      const NavigationRailDestination(
        icon: Icon(Icons.horizontal_split_outlined),
        selectedIcon: Icon(Icons.horizontal_split),
        label: Text('Progress'),
      ),
      NavigationRailDestination(
        icon: settingsIcon(Icons.settings_outlined),
        selectedIcon: settingsIcon(Icons.settings),
        label: const Text('Settings'),
      ),
    ];

    final bottomDestinations = [
      const NavigationDestination(
        icon: Icon(Icons.grid_view_rounded),
        selectedIcon: Icon(Icons.grid_view_rounded),
        label: 'Play',
      ),
      const NavigationDestination(
        icon: Icon(Icons.show_chart_outlined),
        selectedIcon: Icon(Icons.show_chart),
        label: 'Analysis',
      ),
      const NavigationDestination(
        icon: Icon(Icons.extension_outlined),
        selectedIcon: Icon(Icons.extension),
        label: 'Puzzle',
      ),
      const NavigationDestination(
        icon: Icon(Icons.horizontal_split_outlined),
        selectedIcon: Icon(Icons.horizontal_split),
        label: 'Progress',
      ),
      NavigationDestination(
        icon: settingsIcon(Icons.settings_outlined),
        selectedIcon: settingsIcon(Icons.settings),
        label: 'Settings',
      ),
    ];

    final railExtended = size.width >= 1280;
    final body = desktopShell
        ? Row(
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: [
              NavigationRail(
                selectedIndex: tab,
                onDestinationSelected: (i) =>
                    ref.read(mainNavTabIndexProvider.notifier).state = i,
                extended: railExtended,
                minExtendedWidth: 212,
                // With extended:true, Flutter requires labelType none/null (labels sit beside icons).
                labelType: railExtended
                    ? NavigationRailLabelType.none
                    : (size.width >= 1100
                        ? NavigationRailLabelType.all
                        : NavigationRailLabelType.selected),
                destinations: destinations,
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
          )
        : indexed;

    return Scaffold(
      body: body,
      bottomNavigationBar: desktopShell
          ? null
          : NavigationBar(
              selectedIndex: tab,
              onDestinationSelected: (i) =>
                  ref.read(mainNavTabIndexProvider.notifier).state = i,
              destinations: bottomDestinations,
            ),
    );
  }
}
