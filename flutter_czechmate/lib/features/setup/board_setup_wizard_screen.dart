import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../core/services/board_api_exception.dart';
import '../../core/utils/board_setup_fen_steps.dart';
import '../connection/board_session_notifier.dart';
import '../connection/board_session_state.dart';

enum BoardSetupWizardKind { standardStart, fenGuided }

enum _WizardPhase { idle, running, validating, completed, failed }

/// Parita iOS `BoardSetupManager` + sheet UI — LED na cílové pole, matice / figurka ze snapshotu.
class BoardSetupWizardScreen extends ConsumerStatefulWidget {
  const BoardSetupWizardScreen({
    super.key,
    required this.kind,
    this.targetFen,
    this.loadFenWhenDone = true,
  });

  final BoardSetupWizardKind kind;
  final String? targetFen;
  final bool loadFenWhenDone;

  @override
  ConsumerState<BoardSetupWizardScreen> createState() => _BoardSetupWizardScreenState();
}

class _BoardSetupWizardScreenState extends ConsumerState<BoardSetupWizardScreen> {
  _WizardPhase _phase = _WizardPhase.idle;
  List<BoardSetupPieceStep> _steps = [];
  int _currentIndex = 0;
  int _occStable = 0;
  int _pieceStable = 0;
  bool _advanceInFlight = false;
  String? _error;
  Timer? _ledTimer;
  Timer? _matrixTimer;

  BoardSessionNotifier get _sessionN => ref.read(boardSessionNotifierProvider.notifier);

  String get _targetFen {
    if (widget.kind == BoardSetupWizardKind.standardStart) {
      return BoardSetupFenSteps.standardStartFen;
    }
    return widget.targetFen?.trim() ?? '';
  }

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
    if (_phase == _WizardPhase.running || _phase == _WizardPhase.validating) {
      try {
        await _sessionN.postSetupTutorialAction('cancel');
      } catch (_) {}
      try {
        await _sessionN.postHintClearBoard();
      } catch (_) {}
    }
  }

  Future<void> _bootstrap() async {
    final fen = _targetFen;
    if (fen.isEmpty) {
      setState(() {
        _phase = _WizardPhase.failed;
        _error = 'Chybí FEN.';
      });
      return;
    }
    final session = ref.read(boardSessionNotifierProvider);
    if (!_linked(session)) {
      setState(() {
        _phase = _WizardPhase.failed;
        _error = 'Připoj desku přes Wi‑Fi nebo Bluetooth (mock režim tutoriál nepodporuje).';
      });
      return;
    }
    _steps = BoardSetupFenSteps.build(fen);
    if (_steps.isEmpty) {
      setState(() {
        _phase = _WizardPhase.failed;
        _error = 'Z FEN nešlo sestavit žádné kroky.';
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
      await _sessionN.postSetupTutorialAction('start');
      final tr = ref.read(boardSessionNotifierProvider).transport;
      if (tr == BoardTransport.wifi) await _sessionN.refreshNow();
    } catch (e) {
      if (mounted) {
        setState(() {
          _phase = _WizardPhase.failed;
          _error = '$e';
        });
      }
      return;
    }
    await _applyHighlight();
    _startTimers();
  }

  bool _linked(BoardSessionState s) =>
      (s.transport == BoardTransport.wifi && s.wifiBaseUrl != null) || s.transport == BoardTransport.ble;

  void _startTimers() {
    _ledTimer?.cancel();
    _matrixTimer?.cancel();
    _ledTimer = Timer.periodic(const Duration(milliseconds: 600), (_) => unawaited(_refreshLed()));
    _matrixTimer = Timer.periodic(const Duration(milliseconds: 400), (_) => unawaited(_pollMatrix()));
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
    try {
      await _sessionN.postHintDestination(step.square);
    } catch (_) {}
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
    try {
      await _sessionN.postHintDestination(step.square);
    } catch (e) {
      _advanceInFlight = false;
      if (mounted) {
        setState(() {
          _phase = _WizardPhase.failed;
          _error = '$e';
        });
      }
    }
  }

  Future<void> _finalize() async {
    setState(() => _phase = _WizardPhase.validating);
    if (widget.kind == BoardSetupWizardKind.standardStart) {
      await Future.delayed(const Duration(milliseconds: 300));
      for (var attempt = 1; attempt <= 5; attempt++) {
        try {
          await _sessionN.postSetupTutorialAction('finish');
          if (mounted) setState(() => _phase = _WizardPhase.completed);
          return;
        } catch (e) {
          final is409 = e is BoardApiException && e.statusCode == 409;
          if (is409 && attempt < 5) {
            await Future.delayed(const Duration(milliseconds: 400));
            continue;
          }
          if (mounted) {
            setState(() {
              _phase = _WizardPhase.failed;
              _error = '$e';
            });
          }
          return;
        }
      }
    } else {
      try {
        await _sessionN.postSetupTutorialAction('cancel');
      } catch (_) {}
      if (widget.loadFenWhenDone) {
        try {
          await _sessionN.sendPuzzleFenToBoard(_targetFen);
        } catch (e) {
          if (mounted) {
            setState(() {
              _phase = _WizardPhase.failed;
              _error = '$e';
            });
          }
          return;
        }
      }
      if (mounted) setState(() => _phase = _WizardPhase.completed);
    }
  }

  Future<void> _skipStep() async {
    if (_phase != _WizardPhase.running || _advanceInFlight) return;
    _occStable = 0;
    _pieceStable = 0;
    await _advance();
  }

  Future<void> _cancelWizard() async {
    _stopTimers();
    try {
      await _sessionN.postSetupTutorialAction('cancel');
    } catch (_) {}
    try {
      await _sessionN.postHintClearBoard();
    } catch (_) {}
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
    final title = widget.kind == BoardSetupWizardKind.standardStart
        ? 'Základní postavení'
        : 'Postavení z FEN';

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
            Text(_error ?? 'Chyba', style: TextStyle(color: cs.error)),
            const SizedBox(height: 24),
            FilledButton(onPressed: () => Navigator.pop(context), child: const Text('Zavřít')),
          ] else if (_phase == _WizardPhase.completed) ...[
            Icon(Icons.check_circle, size: 56, color: cs.primary),
            const SizedBox(height: 12),
            Text(
              widget.kind == BoardSetupWizardKind.standardStart
                  ? 'Deska potvrdila základní postavení.'
                  : (widget.loadFenWhenDone ? 'Pozice byla nahrána na desku.' : 'Průvodce dokončen.'),
              style: Theme.of(context).textTheme.titleMedium,
            ),
            const SizedBox(height: 24),
            FilledButton(onPressed: () => Navigator.pop(context), child: const Text('Hotovo')),
          ] else ...[
            LinearProgressIndicator(
              value: _steps.isEmpty ? 0 : _currentIndex / _steps.length,
            ),
            const SizedBox(height: 8),
            Text(
              'Krok ${_steps.isEmpty ? 0 : _currentIndex + 1} / ${_steps.length}',
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
                'Polož figurku na ${step.square.toUpperCase()}',
                textAlign: TextAlign.center,
                style: Theme.of(context).textTheme.headlineSmall,
              ),
              Text(
                step.label,
                textAlign: TextAlign.center,
                style: Theme.of(context).textTheme.bodyLarge?.copyWith(color: cs.onSurfaceVariant),
              ),
            ] else if (_phase == _WizardPhase.validating)
              const Center(child: Padding(padding: EdgeInsets.all(24), child: CircularProgressIndicator())),
            const SizedBox(height: 28),
            Text(
              'LED na desce ukazuje cílové pole. Postup se potvrdí senzorem matice nebo správnou figurkou ve snapshotu.',
              style: Theme.of(context).textTheme.bodySmall?.copyWith(color: cs.outline),
            ),
            const SizedBox(height: 20),
            Row(
              children: [
                Expanded(
                  child: OutlinedButton(
                    onPressed: (_phase == _WizardPhase.running && !_advanceInFlight) ? _skipStep : null,
                    child: const Text('Přeskočit krok'),
                  ),
                ),
                const SizedBox(width: 12),
                Expanded(
                  child: OutlinedButton(
                    onPressed: _cancelWizard,
                    child: const Text('Zrušit'),
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
