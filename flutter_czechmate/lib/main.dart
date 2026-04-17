import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:shared_preferences/shared_preferences.dart';

import 'app_providers.dart';
import 'core/constants/app_environment.dart';
import 'features/coach/coach_screen.dart';
import 'features/connection/board_discovery_screen.dart';
import 'features/connection/board_session_notifier.dart';
import 'features/game/game_screen.dart';
import 'features/settings/settings_screen.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
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

class CzechMateApp extends StatelessWidget {
  const CzechMateApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'CzechMate',
      theme: ThemeData(
        colorScheme: ColorScheme.fromSeed(seedColor: const Color(0xFF2E4A3E)),
        useMaterial3: true,
      ),
      home: const _MainShell(),
    );
  }
}

class _MainShell extends ConsumerStatefulWidget {
  const _MainShell();

  @override
  ConsumerState<_MainShell> createState() => _MainShellState();
}

class _MainShellState extends ConsumerState<_MainShell> {
  int _tab = 0;

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (AppEnvironment.staging) {
        debugPrint('[staging] resume prefs');
      }
      ref.read(boardSessionNotifierProvider.notifier).tryResumeFromPrefs();
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: IndexedStack(
        index: _tab,
        children: const [
          GameScreen(),
          BoardDiscoveryScreen(),
          CoachScreen(),
          SettingsScreen(),
        ],
      ),
      bottomNavigationBar: NavigationBar(
        selectedIndex: _tab,
        onDestinationSelected: (i) => setState(() => _tab = i),
        destinations: const [
          NavigationDestination(icon: Icon(Icons.grid_on), label: 'Hra'),
          NavigationDestination(icon: Icon(Icons.link), label: 'Deska'),
          NavigationDestination(icon: Icon(Icons.school), label: 'Coach'),
          NavigationDestination(icon: Icon(Icons.settings), label: 'Nastavení'),
        ],
      ),
    );
  }
}
