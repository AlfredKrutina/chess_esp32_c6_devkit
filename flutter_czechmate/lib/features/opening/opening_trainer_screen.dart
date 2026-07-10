import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app_providers.dart';
import '../../core/layout/form_factor.dart';
import '../../core/localization/context_l10n.dart';
import '../../core/widgets/glass_snackbar.dart';
import '../../core/utils/board_setup_fen_steps.dart';
import '../connection/board_session_notifier.dart';
import '../setup/board_setup_wizard_screen.dart';
import 'opening_catalog_repository.dart';

class OpeningTrainerScreen extends ConsumerStatefulWidget {
  const OpeningTrainerScreen({super.key, required this.lineId, this.mode = 'learn'});

  final String lineId;
  final String mode;

  bool get isDrillLike => mode == 'drill' || mode == 'timed' || mode == 'mirror';

  @override
  ConsumerState<OpeningTrainerScreen> createState() =>
      _OpeningTrainerScreenState();
}

class _OpeningTrainerScreenState extends ConsumerState<OpeningTrainerScreen> {
  OpeningLine? _line;
  String? _error;
  bool _busy = false;
  String _feedback = 'none';
  int _playerPly = 0;
  int _playerTotal = 0;
  int _drillErrors = 0;
  int _timedSecondsLeft = 90;
  bool _timedExpired = false;
  bool _progressSaved = false;
  bool _setupWizardInFlight = false;
  Timer? _hintRefreshTimer;
  Timer? _countdownTimer;

  static const _hintRefreshMs = 600;
  static const _timedLimitSec = 90;

  @override
  void initState() {
    super.initState();
    _load();
  }

  @override
  void dispose() {
    _stopHintRefresh();
    _countdownTimer?.cancel();
    super.dispose();
  }

  void _startTimedCountdown() {
    if (widget.mode != 'timed') return;
    _countdownTimer?.cancel();
    _timedSecondsLeft = _timedLimitSec;
    _timedExpired = false;
    _countdownTimer = Timer.periodic(const Duration(seconds: 1), (t) {
      if (!mounted) return;
      if (_feedback == 'complete') {
        t.cancel();
        return;
      }
      setState(() {
        _timedSecondsLeft--;
        if (_timedSecondsLeft <= 0) {
          _timedExpired = true;
          t.cancel();
        }
      });
      if (_timedExpired) {
        showGlassSnackBar(context, 'Čas vypršel');
        _cancel();
      }
    });
  }

  Future<void> _saveProgressIfNeeded() async {
    if (_progressSaved || _feedback != 'complete') return;
    _progressSaved = true;
    await ref.read(openingProgressRepositoryProvider).recordCompletion(
          lineId: widget.lineId,
          mode: widget.mode,
          drillErrors: _drillErrors,
          timedSuccess: widget.mode != 'timed' || !_timedExpired,
        );
  }

  void _stopHintRefresh() {
    _hintRefreshTimer?.cancel();
    _hintRefreshTimer = null;
  }

  void _syncHintRefresh(bool activeLesson, bool checkpoint) {
    if (!activeLesson || checkpoint) {
      _stopHintRefresh();
      return;
    }
    if (_hintRefreshTimer != null) return;
    _hintRefreshTimer = Timer.periodic(
      const Duration(milliseconds: _hintRefreshMs),
      (_) => _refreshOpeningHint(),
    );
  }

  Future<void> _refreshOpeningHint() async {
    try {
      await ref
          .read(boardSessionNotifierProvider.notifier)
          .postOpeningRaw({'action': 'hint'});
    } catch (_) {}
  }

  Future<void> _load() async {
    try {
      final line = await OpeningCatalogRepository().findById(widget.lineId);
      if (!mounted) return;
      if (line == null) {
        setState(() => _error = 'Opening not found');
        return;
      }
      setState(() {
        _line = line;
        _playerTotal = line.playerPlyIndices.length;
      });
      await _startLesson(line);
    } catch (e) {
      if (mounted) setState(() => _error = e.toString());
    }
  }

  Future<void> _startLesson(OpeningLine line) async {
    setState(() => _busy = true);
    try {
      await ref
          .read(boardSessionNotifierProvider.notifier)
          .postOpeningAction(line.toStartPayload(mode: widget.mode));
      final opening = ref
          .read(boardSessionNotifierProvider)
          .snapshot
          ?.status
          .openingTraining;
      if (opening?.setupPhase == true) {
        if (mounted) {
          showGlassSnackBar(
            context,
            'Deska nesedí se startovní pozicí — nastav figurky',
          );
        }
        return;
      }
      _startTimedCountdown();
      if (mounted) {
        final msg = widget.mode == 'drill'
            ? 'Drill — táhni bez nápověd v textu'
            : widget.mode == 'timed'
                ? 'Na čas $_timedLimitSec s — začni!'
                : 'Lekce spuštěna — táhni na desce';
        showGlassSnackBar(context, msg);
      }
    } catch (e) {
      if (mounted) setState(() => _error = e.toString());
    } finally {
      if (mounted) setState(() => _busy = false);
    }
  }

  Future<void> _openSetupWizard(OpeningLine line) async {
    if (_setupWizardInFlight || !mounted) return;
    _setupWizardInFlight = true;
    final useStandard =
        line.startFen.trim() == BoardSetupFenSteps.standardStartFen;
    try {
      await Navigator.of(context).push<void>(
        MaterialPageRoute<void>(
          fullscreenDialog: true,
          builder: (_) => BoardSetupWizardScreen(
            kind: useStandard
                ? BoardSetupWizardKind.standardStart
                : BoardSetupWizardKind.fenGuided,
            targetFen: useStandard ? null : line.startFen,
            loadFenWhenDone: !useStandard,
          ),
        ),
      );
      if (!mounted) return;
      await _startLesson(line);
    } finally {
      _setupWizardInFlight = false;
    }
  }

  Future<void> _hint() async {
    final n = ref.read(boardSessionNotifierProvider.notifier);
    await n.postOpeningRaw({'action': 'hint'});
  }

  Future<void> _cancel() async {
    _stopHintRefresh();
    _countdownTimer?.cancel();
    final n = ref.read(boardSessionNotifierProvider.notifier);
    await n.postOpeningRaw({'action': 'cancel'});
    if (mounted) Navigator.of(context).pop();
  }

  Future<void> _checkpointAck() async {
    final n = ref.read(boardSessionNotifierProvider.notifier);
    try {
      await n.postOpeningRaw({'action': 'checkpoint_ack'});
      if (mounted) {
        showGlassSnackBar(context, 'Deska srovnaná — pokračuj');
      }
    } catch (e) {
      if (mounted) {
        showGlassSnackBar(
          context,
          'Deska ještě nesedí — uprav figurky podle návodu',
        );
      }
    }
  }

  static String _squareFromIndex(int index) {
    final col = index % 8;
    final row = index ~/ 8;
    return '${String.fromCharCode(97 + col)}${row + 1}';
  }

  List<String> _checkpointMismatches({
    required List<int>? matrix,
    required List<int>? expected,
  }) {
    if (matrix == null ||
        expected == null ||
        matrix.length < 64 ||
        expected.length < 64) {
      return const [];
    }
    final out = <String>[];
    for (var i = 0; i < 64; i++) {
      if (matrix[i] != expected[i]) {
        final sq = _squareFromIndex(i);
        if (expected[i] == 1 && matrix[i] == 0) {
          out.add('Polož figurku na $sq');
        } else if (expected[i] == 0 && matrix[i] == 1) {
          out.add('Zvedni figurku z $sq');
        } else {
          out.add('Uprav pole $sq');
        }
      }
    }
    return out;
  }

  @override
  Widget build(BuildContext context) {
    final l10n = context.l10n;
    final session = ref.watch(boardSessionNotifierProvider);
    final opening = session.snapshot?.status.openingTraining;
    final matrix = session.snapshot?.status.matrixOccupied;
    if (opening != null) {
      WidgetsBinding.instance.addPostFrameCallback((_) async {
        if (!mounted) return;
        final prevFeedback = _feedback;
        setState(() {
          if (opening.feedback == 'wrong' || opening.feedback == 'illegal') {
            _drillErrors++;
          }
          _feedback = opening.feedback ?? 'none';
          _playerPly = opening.playerPlyIndex ?? 0;
          _playerTotal = opening.playerPlyTotal ?? _playerTotal;
        });
        if (_feedback == 'complete' && prevFeedback != 'complete') {
          await _saveProgressIfNeeded();
        }
        if (opening.setupPhase == true &&
            opening.active != true &&
            _line != null &&
            !_setupWizardInFlight) {
          unawaited(_openSetupWizard(_line!));
        }
        _syncHintRefresh(
          opening.active == true && opening.setupPhase != true,
          opening.feedback == 'checkpoint' || opening.awaitingCheckpointAck == true,
        );
      });
    }

    final isSetupPhase = opening?.setupPhase == true && opening?.active != true;
    final isMistakeHint = _feedback == 'mistake_hint';
    final isCheckpoint =
        _feedback == 'checkpoint' || opening?.awaitingCheckpointAck == true;
    final physicalSynced = opening?.physicalSynced == true;
    final mismatches = isCheckpoint
        ? _checkpointMismatches(
            matrix: matrix,
            expected: opening?.checkpointExpectedOccupied,
          )
        : const <String>[];

    final line = _line;
    final stepComment = line?.commentForPlayerPly(_playerPly);
    final modeLabel = switch (widget.mode) {
      'drill' => 'Drill',
      'timed' => 'Na čas',
      'mirror' => 'Mirror',
      _ => 'Učení',
    };
    return Scaffold(
      appBar: AppBar(
        title: Text(line?.nameForLocale(Localizations.localeOf(context).languageCode) ??
            l10n.learnAppBarTitle),
        actions: [
          if (widget.mode == 'timed')
            Padding(
              padding: const EdgeInsets.only(right: 8),
              child: Center(
                child: Text(
                  '${_timedSecondsLeft}s',
                  style: TextStyle(
                    color: _timedSecondsLeft <= 10
                        ? Theme.of(context).colorScheme.error
                        : null,
                    fontWeight: FontWeight.bold,
                  ),
                ),
              ),
            ),
          IconButton(onPressed: _busy ? null : _cancel, icon: const Icon(Icons.close)),
        ],
      ),
      body: desktopFormDetailBody(
        Padding(
          padding: const EdgeInsets.all(16),
          child: _error != null
              ? Text(_error!, style: const TextStyle(color: Colors.redAccent))
              : line == null
                  ? const Center(child: CircularProgressIndicator())
                  : Column(
                      crossAxisAlignment: CrossAxisAlignment.stretch,
                      children: [
                        Text('ECO ${line.eco} · ${line.side} · $modeLabel',
                            style: Theme.of(context).textTheme.titleSmall),
                        const SizedBox(height: 8),
                        if (!widget.isDrillLike && line.ideaCs != null)
                          Text(line.ideaCs!, style: Theme.of(context).textTheme.bodyLarge),
                        if (!widget.isDrillLike &&
                            stepComment != null &&
                            stepComment.isNotEmpty) ...[
                          const SizedBox(height: 8),
                          Text(stepComment,
                              style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                                    color: Theme.of(context).colorScheme.primary,
                                  )),
                        ],
                        if (widget.isDrillLike) ...[
                          const SizedBox(height: 8),
                          Text('Chyby: $_drillErrors'),
                        ],
                        const SizedBox(height: 16),
                        LinearProgressIndicator(
                          value: _playerTotal == 0
                              ? null
                              : (_playerPly + 1) / _playerTotal,
                        ),
                        const SizedBox(height: 8),
                        Text('Tah hráče ${_playerPly + 1} / $_playerTotal'),
                        Text('Stav: $_feedback'),
                        if (opening?.wrongMoveCount != null &&
                            opening!.wrongMoveCount! > 0)
                          Text('Špatné pokusy: ${opening.wrongMoveCount}'),
                        if (isSetupPhase) ...[
                          const SizedBox(height: 16),
                          Text(
                            opening?.physicalMatch == false
                                ? 'Fyzická deska nesedí se startovní pozicí této linie.'
                                : 'Čekám na potvrzení fyzické desky…',
                            style: Theme.of(context).textTheme.bodyMedium,
                          ),
                          const SizedBox(height: 8),
                          FilledButton(
                            onPressed: (_busy || _setupWizardInFlight || line == null)
                                ? null
                                : () => _openSetupWizard(line),
                            child: const Text('Nastavit desku'),
                          ),
                        ],
                        if (isMistakeHint) ...[
                          const SizedBox(height: 16),
                          Text(
                            'Po 3 chybách — správný tah: '
                            '${opening?.expectedFrom ?? '?'} → ${opening?.expectedTo ?? '?'}',
                            style: TextStyle(
                              color: Theme.of(context).colorScheme.error,
                              fontWeight: FontWeight.w600,
                            ),
                          ),
                          const SizedBox(height: 8),
                          FilledButton(
                            onPressed: _busy ? null : _hint,
                            child: const Text('Zobrazit tah na LED'),
                          ),
                        ],
                        if (isCheckpoint) ...[
                          const SizedBox(height: 16),
                          Text(
                            'Srovnej fyzickou desku s logickou pozicí po tahu soupeře.',
                            style: Theme.of(context).textTheme.bodyMedium,
                          ),
                          const SizedBox(height: 8),
                          if (mismatches.isEmpty && !physicalSynced)
                            const Text('Načítám stav matice…')
                          else if (mismatches.isEmpty)
                            Text(
                              'Deska sedí — můžeš pokračovat.',
                              style: TextStyle(
                                color: Theme.of(context).colorScheme.primary,
                              ),
                            )
                          else
                            ...mismatches.take(8).map(
                                  (m) => Padding(
                                    padding: const EdgeInsets.only(bottom: 4),
                                    child: Row(
                                      crossAxisAlignment: CrossAxisAlignment.start,
                                      children: [
                                        const Icon(Icons.circle, size: 8),
                                        const SizedBox(width: 8),
                                        Expanded(child: Text(m)),
                                      ],
                                    ),
                                  ),
                                ),
                        ],
                        const Spacer(),
                        if (isCheckpoint)
                          FilledButton(
                            onPressed: (_busy || !physicalSynced) ? null : _checkpointAck,
                            child: Text(
                              physicalSynced
                                  ? 'Deska srovnaná — pokračovat'
                                  : 'Nejdřív srovnej desku (${mismatches.length} rozdílů)',
                            ),
                          ),
                        const SizedBox(height: 8),
                        if (!isCheckpoint && !isSetupPhase && !isMistakeHint)
                          OutlinedButton(
                            onPressed: _busy ? null : _hint,
                            child: const Text('LED nápověda'),
                          ),
                        if (_feedback == 'complete')
                          Padding(
                            padding: const EdgeInsets.only(top: 12),
                            child: FilledButton(
                              onPressed: () => Navigator.of(context).pop(),
                              child: const Text('Hotovo'),
                            ),
                          ),
                      ],
                    ),
        ),
      ),
    );
  }
}
