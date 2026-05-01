import 'dart:async';
import 'dart:convert';
import 'dart:math';

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:url_launcher/url_launcher.dart';

import '../../app_navigation.dart';
import '../../app_providers.dart';
import '../../core/localization/context_l10n.dart';
import '../../core/services/prefs_repository.dart';
import '../../l10n/app_localizations.dart';
import '../connection/board_session_notifier.dart';
import '../connection/board_session_state.dart';
import '../game/board/chess_board_widget.dart';
import '../game/state/game_ui_notifier.dart';
import '../setup/board_setup_wizard_screen.dart';
import 'bundled_puzzles.dart';
import 'puzzle_service.dart';

class PuzzleLibraryItem {
  PuzzleLibraryItem({
    required this.id,
    required this.title,
    required this.fen,
    this.solution = const [],
    this.themes = const [],
    this.rating,
  });

  final String id;
  final String title;
  final String fen;
  final List<String> solution;
  final List<String> themes;
  final int? rating;

  Map<String, dynamic> toJson() => {
        'id': id,
        'title': title,
        'fen': fen,
        'solution': solution,
        'themes': themes,
        'rating': rating,
      };

  factory PuzzleLibraryItem.fromJson(Map<String, dynamic> j) {
    return PuzzleLibraryItem(
      id: j['id'] as String? ?? '',
      title: j['title'] as String? ?? 'Puzzle',
      fen: j['fen'] as String? ?? '',
      solution: (j['solution'] as List<dynamic>?)?.map((e) => '$e').toList() ??
          const [],
      themes: (j['themes'] as List<dynamic>?)?.map((e) => '$e').toList() ??
          const [],
      rating: (j['rating'] as num?)?.toInt(),
    );
  }
}

List<PuzzleLibraryItem> _decodeLibrary(String? raw) {
  if (raw == null || raw.trim().isEmpty) return [];
  try {
    final list = jsonDecode(raw) as List<dynamic>;
    return list
        .map((e) =>
            PuzzleLibraryItem.fromJson(Map<String, dynamic>.from(e as Map)))
        .toList();
  } catch (_) {
    return [];
  }
}

String _encodeLibrary(List<PuzzleLibraryItem> items) =>
    jsonEncode(items.map((e) => e.toJson()).toList());

class _TrainingPick {
  _TrainingPick({required this.title, required this.fen, this.bundled});
  final String title;
  final String fen;
  final BundledPuzzleItem? bundled;

  String displayTitle(AppLocalizations l10n) =>
      bundled != null ? bundled!.localizedTitle(l10n) : title;
}

String _sideToMove(AppLocalizations l10n, String fen) {
  final parts = fen.trim().split(RegExp(r'\s+'));
  if (parts.length >= 2) {
    return parts[1] == 'b'
        ? l10n.puzzleSideBlackMove
        : l10n.puzzleSideWhiteMove;
  }
  return l10n.puzzleSideUnknown;
}

String _localizedPuzzleTheme(AppLocalizations l10n, String raw) {
  switch (raw) {
    case 'mateIn1':
      return l10n.puzzleThemeMateIn1;
    case 'endgame':
      return l10n.puzzleThemeEndgame;
    case 'advancedPawn':
      return l10n.puzzleThemeAdvancedPawn;
    case 'opening':
      return l10n.puzzleThemeOpening;
    case 'kingsideAttack':
      return l10n.puzzleThemeKingsideAttack;
    case 'middlegame':
      return l10n.puzzleThemeMiddlegame;
    case 'tactic':
      return l10n.puzzleThemeTactic;
    default:
      return raw
          .replaceAllMapped(
              RegExp(r'([a-z])([A-Z])'), (m) => '${m.group(1)} ${m.group(2)}')
          .replaceAll('_', ' ')
          .trim();
  }
}

Widget _puzzleBoardPreview(BuildContext context, String fen,
    {double maxSide = 420}) {
  final side = min(MediaQuery.sizeOf(context).width - 48, maxSide)
      .clamp(160.0, maxSide);
  return Padding(
    padding: const EdgeInsets.symmetric(vertical: 12),
    child: Center(
      child: SizedBox(
        width: side,
        child: FenBoardPreview(fen: fen),
      ),
    ),
  );
}

Widget _themeChips(BuildContext context, List<String> themes) {
  if (themes.isEmpty) return const SizedBox.shrink();
  final l10n = context.l10n;
  return Wrap(
    spacing: 8,
    runSpacing: 8,
    children: themes.map((t) {
      return Chip(
        label: Text(_localizedPuzzleTheme(l10n, t)),
        visualDensity: VisualDensity.compact,
        materialTapTargetSize: MaterialTapTargetSize.shrinkWrap,
      );
    }).toList(),
  );
}

/// Parita iOS `PuzzleView` — Denní (Lichess), Knihovna (prefs), Trénink (vestavěné + mix).
class PuzzleScreen extends ConsumerStatefulWidget {
  const PuzzleScreen({super.key});

  @override
  ConsumerState<PuzzleScreen> createState() => _PuzzleScreenState();
}

class _PuzzleScreenState extends ConsumerState<PuzzleScreen>
    with SingleTickerProviderStateMixin {
  late TabController _tabs;
  DailyPuzzle? _daily;
  bool _dailyBusy = false;
  String? _dailyErr;
  bool _librarySavedBanner = false;
  final _libTitle = TextEditingController();
  final _libFen = TextEditingController();
  final _rnd = Random();
  Timer? _trainingTimer;
  int _trainingRoundSec = 120;
  int _trainingRemaining = 0;
  _TrainingPick? _trainingPick;

  @override
  void initState() {
    super.initState();
    _tabs = TabController(length: 3, vsync: this);
    _tabs.addListener(() {
      if (_tabs.indexIsChanging) return;
      if (_tabs.index != 2) _stopTrainingIfActive();
    });
    WidgetsBinding.instance.addPostFrameCallback((_) => _loadDaily());
  }

  @override
  void dispose() {
    _trainingTimer?.cancel();
    _tabs.dispose();
    _libTitle.dispose();
    _libFen.dispose();
    super.dispose();
  }

  Future<void> _loadDaily() async {
    setState(() {
      _dailyBusy = true;
      _dailyErr = null;
    });
    try {
      final p = await PuzzleService.fetchDaily();
      if (mounted) setState(() => _daily = p);
    } catch (e) {
      if (mounted) setState(() => _dailyErr = '$e');
    } finally {
      if (mounted) setState(() => _dailyBusy = false);
    }
  }

  Future<void> _openLichess() async {
    final u = Uri.parse('https://lichess.org/training/daily');
    if (await canLaunchUrl(u))
      await launchUrl(u, mode: LaunchMode.externalApplication);
  }

  List<PuzzleLibraryItem> _library(PrefsRepository p) =>
      _decodeLibrary(p.puzzleLibraryJson);

  List<_TrainingPick> _trainingPool(PrefsRepository p) {
    final mode = p.puzzleTrainingPoolMode;
    final bundled = bundledPuzzleCatalog
        .map((b) => _TrainingPick(title: b.title, fen: b.fen, bundled: b))
        .toList();
    final lib = _library(p)
        .map((x) => _TrainingPick(title: x.title, fen: x.fen))
        .toList();
    switch (mode) {
      case 'bundledOnly':
        return bundled;
      case 'libraryOnly':
        return lib;
      default:
        return [...bundled, ...lib];
    }
  }

  void _stopTrainingRound() {
    _trainingTimer?.cancel();
    _trainingTimer = null;
    if (mounted) setState(() => _trainingRemaining = 0);
  }

  void _stopTrainingIfActive() {
    if (_trainingRemaining > 0 || _trainingTimer != null) {
      _stopTrainingRound();
    }
  }

  Future<void> _startTrainingRound(PrefsRepository p) async {
    final pool = _trainingPool(p);
    if (pool.isEmpty) {
      if (mounted) {
        final l10n = context.l10n;
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text(l10n.puzzlePoolEmptySnack)),
        );
      }
      return;
    }
    _trainingTimer?.cancel();
    await p.incrementPuzzleTrainingSessions();
    final pick = pool[_rnd.nextInt(pool.length)];
    final sec = _trainingRoundSec.clamp(30, 600);
    if (!mounted) return;
    setState(() {
      _trainingPick = pick;
      _trainingRemaining = sec;
    });
    _trainingTimer = Timer.periodic(const Duration(seconds: 1), (t) {
      if (!mounted) {
        t.cancel();
        return;
      }
      if (_trainingRemaining <= 1) {
        t.cancel();
        _trainingTimer = null;
        setState(() => _trainingRemaining = 0);
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text(context.l10n.puzzleRoundExpiredSnack)),
        );
      } else {
        setState(() => _trainingRemaining--);
      }
    });
  }

  Future<void> _saveLibrary(
      PrefsRepository p, List<PuzzleLibraryItem> items) async {
    await p.setPuzzleLibraryJson(_encodeLibrary(items));
    setState(() {});
  }

  bool _dailyInLibrary(PrefsRepository p, DailyPuzzle d) {
    return _library(p).any((x) => x.id == d.id || x.fen == d.fen);
  }

  Future<void> _addDailyToLibrary(PrefsRepository p, DailyPuzzle d) async {
    final list = [..._library(p)];
    if (list.any((x) => x.id == d.id)) return;
    if (!mounted) return;
    final dailyTitle = context.l10n.puzzleDailyTitle(d.id);
    list.insert(
      0,
      PuzzleLibraryItem(
        id: d.id,
        title: dailyTitle,
        fen: d.fen,
        solution: d.solutionMoves,
        themes: d.themes,
        rating: d.rating,
      ),
    );
    await _saveLibrary(p, list);
    setState(() => _librarySavedBanner = true);
    Future.delayed(const Duration(seconds: 4), () {
      if (mounted && _librarySavedBanner) {
        setState(() => _librarySavedBanner = false);
      }
    });
  }

  void _tryPuzzleOnScreen(
    WidgetRef ref, {
    required String fen,
    required String title,
    List<String> solution = const [],
    int? rating,
  }) {
    _stopTrainingIfActive();
    final uiN = ref.read(gameUiNotifierProvider.notifier);
    if (solution.isEmpty) {
      uiN.enterSandboxFromCustomFen(fen);
    } else {
      uiN.enterPuzzleChallenge(
        fen: fen,
        title: title,
        solutionUci: solution,
        rating: rating,
      );
    }
    ref.read(mainNavTabIndexProvider.notifier).state = AppMainTab.game;
    final l10n = context.l10n;
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(
        content: Text(
          solution.isEmpty
              ? l10n.puzzleGoPlaySandbox
              : l10n.puzzleGoPlayPuzzle(title),
        ),
      ),
    );
  }

  void _openBoardSetupWizard(BuildContext context, String fen) {
    _stopTrainingIfActive();
    Navigator.of(context).push<void>(
      MaterialPageRoute<void>(
        fullscreenDialog: true,
        builder: (_) => BoardSetupWizardScreen(
          kind: BoardSetupWizardKind.fenGuided,
          targetFen: fen,
          loadFenWhenDone: true,
        ),
      ),
    );
  }

  Future<void> _sendToBoard(WidgetRef ref, String fen) async {
    _stopTrainingIfActive();
    final session = ref.read(boardSessionNotifierProvider);
    if (session.transport != BoardTransport.wifi &&
        session.transport != BoardTransport.ble) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text(context.l10n.puzzleConnectBoardSnack)),
      );
      return;
    }
    try {
      await ref
          .read(boardSessionNotifierProvider.notifier)
          .sendPuzzleFenToBoard(fen);
      if (mounted)
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text(context.l10n.puzzleNewGameSentSnack)),
        );
    } catch (e) {
      if (mounted)
        ScaffoldMessenger.of(context)
            .showSnackBar(SnackBar(content: Text('$e')));
    }
  }

  @override
  Widget build(BuildContext context) {
    final l10n = context.l10n;
    final prefs = ref.watch(prefsRepositoryProvider);
    final session = ref.watch(boardSessionNotifierProvider);
    final gameUi = ref.watch(gameUiNotifierProvider);
    final localPuzzle = gameUi.puzzleChallenge != null && gameUi.puzzleChallenge!.hasVerifier;
    final isPuzzleActive = session.snapshot?.status.puzzle?.active == true;
    final pData = session.snapshot?.status.puzzle;
    final showPuzzleReturnBar = isPuzzleActive || localPuzzle;

    return Scaffold(
      appBar: AppBar(
        title: Text(l10n.navPuzzle),
        leading: IconButton(
          tooltip: l10n.puzzleBackLiveTooltip,
          icon: const Icon(Icons.sports_esports_outlined),
          onPressed: () async {
            final ui = ref.read(gameUiNotifierProvider);
            if (ui.sandboxMode) {
              await ref.read(gameUiNotifierProvider.notifier).returnToLiveGame();
            }
            if (!context.mounted) return;
            ref.read(mainNavTabIndexProvider.notifier).state =
                AppMainTab.game;
          },
        ),
        bottom: TabBar(
          controller: _tabs,
          isScrollable: true,
          tabs: [
            Tab(text: l10n.puzzleTabDaily),
            Tab(text: l10n.puzzleTabLibrary),
            Tab(text: l10n.puzzleTabTraining),
          ],
        ),
      ),
      body: TabBarView(
        controller: _tabs,
        children: [
          _dailyTab(context, prefs, session),
          _libraryTab(context, prefs, session),
          _trainingTab(context, prefs, session),
        ],
      ),
      bottomNavigationBar: showPuzzleReturnBar
          ? Material(
              elevation: 8,
              color: Theme.of(context).colorScheme.surfaceContainerHigh,
              child: SafeArea(
                child: ListTile(
                  leading: const Icon(Icons.extension),
                  title: Text(
                    isPuzzleActive
                        ? (pData?.title ?? l10n.puzzleBoardRiddleTitle)
                        : (gameUi.puzzleChallenge?.title ?? l10n.puzzleBoardRiddleTitle),
                  ),
                  subtitle: Text(
                    isPuzzleActive
                        ? (pData?.message ?? '')
                        : context.l10n.gamePuzzleMovesProgress(
                            gameUi.puzzleChallenge!.playedUci.length,
                            gameUi.puzzleChallenge!.solutionUci.length,
                          ),
                  ),
                  trailing: TextButton.icon(
                    icon: const Icon(Icons.sports_esports_outlined),
                    label: Text(l10n.gameReturnLive),
                    onPressed: () async {
                      if (gameUi.sandboxMode) {
                        await ref.read(gameUiNotifierProvider.notifier).returnToLiveGame();
                      }
                      if (!context.mounted) return;
                      ref.read(mainNavTabIndexProvider.notifier).state = AppMainTab.game;
                    },
                  ),
                ),
              ),
            )
          : null,
    );
  }

  Widget _dailyTab(
      BuildContext context, PrefsRepository prefs, BoardSessionState session) {
    final l10n = context.l10n;
    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        if (_librarySavedBanner)
          Material(
            color: Theme.of(context).colorScheme.surfaceContainerHighest,
            child: ListTile(title: Text(l10n.puzzleSavedToLibraryBanner)),
          ),
        if (_dailyBusy) const LinearProgressIndicator(),
        if (_dailyErr != null)
          ListTile(
            leading: Icon(Icons.error_outline,
                color: Theme.of(context).colorScheme.error),
            title: Text(_dailyErr!),
          ),
        if (_daily != null) ...[
          Row(
            children: [
              if (_daily!.rating != null)
                Padding(
                  padding: const EdgeInsets.only(right: 12),
                  child: Text('${_daily!.rating}',
                      style: Theme.of(context).textTheme.headlineSmall),
                ),
              Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(_sideToMove(l10n, _daily!.fen),
                        style: Theme.of(context).textTheme.titleMedium),
                    if (_daily!.solutionMoves.isNotEmpty)
                      Text(
                        l10n.puzzleSolutionMoves(_daily!.solutionMoves.length),
                        style: Theme.of(context).textTheme.bodySmall,
                      ),
                  ],
                ),
              ),
            ],
          ),
          if (_daily!.themes.isNotEmpty) _themeChips(context, _daily!.themes),
          _puzzleBoardPreview(context, _daily!.fen),
          const SizedBox(height: 4),
          SelectableText(_daily!.fen,
              style: const TextStyle(fontFamily: 'monospace', fontSize: 12)),
          const SizedBox(height: 16),
          FilledButton(
            onPressed: () => _tryPuzzleOnScreen(
              ref,
              fen: _daily!.fen,
              title: l10n.puzzleDailySolveTitle,
              solution: _daily!.solutionMoves,
              rating: _daily!.rating,
            ),
            child: Text(l10n.puzzleTryOnScreen),
          ),
          const SizedBox(height: 8),
          OutlinedButton(
            onPressed: () => _sendToBoard(ref, _daily!.fen),
            child: Text(l10n.puzzleLoadToBoard),
          ),
          const SizedBox(height: 8),
          FilledButton.tonalIcon(
            onPressed: (session.transport == BoardTransport.wifi ||
                    session.transport == BoardTransport.ble)
                ? () => _openBoardSetupWizard(context, _daily!.fen)
                : null,
            icon: const Icon(Icons.lightbulb_outline),
            label: Text(l10n.puzzleSetupWizardLabel),
          ),
          const SizedBox(height: 8),
          FilledButton.tonal(
            onPressed: _dailyInLibrary(prefs, _daily!)
                ? null
                : () => _addDailyToLibrary(prefs, _daily!),
            child: Text(_dailyInLibrary(prefs, _daily!)
                ? l10n.puzzleAlreadyInLibrary
                : l10n.puzzleAddToLibrary),
          ),
          TextButton.icon(
            onPressed: _openLichess,
            icon: const Icon(Icons.open_in_browser),
            label: Text(l10n.puzzleOpenLichess),
          ),
        ],
        const SizedBox(height: 16),
        OutlinedButton(
          onPressed: _dailyBusy ? null : _loadDaily,
          child: Text(
            _daily == null ? l10n.puzzleLoadDaily : l10n.puzzleRefreshDaily,
          ),
        ),
      ],
    );
  }

  Widget _libraryTab(
      BuildContext context, PrefsRepository prefs, BoardSessionState session) {
    final l10n = context.l10n;
    final items = _library(prefs);
    return ListView(
      padding: const EdgeInsets.all(12),
      children: [
        Text(l10n.puzzleSavePositionTitle,
            style: Theme.of(context).textTheme.titleSmall),
        TextField(
            controller: _libTitle,
            decoration: InputDecoration(
              labelText: l10n.puzzleLibNameLabel,
              border: const OutlineInputBorder(),
            )),
        const SizedBox(height: 8),
        TextField(
          controller: _libFen,
          maxLines: 2,
          decoration: InputDecoration(
            labelText: l10n.puzzleLibFenLabel,
            border: const OutlineInputBorder(),
          ),
        ),
        const SizedBox(height: 8),
        FilledButton.tonal(
          onPressed: () async {
            final t = _libTitle.text.trim();
            final f = _libFen.text.trim();
            if (f.isEmpty) return;
            final list = [
              ...items,
              PuzzleLibraryItem(
                  id: DateTime.now().millisecondsSinceEpoch.toString(),
                  title: t.isEmpty ? l10n.puzzleCustomTitle : t,
                  fen: f)
            ];
            await _saveLibrary(prefs, list);
            _libTitle.clear();
            _libFen.clear();
            if (!context.mounted) return;
            ScaffoldMessenger.of(context).showSnackBar(
              SnackBar(content: Text(context.l10n.puzzleSavedSnack)),
            );
          },
          child: Text(l10n.puzzleAddToLibrary),
        ),
        const Divider(height: 24),
        if (items.isEmpty)
          Padding(
            padding: const EdgeInsets.all(24),
            child: Text(l10n.puzzleLibraryEmptyHint),
          )
        else
          ...List.generate(items.length, (i) {
            final it = items[i];
            return Card(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  ListTile(
                    title: Text(it.title),
                    subtitle: Text(it.fen.length > 48
                        ? '${it.fen.substring(0, 48)}…'
                        : it.fen),
                    isThreeLine: true,
                    onTap: () => _tryPuzzleOnScreen(
                      ref,
                      fen: it.fen,
                      title: it.title,
                      solution: it.solution,
                      rating: it.rating,
                    ),
                  ),
                  Padding(
                    padding: const EdgeInsets.symmetric(horizontal: 12),
                    child: LayoutBuilder(
                      builder: (ctx, bc) {
                        final side = min(bc.maxWidth - 8, 280.0)
                            .clamp(140.0, 280.0);
                        return Center(
                          child: SizedBox(
                            width: side,
                            child: FenBoardPreview(fen: it.fen),
                          ),
                        );
                      },
                    ),
                  ),
                  Padding(
                    padding: const EdgeInsets.fromLTRB(12, 8, 12, 12),
                    child: Row(
                      children: [
                        Expanded(
                          child: OutlinedButton.icon(
                            onPressed: (session.transport ==
                                        BoardTransport.wifi ||
                                    session.transport == BoardTransport.ble)
                                ? () => _openBoardSetupWizard(context, it.fen)
                                : null,
                            icon: const Icon(Icons.lightbulb_outline, size: 18),
                            label: Text(l10n.puzzleSetupOnBoard),
                          ),
                        ),
                        IconButton(
                          icon: const Icon(Icons.delete_outline),
                          onPressed: () async {
                            final next = [...items]..removeAt(i);
                            await _saveLibrary(prefs, next);
                          },
                        ),
                      ],
                    ),
                  ),
                ],
              ),
            );
          }),
      ],
    );
  }

  Widget _trainingTab(
      BuildContext context, PrefsRepository prefs, BoardSessionState session) {
    final l10n = context.l10n;
    final mode = prefs.puzzleTrainingPoolMode;
    final poolN = _trainingPool(prefs).length;
    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        Text(l10n.puzzlePoolModeTitle,
            style: const TextStyle(fontWeight: FontWeight.bold)),
        SegmentedButton<String>(
          segments: [
            ButtonSegment<String>(
              value: 'mixed',
              label: Text(l10n.puzzlePoolMixed),
            ),
            ButtonSegment<String>(
              value: 'bundledOnly',
              label: Text(l10n.puzzlePoolBundled),
            ),
            ButtonSegment<String>(
              value: 'libraryOnly',
              label: Text(l10n.puzzlePoolLibrary),
            ),
          ],
          selected: {mode},
          onSelectionChanged: (Set<String> s) async {
            if (s.isEmpty) return;
            await prefs.setPuzzleTrainingPoolMode(s.first);
            setState(() {});
          },
        ),
        const SizedBox(height: 8),
        Text(
          l10n.puzzleTrainingPoolStats(
            poolN,
            prefs.puzzleTrainingSessionsStarted,
          ),
          style: Theme.of(context).textTheme.bodySmall,
        ),
        const Divider(height: 24),
        Text(l10n.puzzleTimedRoundTitle,
            style: Theme.of(context).textTheme.titleSmall),
        Text(l10n.puzzleTimedRoundBody,
            style: Theme.of(context).textTheme.bodySmall),
        const SizedBox(height: 8),
        Row(
          children: [
            Text('${_trainingRoundSec}s',
                style: Theme.of(context).textTheme.titleMedium),
            Expanded(
              child: Slider(
                value: _trainingRoundSec.toDouble().clamp(30, 600),
                min: 30,
                max: 600,
                divisions: 38,
                label: '${_trainingRoundSec}s',
                onChanged: (v) => setState(
                    () => _trainingRoundSec = v.round().clamp(30, 600)),
              ),
            ),
          ],
        ),
        Row(
          children: [
            Expanded(
              child: FilledButton(
                onPressed: _trainingRemaining > 0
                    ? null
                    : () => _startTrainingRound(prefs),
                child: Text(l10n.puzzleNewRound),
              ),
            ),
            const SizedBox(width: 8),
            Expanded(
              child: OutlinedButton(
                onPressed: _trainingRemaining > 0 ? _stopTrainingRound : null,
                child: Text(l10n.puzzleStop),
              ),
            ),
          ],
        ),
        if (_trainingPick != null) ...[
          const SizedBox(height: 12),
          Card(
            child: Padding(
              padding: const EdgeInsets.all(12),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  if (_trainingRemaining > 0)
                    Text(l10n.puzzleRemainingSeconds(_trainingRemaining),
                        style: Theme.of(context).textTheme.headlineSmall),
                  Text(_trainingPick!.displayTitle(l10n),
                      style: Theme.of(context).textTheme.titleMedium),
                  Text(_sideToMove(l10n, _trainingPick!.fen),
                      style: Theme.of(context).textTheme.bodySmall),
                  const SizedBox(height: 6),
                  SelectableText(_trainingPick!.fen,
                      style: const TextStyle(
                          fontFamily: 'monospace', fontSize: 11)),
                  _puzzleBoardPreview(context, _trainingPick!.fen, maxSide: 360),
                  const SizedBox(height: 10),
                  FilledButton.tonal(
                    onPressed: () => _tryPuzzleOnScreen(
                      ref,
                      fen: _trainingPick!.fen,
                      title: _trainingPick!.displayTitle(l10n),
                    ),
                    child: Text(l10n.puzzleSolveOnScreen),
                  ),
                  const SizedBox(height: 6),
                  OutlinedButton(
                    onPressed: () => _sendToBoard(ref, _trainingPick!.fen),
                    child: Text(l10n.puzzleLoadToBoardShort),
                  ),
                  const SizedBox(height: 6),
                  OutlinedButton.icon(
                    onPressed: (session.transport == BoardTransport.wifi ||
                            session.transport == BoardTransport.ble)
                        ? () =>
                            _openBoardSetupWizard(context, _trainingPick!.fen)
                        : null,
                    icon: const Icon(Icons.lightbulb_outline, size: 18),
                    label: Text(l10n.puzzleSetupWizardShort),
                  ),
                ],
              ),
            ),
          ),
        ],
        const Divider(height: 32),
        Text(l10n.puzzleBundledOfflineTitle,
            style: const TextStyle(fontWeight: FontWeight.bold)),
        for (final b in bundledPuzzleCatalog)
          Card(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                ListTile(
                  title: Text(b.localizedTitle(l10n)),
                  subtitle: Text(b.localizedSubtitle(l10n)),
                  isThreeLine: true,
                  trailing: const Icon(Icons.chevron_right),
                  onTap: () => _tryPuzzleOnScreen(
                    ref,
                    fen: b.fen,
                    title: b.localizedTitle(l10n),
                    solution: b.solution,
                  ),
                ),
                Padding(
                  padding: const EdgeInsets.symmetric(horizontal: 12),
                  child: LayoutBuilder(
                    builder: (ctx, bc) {
                      final side =
                          min(bc.maxWidth - 8, 260.0).clamp(140.0, 260.0);
                      return Center(
                        child: SizedBox(
                          width: side,
                          child: FenBoardPreview(fen: b.fen),
                        ),
                      );
                    },
                  ),
                ),
                if (b.themes.isNotEmpty)
                  Padding(
                    padding: const EdgeInsets.fromLTRB(12, 0, 12, 12),
                    child: _themeChips(context, b.themes),
                  ),
                Padding(
                  padding: const EdgeInsets.fromLTRB(12, 0, 12, 12),
                  child: OutlinedButton.icon(
                    onPressed: (session.transport == BoardTransport.wifi ||
                            session.transport == BoardTransport.ble)
                        ? () => _openBoardSetupWizard(context, b.fen)
                        : null,
                    icon: const Icon(Icons.lightbulb_outline, size: 18),
                    label: Text(l10n.puzzleSetupWizardLabel),
                  ),
                ),
              ],
            ),
          ),
      ],
    );
  }
}
