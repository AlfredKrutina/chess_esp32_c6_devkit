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
import 'opening_common_mistake.dart';
import 'opening_feedback_l10n.dart';
import 'opening_lesson_board.dart';
import 'opening_rationale.dart';

class OpeningTrainerScreen extends ConsumerStatefulWidget {
  const OpeningTrainerScreen({
    super.key,
    required this.lineId,
    this.mode = 'learn',
    this.opponentMode = 'physical',
  });

  final String lineId;
  final String mode;
  final String opponentMode;

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

  bool _isCs() {
    if (!mounted) return true;
    return Localizations.localeOf(context).languageCode.startsWith('cs');
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
        showGlassSnackBar(
          context,
          _isCs() ? 'Čas vypršel' : 'Time is up',
        );
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
        setState(() => _error = _isCs() ? 'Linie nenalezena' : 'Opening not found');
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
          .postOpeningAction(line.toStartPayload(
            mode: widget.mode,
            opponentMode: widget.opponentMode,
          ));
      final opening = ref
          .read(boardSessionNotifierProvider)
          .snapshot
          ?.status
          .openingTraining;
      if (opening?.setupPhase == true) {
        if (mounted) {
          showGlassSnackBar(
            context,
            _isCs()
                ? 'Deska nesedí se startovní pozicí — nastav figurky'
                : 'Board does not match the start — set up the pieces',
          );
        }
        return;
      }
      _startTimedCountdown();
      if (mounted) {
        final msg = switch (widget.mode) {
          'drill' => _isCs()
              ? 'Drill — táhni bez komentářů v textu'
              : 'Drill — play without move comments',
          'timed' => _isCs()
              ? 'Na čas $_timedLimitSec s — začni!'
              : 'Timed $_timedLimitSec s — go!',
          _ => _isCs() ? 'Lekce spuštěna — táhni na desce' : 'Lesson started — play on the board',
        };
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
        showGlassSnackBar(
          context,
          _isCs() ? 'Deska srovnaná — pokračuj' : 'Board synced — continue',
        );
      }
    } catch (e) {
      if (mounted) {
        showGlassSnackBar(
          context,
          _isCs()
              ? 'Deska ještě nesedí — uprav figurky podle návodu'
              : 'Board still mismatched — adjust pieces per guide',
        );
      }
    }
  }

  static String _squareFromIndex(int index) {
    final col = index % 8;
    final row = index ~/ 8;
    return '${String.fromCharCode(97 + col)}${row + 1}';
  }

  List<String> _checkpointDiffSquares({
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
        out.add(_squareFromIndex(i));
      }
    }
    return out;
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
          out.add(_isCs() ? 'Polož figurku na $sq' : 'Place a piece on $sq');
        } else if (expected[i] == 0 && matrix[i] == 1) {
          out.add(_isCs() ? 'Zvedni figurku z $sq' : 'Remove piece from $sq');
        } else {
          out.add(_isCs() ? 'Uprav pole $sq' : 'Fix square $sq');
        }
      }
    }
    return out;
  }

  @override
  Widget build(BuildContext context) {
    final l10n = context.l10n;
    final locale = Localizations.localeOf(context).languageCode;
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
          (opening.active == true || opening.feedback == 'opponent_turn') &&
              opening.setupPhase != true,
          opening.feedback == 'checkpoint' || opening.awaitingCheckpointAck == true,
        );
      });
    }

    final isSetupPhase = opening?.setupPhase == true && opening?.active != true;
    final isOpponentTurn = _feedback == 'opponent_turn' ||
        opening?.awaitingOpponentPhysical == true;
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
    final checkpointSquares = isCheckpoint
        ? _checkpointDiffSquares(
            matrix: matrix,
            expected: opening?.checkpointExpectedOccupied,
          )
        : const <String>[];

    final line = _line;
    final stepComment = line?.commentForPlayerPly(_playerPly, locale);
    final idea = line?.ideaForLocale(locale);
    final showRationale = !widget.isDrillLike &&
        _playerPly == 0 &&
        line?.rationale != null;
    final modeLabel = OpeningFeedbackL10n.modeLabel(locale, widget.mode);
    final from = opening?.expectedFrom ?? '?';
    final to = opening?.expectedTo ?? '?';
    final instruction = OpeningFeedbackL10n.moveLabel(
      locale: locale,
      playerPlyIndex: _playerPly,
      playerPlyTotal: _playerTotal,
      from: from,
      to: to,
      feedback: _feedback,
      drillLike: widget.isDrillLike,
      isSetupPhase: isSetupPhase,
      isCheckpoint: isCheckpoint,
      physicalSynced: physicalSynced,
      mismatchCount: mismatches.length,
      physicalMatch: opening?.physicalMatch ?? true,
      opponentMode: widget.opponentMode,
    );
    final opponentHint =
        OpeningFeedbackL10n.opponentModeHint(locale, widget.opponentMode);
    final mistakeHint = openingCommonMistakeHint(
      mistakes: line?.commonMistakes ?? const [],
      plyIndex: opening?.plyIndex,
      lastWrongUci: opening?.lastWrongUci,
      locale: locale,
    );
    final showMistakePedagogy = (_feedback == 'wrong' || _feedback == 'illegal') &&
        mistakeHint != null &&
        mistakeHint.isNotEmpty;

    return Scaffold(
      appBar: AppBar(
        title: Text(line?.nameForLocale(locale) ?? l10n.learnAppBarTitle),
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
                        OpeningLessonBoardPreview(
                          line: line,
                          hintFrom: opening?.expectedFrom,
                          hintTo: opening?.expectedTo,
                          checkpointSquares: checkpointSquares,
                        ),
                        const SizedBox(height: 8),
                        Text(
                          instruction,
                          style: Theme.of(context).textTheme.titleMedium?.copyWith(
                                fontWeight: FontWeight.w600,
                              ),
                        ),
                        if (showMistakePedagogy) ...[
                          const SizedBox(height: 8),
                          Text(
                            mistakeHint!,
                            style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                                  color: Theme.of(context).colorScheme.tertiary,
                                  fontWeight: FontWeight.w600,
                                ),
                          ),
                        ],
                        if (!widget.isDrillLike && idea != null && idea.isNotEmpty) ...[
                          const SizedBox(height: 8),
                          Text(idea, style: Theme.of(context).textTheme.bodyLarge),
                        ],
                        if (showRationale) ...[
                          const SizedBox(height: 8),
                          OpeningRationalePanel(
                            rationale: line.rationale!,
                            locale: locale,
                            compact: true,
                          ),
                        ],
                        if (!widget.isDrillLike &&
                            _playerPly > 0 &&
                            stepComment != null &&
                            stepComment.isNotEmpty) ...[
                          const SizedBox(height: 8),
                          Text(
                            stepComment,
                            style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                                  color: Theme.of(context).colorScheme.primary,
                                ),
                          ),
                        ],
                        if (widget.isDrillLike) ...[
                          const SizedBox(height: 8),
                          Text(_isCs() ? 'Chyby: $_drillErrors' : 'Errors: $_drillErrors'),
                        ],
                        const SizedBox(height: 16),
                        LinearProgressIndicator(
                          value: _playerTotal == 0
                              ? null
                              : (_playerPly + 1) / _playerTotal,
                        ),
                        const SizedBox(height: 8),
                        Text(
                          _isCs()
                              ? 'Tah hráče ${_playerPly + 1} / $_playerTotal'
                              : 'Your move ${_playerPly + 1} / $_playerTotal',
                        ),
                        if (opening?.wrongMoveCount != null &&
                            opening!.wrongMoveCount! > 0)
                          Text(
                            _isCs()
                                ? 'Špatné pokusy: ${opening.wrongMoveCount}'
                                : 'Wrong attempts: ${opening.wrongMoveCount}',
                          ),
                        if (isOpponentTurn && opponentHint != null) ...[
                          const SizedBox(height: 8),
                          Text(
                            opponentHint,
                            style: Theme.of(context).textTheme.bodySmall,
                          ),
                        ],
                        if (isSetupPhase) ...[
                          const SizedBox(height: 16),
                          FilledButton(
                            onPressed: (_busy || _setupWizardInFlight)
                                ? null
                                : () => _openSetupWizard(line),
                            child: Text(_isCs() ? 'Nastavit desku' : 'Set up board'),
                          ),
                        ],
                        if (isMistakeHint) ...[
                          const SizedBox(height: 16),
                          FilledButton(
                            onPressed: _busy ? null : _hint,
                            child: Text(_isCs() ? 'Zobrazit tah na LED' : 'Show move on LED'),
                          ),
                        ],
                        if (isCheckpoint) ...[
                          const SizedBox(height: 16),
                          if (mismatches.isEmpty && !physicalSynced)
                            Text(_isCs() ? 'Načítám stav matice…' : 'Loading matrix state…')
                          else if (mismatches.isEmpty)
                            Text(
                              _isCs() ? 'Deska sedí — můžeš pokračovat.' : 'Board matches — you can continue.',
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
                                  ? (_isCs() ? 'Deska srovnaná — pokračovat' : 'Synced — continue')
                                  : (_isCs()
                                      ? 'Nejdřív srovnej desku (${mismatches.length} rozdílů)'
                                      : 'Sync board first (${mismatches.length} diffs)'),
                            ),
                          ),
                        const SizedBox(height: 8),
                        if (!isCheckpoint &&
                            !isSetupPhase &&
                            !isMistakeHint &&
                            !isOpponentTurn &&
                            _feedback != 'complete')
                          OutlinedButton(
                            onPressed: _busy ? null : _hint,
                            child: Text(_isCs() ? 'LED nápověda' : 'LED hint'),
                          ),
                        if (_feedback == 'complete')
                          Padding(
                            padding: const EdgeInsets.only(top: 12),
                            child: FilledButton(
                              onPressed: () => Navigator.of(context).pop(),
                              child: Text(_isCs() ? 'Hotovo' : 'Done'),
                            ),
                          ),
                      ],
                    ),
        ),
      ),
    );
  }
}
