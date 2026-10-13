import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../core/localization/locale_bridge.dart';
import '../../core/services/board_api_exception.dart';
import '../../core/constants/app_environment.dart';
import '../../core/utils/board_setup_fen_steps.dart';
import '../../core/utils/puzzle_uci.dart';
import '../game/state/game_ui_notifier.dart';
import '../../core/utils/user_facing_error_message.dart';
import '../../l10n/app_localizations.dart';
import '../../app_providers.dart';
import '../../core/localization/context_l10n.dart';
import '../../core/widgets/pressable_scale.dart';
import '../connection/board_session_notifier.dart';
import '../connection/board_session_state.dart';

enum BoardSetupWizardKind { standardStart, fenGuided }

enum _WizardPhase { idle, running, validating, completed, failed }

/// Parity iOS `BoardSetupManager` + sheet UI — LED on target field, matrix / figure from snapshot.
class BoardSetupWizardScreen extends ConsumerStatefulWidget {
  const BoardSetupWizardScreen({
    super.key,
    required this.kind,
    this.targetFen,
    this.loadFenWhenDone = true,
    this.puzzleSolution = const [],
    this.puzzleTitle = '',
    this.puzzleRating,
  });

  final BoardSetupWizardKind kind;
  final String? targetFen;
  final bool loadFenWhenDone;

  /// Solver-first line. Fen-guided setup uses the puzzle session, not `new_game`.
  final List<String> puzzleSolution;
  final String puzzleTitle;
  final int? puzzleRating;

  @override
  ConsumerState<BoardSetupWizardScreen> createState() =>
      _BoardSetupWizardScreenState();
}

class _BoardSetupWizardScreenState
    extends ConsumerState<BoardSetupWizardScreen> {
  _WizardPhase _phase = _WizardPhase.idle;
  List<BoardSetupPieceStep> _steps = [];
  int _currentIndex = 0;
  int _occStable = 0;
  int _pieceStable = 0;
  bool _advanceInFlight = false;
  bool _finalizeInFlight = false;
  String? _lastLedRefreshSquare;
  DateTime? _lastLedRefreshAt;
  String? _error;
  Timer? _ledTimer;
  Timer? _matrixTimer;

  BoardSessionNotifier get _sessionN =>
      ref.read(boardSessionNotifierProvider.notifier);

  String get _targetFen {
    if (widget.kind == BoardSetupWizardKind.standardStart) {
      return BoardSetupFenSteps.standardStartFen;
    }
    return widget.targetFen?.trim() ?? '';
  }

  bool get _puzzleSession =>
      widget.kind == BoardSetupWizardKind.fenGuided;

  BoardSetupPieceStep? get _current {
    if (_phase != _WizardPhase.running) return null;
    if (_currentIndex >= _steps.length) return null;
    return _steps[_currentIndex];
  }

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addPostFrameCallback((_) => _bootstrap());
  }

  @override
  void dispose() {
    _ledTimer?.cancel();
    _matrixTimer?.cancel();
    unawaited(_silentCancel());
    super.dispose();
  }

  Future<void> _silentCancel() async {
    if (_phase != _WizardPhase.running && _phase != _WizardPhase.validating) {
      return;
    }
    if (_puzzleSession) {
      try {
        await _sessionN.cancelPuzzle();
      } catch (_) {}
      return;
    }
    try {
      await _sessionN.postSetupTutorialAction('cancel');
    } catch (_) {}
    try {
      await _sessionN.postHintClearBoard();
    } catch (_) {}
  }

  Future<void> _bootstrap() async {
    final strings = appStringsForPrefs(ref.read(prefsRepositoryProvider));
    final fen = _targetFen;
    if (fen.isEmpty) {
      setState(() {
        _phase = _WizardPhase.failed;
        _error = strings.setupWizardErrMissingFen;
      });
      return;
    }
    final session = ref.read(boardSessionNotifierProvider);
    if (!_linked(session)) {
      setState(() {
        _phase = _WizardPhase.failed;
        _error = strings.setupWizardErrConnectBoard;
      });
      return;
    }
    _steps = BoardSetupFenSteps.build(fen);
    if (_steps.isEmpty) {
      setState(() {
        _phase = _WizardPhase.failed;
        _error = strings.setupWizardErrNoSteps;
      });
      return;
    }
    setState(() {
      _phase = _WizardPhase.running;
      _currentIndex = 0;
      _occStable = 0;
      _pieceStable = 0;
      _error = null;
    });
    try {
      if (_puzzleSession) {
        await _sessionN.sendPuzzleToBoard(
          fen: fen,
          solution: widget.puzzleSolution,
          title: widget.puzzleTitle,
        );
        ref.read(gameUiNotifierProvider.notifier).armBoardPuzzle(
              fen: fen,
              title: widget.puzzleTitle,
              solutionUci: widget.puzzleSolution,
              rating: widget.puzzleRating,
            );
        if (AppEnvironment.staging) {
          debugPrint(
            '[staging] puzzle wizard armed plies=${widget.puzzleSolution.length}',
          );
        }
      } else {
        await _sessionN.postSetupTutorialAction('start');
        final tr = ref.read(boardSessionNotifierProvider).transport;
        if (tr == BoardTransport.wifi) await _sessionN.refreshNow();
      }
    } catch (e) {
      if (mounted) {
        setState(() {
          _phase = _WizardPhase.failed;
          _error = userFacingErrorSummary(strings, e);
        });
      }
      return;
    }
    await _applyHighlight();
    _startTimers();
  }

  bool _linked(BoardSessionState s) =>
      (s.transport == BoardTransport.wifi && s.wifiBaseUrl != null) ||
      s.transport == BoardTransport.ble;

  String _setupPieceWord(AppLocalizations l, String pieceCh) {
    final up = pieceCh.toUpperCase() == pieceCh;
    switch (pieceCh.toLowerCase()) {
      case 'k':
        return up ? l.boardSetupWhiteKing : l.boardSetupBlackKing;
      case 'q':
        return up ? l.boardSetupWhiteQueen : l.boardSetupBlackQueen;
      case 'r':
        return up ? l.boardSetupWhiteRook : l.boardSetupBlackRook;
      case 'b':
        return up ? l.boardSetupWhiteBishop : l.boardSetupBlackBishop;
      case 'n':
        return up ? l.boardSetupWhiteKnight : l.boardSetupBlackKnight;
      case 'p':
        return up ? l.boardSetupWhitePawn : l.boardSetupBlackPawn;
      default:
        return l.boardSetupPieceGeneric;
    }
  }

  void _startTimers() {
    _ledTimer?.cancel();
    _matrixTimer?.cancel();
    if (!_puzzleSession) {
      _ledTimer = Timer.periodic(
          const Duration(milliseconds: 900), (_) => unawaited(_refreshLed()));
    }
    _matrixTimer = Timer.periodic(
        const Duration(milliseconds: 400),
        (_) => unawaited(_puzzleSession ? _pollPuzzleAlign() : _pollMatrix()));
  }

  void _stopTimers() {
    _ledTimer?.cancel();
    _ledTimer = null;
    _matrixTimer?.cancel();
    _matrixTimer = null;
  }

  Future<void> _refreshLed() async {
    if (!mounted || _phase != _WizardPhase.running) return;
    final step = _current;
    if (step == null) return;
    final now = DateTime.now();
    if (_lastLedRefreshSquare == step.square &&
        _lastLedRefreshAt != null &&
        now.difference(_lastLedRefreshAt!) <
            const Duration(milliseconds: 850)) {
      return;
    }
    _lastLedRefreshSquare = step.square;
    _lastLedRefreshAt = now;
    try {
      await _sessionN.postHintDestination(step.square);
    } catch (_) {}
  }

  Future<void> _pollPuzzleAlign() async {
    if (!mounted || _phase != _WizardPhase.running || _advanceInFlight) return;
    await _sessionN.refreshNow();
    if (!mounted || _phase != _WizardPhase.running) return;
    final snap = ref.read(boardSessionNotifierProvider).snapshot;
    final puzzle = snap?.status.puzzle;
    if (puzzle?.active == true) {
      _stopTimers();
      setState(() => _phase = _WizardPhase.completed);
      return;
    }
    final matrix = await _sessionN.fetchMatrixOccupiedForWizard();
    final diffs = puzzleOccupancyDiffs(_targetFen, matrix);
    if (diffs.isEmpty && puzzle?.physicalMatch == true) {
      _occStable++;
    } else {
      _occStable = 0;
      final next = diffs.isEmpty ? null : diffs.first.square;
      if (next != null) {
        final idx = _steps.indexWhere(
          (s) => s.square.toLowerCase() == next.toLowerCase(),
        );
        if (idx >= 0 && idx != _currentIndex && mounted) {
          setState(() => _currentIndex = idx);
        }
      }
    }
    if (_occStable >= 2) {
      _advanceInFlight = true;
      _stopTimers();
      try {
        await _sessionN.startPuzzlePlay();
      } catch (_) {}
      await _sessionN.refreshNow();
      if (!mounted) return;
      final active = ref
              .read(boardSessionNotifierProvider)
              .snapshot
              ?.status
              .puzzle
              ?.active ==
          true;
      setState(() {
        _phase = active ? _WizardPhase.completed : _WizardPhase.running;
      });
      _advanceInFlight = false;
      if (!active) _startTimers();
    }
  }

  Future<void> _pollMatrix() async {
    if (!mounted || _phase != _WizardPhase.running || _advanceInFlight) return;
    final step = _current;
    if (step == null) return;
    final idx = BoardSetupFenSteps.matrixIndexForSquare(step.square);
    if (idx == null) return;
    final row = await _sessionN.fetchMatrixOccupiedForWizard();
    if (!mounted || row == null || idx < 0 || idx >= row.length) {
      _occStable = 0;
      return;
    }
    if (row[idx] == 1) {
      _occStable++;
    } else {
      _occStable = 0;
    }
    _tryScheduleAdvance();
  }

  void _consumeSnapshot() {
    if (_puzzleSession) return;
    if (!mounted || _phase != _WizardPhase.running || _advanceInFlight) return;
    final step = _current;
    final snap = ref.read(boardSessionNotifierProvider).snapshot;
    if (step == null || snap == null) return;
    if (_pieceMatches(snap.board, step)) {
      _pieceStable++;
    } else {
      _pieceStable = 0;
    }
    _tryScheduleAdvance();
  }

  bool _pieceMatches(List<List<String>> board, BoardSetupPieceStep step) {
    final idx = BoardSetupFenSteps.boardIndices(step.square);
    if (idx == null) return false;
    final cell = board[idx.row][idx.col].trim();
    if (cell.isEmpty || cell == '.') return false;
    return cell.substring(0, 1) == step.pieceChar;
  }

  void _tryScheduleAdvance() {
    if (_phase != _WizardPhase.running || _advanceInFlight) return;
    final occOk = _occStable >= 2;
    final pieceOk = _pieceStable >= 2;
    if (!occOk && !pieceOk) return;
    unawaited(_advance());
  }

  Future<void> _advance() async {
    if (_advanceInFlight || _phase != _WizardPhase.running) return;
    _advanceInFlight = true;
    _occStable = 0;
    _pieceStable = 0;
    try {
      await _sessionN.postHintClearBoard();
    } catch (_) {}
    _currentIndex++;
    if (_currentIndex >= _steps.length) {
      _stopTimers();
      _advanceInFlight = false;
      await _finalize();
      return;
    }
    await _applyHighlight();
    _advanceInFlight = false;
    if (mounted) setState(() {});
  }

  Future<void> _applyHighlight() async {
    final step = _current;
    if (step == null) return;
    _lastLedRefreshSquare = null;
    _lastLedRefreshAt = null;
    try {
      await _sessionN.postHintDestination(step.square);
    } catch (e) {
      _advanceInFlight = false;
      if (mounted) {
        final l = appStringsForPrefs(ref.read(prefsRepositoryProvider));
        setState(() {
          _phase = _WizardPhase.failed;
          _error = userFacingErrorSummary(l, e);
        });
      }
    }
  }

  Future<void> _finalize() async {
    if (_finalizeInFlight) return;
    _finalizeInFlight = true;
    try {
      if (!mounted) return;
      setState(() => _phase = _WizardPhase.validating);
      final transport = ref.read(boardSessionNotifierProvider).transport;
      final l10n = context.l10n;

      if (widget.kind == BoardSetupWizardKind.standardStart) {
        await Future<void>.delayed(const Duration(milliseconds: 300));
        if (!mounted) return;

        if (transport == BoardTransport.ble) {
          try {
            await _sessionN.postSetupTutorialAction('finish');
            await _sessionN.refreshNow();
            await Future<void>.delayed(const Duration(milliseconds: 220));
            if (!mounted) return;
            final snap = ref.read(boardSessionNotifierProvider).snapshot;
            final stillTutorial = snap?.status.boardSetupTutorial == true;
            if (stillTutorial) {
              setState(() {
                _phase = _WizardPhase.failed;
                _error = l10n.setupWizardErrPhysicalMismatch;
              });
              return;
            }
            setState(() => _phase = _WizardPhase.completed);
            return;
          } catch (e) {
            if (mounted) {
              setState(() {
                _phase = _WizardPhase.failed;
                _error = userFacingErrorSummary(l10n, e);
              });
            }
            return;
          }
        }

        for (var attempt = 1; attempt <= 3; attempt++) {
          try {
            await _sessionN.postSetupTutorialAction('finish');
            if (mounted) setState(() => _phase = _WizardPhase.completed);
            return;
          } catch (e) {
            final is409 = e is BoardApiException && e.statusCode == 409;
            if (is409 && attempt < 3) {
              await Future<void>.delayed(
                Duration(milliseconds: 400 + attempt * 350),
              );
              continue;
            }
            if (mounted) {
              setState(() {
                _phase = _WizardPhase.failed;
                _error = is409
                    ? l10n.setupWizardErrPhysicalMismatch
                    : userFacingErrorSummary(l10n, e);
              });
            }
            return;
          }
        }
      } else if (_puzzleSession) {
        final active = ref
                .read(boardSessionNotifierProvider)
                .snapshot
                ?.status
                .puzzle
                ?.active ==
            true;
        if (!active) {
          final matrix = await _sessionN.fetchMatrixOccupiedForWizard();
          if (puzzleOccupancyDiffs(_targetFen, matrix).isNotEmpty) {
            if (mounted) {
              setState(() {
                _phase = _WizardPhase.failed;
                _error = l10n.setupWizardErrPhysicalMismatch;
              });
            }
            return;
          }
          try {
            await _sessionN.startPuzzlePlay();
          } catch (e) {
            if (mounted) {
              setState(() {
                _phase = _WizardPhase.failed;
                _error = userFacingErrorSummary(l10n, e);
              });
            }
            return;
          }
        }
        if (mounted) setState(() => _phase = _WizardPhase.completed);
      } else {
        try {
          await _sessionN.postSetupTutorialAction('cancel');
        } catch (_) {}
        if (widget.loadFenWhenDone) {
          try {
            await _sessionN.sendPuzzleToBoard(
              fen: _targetFen,
              solution: widget.puzzleSolution,
              title: widget.puzzleTitle,
            );
          } catch (e) {
            if (mounted) {
              final l = appStringsForPrefs(ref.read(prefsRepositoryProvider));
              setState(() {
                _phase = _WizardPhase.failed;
                _error = userFacingErrorSummary(l, e);
              });
            }
            return;
          }
        }
        if (mounted) setState(() => _phase = _WizardPhase.completed);
      }
    } finally {
      _finalizeInFlight = false;
    }
  }

  Future<void> _skipStep() async {
    if (_phase != _WizardPhase.running || _advanceInFlight) return;
    if (_puzzleSession) {
      if (_steps.isEmpty) return;
      setState(() {
        _currentIndex = (_currentIndex + 1) % _steps.length;
      });
      return;
    }
    _occStable = 0;
    _pieceStable = 0;
    await _advance();
  }

  Future<void> _cancelWizard() async {
    _stopTimers();
    if (_puzzleSession) {
      try {
        await _sessionN.cancelPuzzle();
      } catch (_) {}
      ref.read(gameUiNotifierProvider.notifier).exitPuzzleChallenge();
    } else {
      try {
        await _sessionN.postSetupTutorialAction('cancel');
      } catch (_) {}
      try {
        await _sessionN.postHintClearBoard();
      } catch (_) {}
    }
    if (mounted) Navigator.of(context).pop();
  }

  static String _glyphForPiece(String ch) {
    if (ch.isEmpty) return '♟';
    switch (ch) {
      case 'K':
        return '♔';
      case 'k':
        return '♚';
      case 'Q':
        return '♕';
      case 'q':
        return '♛';
      case 'R':
        return '♖';
      case 'r':
        return '♜';
      case 'B':
        return '♗';
      case 'b':
        return '♝';
      case 'N':
        return '♘';
      case 'n':
        return '♞';
      case 'P':
        return '♙';
      case 'p':
        return '♟';
      default:
        return ch;
    }
  }

  @override
  Widget build(BuildContext context) {
    ref.listen<BoardSessionState>(boardSessionNotifierProvider, (prev, next) {
      _consumeSnapshot();
    });

    final step = _current;
    final cs = Theme.of(context).colorScheme;
    final l10n = context.l10n;
    final title = widget.kind == BoardSetupWizardKind.standardStart
        ? l10n.setupWizardTitleStandard
        : l10n.setupWizardTitleFromFen;

    return Scaffold(
      appBar: AppBar(
        title: Text(title),
        leading: IconButton(
          icon: const Icon(Icons.close),
          onPressed: _cancelWizard,
        ),
      ),
      body: ListView(
        padding: const EdgeInsets.all(20),
        children: [
          if (_phase == _WizardPhase.failed) ...[
            Icon(Icons.error_outline, size: 48, color: cs.error),
            const SizedBox(height: 12),
            Text(_error ?? l10n.setupWizardErrGeneric,
                style: TextStyle(color: cs.error)),
            const SizedBox(height: 24),
            PressableScale(
              child: FilledButton(
                onPressed: () => Navigator.pop(context),
                child: Text(l10n.commonClose),
              ),
            ),
          ] else if (_phase == _WizardPhase.completed) ...[
            Icon(Icons.check_circle, size: 56, color: cs.primary),
            const SizedBox(height: 12),
            Text(
              widget.kind == BoardSetupWizardKind.standardStart
                  ? l10n.setupWizardDoneStandard
                  : (widget.loadFenWhenDone
                      ? l10n.setupWizardDoneFenLoaded
                      : l10n.setupWizardDoneNoLoad),
              style: Theme.of(context).textTheme.titleMedium,
            ),
            const SizedBox(height: 24),
            PressableScale(
              child: FilledButton(
                onPressed: () => Navigator.pop(context),
                child: Text(l10n.setupWizardBtnDone),
              ),
            ),
          ] else ...[
            LinearProgressIndicator(
              value: _steps.isEmpty ? 0 : _currentIndex / _steps.length,
            ),
            const SizedBox(height: 8),
            Text(
              l10n.setupWizardStepProgress(
                _steps.isEmpty ? 0 : _currentIndex + 1,
                _steps.length,
              ),
              style: Theme.of(context).textTheme.titleSmall,
            ),
            const SizedBox(height: 20),
            if (step != null) ...[
              Text(
                _glyphForPiece(step.pieceChar),
                textAlign: TextAlign.center,
                style: const TextStyle(fontSize: 72),
              ),
              const SizedBox(height: 8),
              Text(
                l10n.setupWizardPlacePieceOn(step.square.toUpperCase()),
                textAlign: TextAlign.center,
                style: Theme.of(context).textTheme.headlineSmall,
              ),
              Text(
                l10n.setupWizardPieceDetail(
                  _setupPieceWord(l10n, step.pieceChar),
                  step.square.toUpperCase(),
                ),
                textAlign: TextAlign.center,
                style: Theme.of(context)
                    .textTheme
                    .bodyLarge
                    ?.copyWith(color: cs.onSurfaceVariant),
              ),
            ] else if (_phase == _WizardPhase.validating)
              const Center(
                  child: Padding(
                      padding: EdgeInsets.all(24),
                      child: CircularProgressIndicator())),
            const SizedBox(height: 28),
            Text(
              l10n.setupWizardLedHint,
              style: Theme.of(context)
                  .textTheme
                  .bodySmall
                  ?.copyWith(color: cs.outline),
            ),
            const SizedBox(height: 20),
            Row(
              children: [
                Expanded(
                  child: OutlinedButton(
                    onPressed:
                        (_phase == _WizardPhase.running && !_advanceInFlight)
                            ? _skipStep
                            : null,
                    child: Text(l10n.setupWizardSkipStep),
                  ),
                ),
                const SizedBox(width: 12),
                Expanded(
                  child: OutlinedButton(
                    onPressed: _cancelWizard,
                    child: Text(l10n.commonCancel),
                  ),
                ),
              ],
            ),
          ],
        ],
      ),
    );
  }
}
