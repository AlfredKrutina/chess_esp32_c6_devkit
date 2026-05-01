import 'dart:async';

import 'package:chess/chess.dart' as ch;
import 'package:flutter/services.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../../core/localization/locale_bridge.dart';
import '../../../core/models/game_snapshot.dart';
import '../../../core/models/puzzle_challenge_state.dart';
import '../../../core/utils/puzzle_uci.dart';
import '../../../core/services/board_api_exception.dart';
import '../../../core/utils/fen_board_parser.dart';
import '../../../core/utils/fen_from_board.dart';
import '../../connection/board_session_notifier.dart';
import '../../connection/board_session_state.dart';
import 'game_ui_state.dart';

import '../../../app_providers.dart';
import '../../../l10n/app_localizations.dart';

final gameUiNotifierProvider =
    StateNotifierProvider<GameUiNotifier, GameUiState>((ref) {
  final prefs = ref.read(prefsRepositoryProvider);
  return GameUiNotifier(
    ref,
    prefs.boardFlipped,
    prefs.showCoordinates,
    prefs.remoteMovesEnabled,
    prefs.boardStyleRaw,
    prefs.boardZoomStorage,
    prefs.moveAnimationsEnabled,
    layoutMode: prefs.layoutMode,
    hintDepth: prefs.hintDepth,
    moveEvaluationEnabled: prefs.moveEvaluationEnabled,
    learningMode: prefs.learningModeEnabled,
  );
});

class GameUiNotifier extends StateNotifier<GameUiState> {
  GameUiNotifier(
    this._ref,
    bool flipped,
    bool coords,
    bool remote,
    String style,
    double zoom,
    bool anims, {
    String layoutMode = 'standard',
    int hintDepth = 10,
    bool moveEvaluationEnabled = false,
    bool learningMode = false,
  }) : super(GameUiState(
          boardFlipped: flipped,
          showCoordinates: coords,
          remoteMovesEnabled: remote,
          boardStyleRaw: style,
          boardZoomStorage: zoom,
          moveAnimationsEnabled: anims,
          layoutMode: layoutMode,
          hintDepth: hintDepth,
          moveEvaluationEnabled: moveEvaluationEnabled,
          learningMode: learningMode,
        ));

  final Ref _ref;

  AppLocalizations get _l10n =>
      appStringsForPrefs(_ref.read(prefsRepositoryProvider));

  Timer? _transientTimer;
  Timer? _flashTimer;
  Timer? _hintTimer;
  Timer? _puzzleTintTimer;
  ch.Chess? _sandboxChess;
  String? _remoteFrom;
  /// Blocks sandbox moves while puzzle success/failure animation and reset run (allows clean retries).
  bool _puzzleFeedbackBusy = false;

  @override
  void dispose() {
    _transientTimer?.cancel();
    _flashTimer?.cancel();
    _hintTimer?.cancel();
    _puzzleTintTimer?.cancel();
    super.dispose();
  }

  void _moveFeedback({
    bool invalid = false,
    bool successMove = false,
    bool selectionTap = false,
  }) {
    final p = _ref.read(prefsRepositoryProvider);
    if (!p.hapticsEnabled && !p.soundEffectsEnabled) return;
    if (p.hapticsEnabled) {
      if (invalid) {
        HapticFeedback.heavyImpact();
      } else if (successMove) {
        HapticFeedback.lightImpact();
      } else if (selectionTap) {
        HapticFeedback.selectionClick();
      }
    }
    if (p.soundEffectsEnabled) {
      if (invalid) {
        SystemSound.play(SystemSoundType.alert);
      } else if (successMove || selectionTap) {
        SystemSound.play(SystemSoundType.click);
      }
    }
  }

  void toggleControlPanelExpanded() {
    state =
        state.copyWith(isControlPanelExpanded: !state.isControlPanelExpanded);
  }

  void setControlPanelExpanded(bool expanded) {
    if (state.isControlPanelExpanded == expanded) return;
    state = state.copyWith(isControlPanelExpanded: expanded);
  }

  Future<void> setLearningMode(bool v) async {
    await _ref.read(prefsRepositoryProvider).setLearningModeEnabled(v);
    state = state.copyWith(learningMode: v);
  }

  Future<void> setMoveEvaluationEnabled(bool v) async {
    await _ref.read(prefsRepositoryProvider).setMoveEvaluationEnabled(v);
    state = state.copyWith(moveEvaluationEnabled: v);
  }

  Future<void> setHintDepthApp(int d) async {
    final v = d.clamp(1, 18);
    await _ref.read(prefsRepositoryProvider).setHintDepth(v);
    state = state.copyWith(hintDepth: v);
  }

  void showTransientBoardMessage(String message) {
    _transientTimer?.cancel();
    state = state.copyWith(transientBoardMessage: message);
    _transientTimer = Timer(const Duration(seconds: 4), () {
      state = state.copyWith(clearTransient: true);
    });
  }

  void _flashInvalidMove(String? from, String? to) {
    _flashTimer?.cancel();
    state = state.copyWith(invalidFlashFrom: from, invalidFlashTo: to);
    _flashTimer = Timer(const Duration(milliseconds: 600), () {
      state = state.copyWith(clearInvalidFlash: true);
    });
  }

  void showHintOverlay(String from, String to) {
    _hintTimer?.cancel();
    state = state.copyWith(
      hintFrom: from.toLowerCase(),
      hintTo: to.toLowerCase(),
      clearSelection: true,
    );
    _hintTimer = Timer(const Duration(seconds: 5), () {
      state = state.copyWith(clearHint: true);
    });
  }

  void sandboxUndo() {
    if (_puzzleFeedbackBusy) return;
    final chess = _sandboxChess;
    if (chess == null || state.sandboxUndoStack.isEmpty) return;
    final stack = List<String>.from(state.sandboxUndoStack);
    final prev = stack.removeLast();
    if (!chess.load(prev)) return;
    final puzzle = state.puzzleChallenge;
    final PuzzleChallengeState? nextChallenge = puzzle == null
        ? null
        : (puzzle.playedUci.isEmpty
            ? puzzle
            : puzzle.copyWith(
                playedUci: puzzle.playedUci
                    .sublist(0, puzzle.playedUci.length - 1),
              ));
    state = state.copyWith(
      sandboxFenOverride: chess.generate_fen(),
      sandboxUndoStack: stack,
      sandboxUndoCount: stack.length,
      clearSelection: true,
      clearMessage: true,
      puzzleChallenge: nextChallenge,
    );
  }

  Future<void> exitSandboxAndRefresh() async {
    exitSandbox();
    await _ref.read(boardSessionNotifierProvider.notifier).refreshNow();
  }

  void toggleFlip() {
    final next = !state.boardFlipped;
    state = state.copyWith(boardFlipped: next);
    _ref.read(prefsRepositoryProvider).setBoardFlipped(next);
  }

  /// Alias matching iOS parity naming used in SettingsScreen.
  void toggleFlipped() => toggleFlip();

  void toggleCoordinates() {
    final next = !state.showCoordinates;
    state = state.copyWith(showCoordinates: next);
    _ref.read(prefsRepositoryProvider).setShowCoordinates(next);
  }

  void toggleRemoteMoves() {
    final next = !state.remoteMovesEnabled;
    state = state.copyWith(remoteMovesEnabled: next);
    _ref.read(prefsRepositoryProvider).setRemoteMovesEnabled(next);
  }

  void toggleMoveAnimations() {
    final next = !state.moveAnimationsEnabled;
    state = state.copyWith(moveAnimationsEnabled: next);
    _ref.read(prefsRepositoryProvider).setMoveAnimationsEnabled(next);
  }

  void setBoardStyle(String style) {
    state = state.copyWith(boardStyleRaw: style);
    _ref.read(prefsRepositoryProvider).setBoardStyleRaw(style);
  }

  void setBoardZoom(double zoom) {
    final z = zoom.clamp(0.7, 1.5);
    state = state.copyWith(boardZoomStorage: z);
    _ref.read(prefsRepositoryProvider).setBoardZoomStorage(z);
  }

  void setLayoutMode(String mode) {
    final m = mode == 'wide' ? 'standard' : mode;
    state = state.copyWith(layoutMode: m);
    _ref.read(prefsRepositoryProvider).setLayoutMode(m);
  }

  /// Parita iOS „Obnovit výchozí zobrazení desky“.
  Future<void> resetBoardDisplayDefaults() async {
    final p = _ref.read(prefsRepositoryProvider);
    await p.setBoardStyleRaw('wooden');
    await p.setLayoutMode('standard');
    await p.setBoardZoomStorage(1.0);
    await p.setBoardFlipped(false);
    await p.setShowCoordinates(true);
    await p.setMoveAnimationsEnabled(true);
    state = state.copyWith(
      boardStyleRaw: 'wooden',
      layoutMode: 'standard',
      boardZoomStorage: 1.0,
      boardFlipped: false,
      showCoordinates: true,
      moveAnimationsEnabled: true,
    );
  }

  void enterSandbox(GameSnapshot? snap) {
    // Konflikt s dvojitým výběrem (remote) / náhledem historie — jinak sandbox ukazuje špatnou pozici.
    _remoteFrom = null;
    state = state.copyWith(
      sandboxMode: true,
      clearPuzzleChallenge: true,
      clearPuzzleTint: true,
      clearPuzzleSnack: true,
      clearMessage: true,
      clearHistoryIndex: true,
      clearPromotion: true,
      clearSelection: true,
    );
    if (snap != null) {
      _loadSandbox(snap);
      return;
    }
    final chess = ch.Chess();
    _sandboxChess = chess;
    state = state.copyWith(
      sandboxFenOverride: chess.generate_fen(),
      clearSelection: true,
      sandboxUndoStack: const [],
      sandboxUndoCount: 0,
    );
  }

  /// Parita `loadPuzzlePosition(fromFEN:)` — náhled puzzlu v sandboxu z libovolného FEN.
  void enterSandboxFromCustomFen(String fenRaw) {
    final fen = fenRaw.trim();
    if (fen.isEmpty) return;
    _remoteFrom = null;
    state = state.copyWith(
      sandboxMode: true,
      clearPuzzleChallenge: true,
      clearPuzzleTint: true,
      clearPuzzleSnack: true,
      clearMessage: true,
      clearHistoryIndex: true,
      clearPromotion: true,
      clearSelection: true,
    );
    final chess = ch.Chess();
    if (!chess.load(fen)) {
      state = state.copyWith(sandboxMessage: _l10n.gameSandboxCouldNotLoadFen);
      return;
    }
    _sandboxChess = chess;
    state = state.copyWith(
      sandboxFenOverride: chess.generate_fen(),
      clearMessage: true,
      clearSelection: true,
      sandboxUndoStack: const [],
      sandboxUndoCount: 0,
    );
  }

  void exitSandbox() {
    _puzzleTintTimer?.cancel();
    _sandboxChess = null;
    _remoteFrom = null;
    state = state.copyWith(
      sandboxMode: false,
      clearPuzzleChallenge: true,
      clearPuzzleTint: true,
      clearPuzzleSnack: true,
      clearSelection: true,
      clearSandboxFen: true,
      clearPromotion: true,
      clearMessage: true,
      clearHint: true,
      clearHistoryIndex: true,
      sandboxUndoStack: const [],
      sandboxUndoCount: 0,
    );
  }

  /// Z puzzlu / sandboxu zpět na živou partii podle snapshotu (např. po „Zkusit na obrazovce“).
  Future<void> returnToLiveGame() async {
    exitSandbox();
    await _ref.read(boardSessionNotifierProvider.notifier).refreshNow();
  }

  /// Puzzle bez vyhodnocování — jen zavře kontrolu, pozice zůstane.
  void exitPuzzleChallenge() {
    _puzzleTintTimer?.cancel();
    state = state.copyWith(
      clearPuzzleChallenge: true,
      clearPuzzleTint: true,
      clearPuzzleSnack: true,
    );
  }

  void clearPuzzleSnack() {
    if (state.puzzleSnackText == null) return;
    state = state.copyWith(clearPuzzleSnack: true);
  }

  /// Načte pozici a zapne kontrolu řešení podle UCI linky (prázdná = jen sandbox).
  void enterPuzzleChallenge({
    required String fen,
    required String title,
    required List<String> solutionUci,
    int? rating,
  }) {
    final fenTrim = fen.trim();
    if (fenTrim.isEmpty) return;
    _remoteFrom = null;
    _puzzleTintTimer?.cancel();
    state = state.copyWith(
      sandboxMode: true,
      clearMessage: true,
      clearHistoryIndex: true,
      clearPromotion: true,
      clearSelection: true,
      clearPuzzleTint: true,
      clearPuzzleSnack: true,
    );
    final chess = ch.Chess();
    if (!chess.load(fenTrim)) {
      state = state.copyWith(sandboxMessage: _l10n.gameSandboxCouldNotLoadFen);
      return;
    }
    _sandboxChess = chess;
    final normalized = solutionUci
        .map((e) => e.trim().toLowerCase())
        .where((e) => e.isNotEmpty)
        .toList();
    final challenge = PuzzleChallengeState(
      title: title,
      startFen: chess.generate_fen(),
      solutionUci: normalized,
      rating: rating,
    );
    state = state.copyWith(
      puzzleChallenge: challenge,
      sandboxFenOverride: chess.generate_fen(),
      sandboxUndoStack: const [],
      sandboxUndoCount: 0,
      clearSelection: true,
      puzzleBoardTint: 0,
    );
  }

  Future<void> toggleSandbox(GameSnapshot? snap) async {
    if (state.sandboxMode) {
      exitSandbox();
      await _ref.read(boardSessionNotifierProvider.notifier).refreshNow();
    } else {
      enterSandbox(snap);
    }
  }

  void setHistoryReviewIndex(int? index, GameSnapshot? snap) {
    if (index == null ||
        snap == null ||
        index < 0 ||
        index >= snap.history.moves.length) {
      state = state.copyWith(clearHistoryIndex: true);
      return;
    }
    state = state.copyWith(historyReviewMoveIndex: index, clearSelection: true);
  }

  void _loadSandbox(GameSnapshot snap) {
    final fen = fenFromSnapshot(snap);
    final chess = ch.Chess();
    if (!chess.load(fen)) {
      state = state.copyWith(sandboxMessage: _l10n.gameSandboxCouldNotLoadFen);
      return;
    }
    _sandboxChess = chess;
    state = state.copyWith(
      clearPuzzleChallenge: true,
      clearPuzzleTint: true,
      clearPuzzleSnack: true,
      sandboxFenOverride: chess.generate_fen(),
      clearMessage: true,
      clearSelection: true,
      sandboxUndoStack: const [],
      sandboxUndoCount: 0,
    );
  }

  void _notePuzzleMove(String from, String to, String? promotionLower) {
    final puzzle = state.puzzleChallenge;
    if (puzzle == null || !puzzle.hasVerifier) return;
    if (_puzzleFeedbackBusy) return;

    final uci = sandboxMoveToUci(from, to, promotionLower);
    final played = [...puzzle.playedUci, uci];
    final sol = puzzle.solutionUci;
    final n = played.length;

    for (var i = 0; i < n && i < sol.length; i++) {
      if (played[i].toLowerCase().trim() != sol[i].toLowerCase().trim()) {
        state =
            state.copyWith(puzzleChallenge: puzzle.copyWith(playedUci: played));
        unawaited(_handlePuzzleFailure());
        return;
      }
    }

    if (n < sol.length) {
      state =
          state.copyWith(puzzleChallenge: puzzle.copyWith(playedUci: played));
      return;
    }

    if (n == sol.length) {
      state =
          state.copyWith(puzzleChallenge: puzzle.copyWith(playedUci: played));
      unawaited(_handlePuzzleSuccess());
      return;
    }

    state = state.copyWith(puzzleChallenge: puzzle.copyWith(playedUci: played));
    unawaited(_handlePuzzleFailure());
  }

  void _resetPuzzlePositionOnly() {
    final puzzle = state.puzzleChallenge;
    if (puzzle == null) return;
    final chess = ch.Chess();
    if (!chess.load(puzzle.startFen)) return;
    _sandboxChess = chess;
    state = state.copyWith(
      sandboxFenOverride: chess.generate_fen(),
      puzzleChallenge: puzzle.copyWith(playedUci: const []),
      sandboxUndoStack: const [],
      sandboxUndoCount: 0,
      clearSelection: true,
    );
  }

  void _runTintPulse(bool green) {
    _puzzleTintTimer?.cancel();
    var step = 0;
    const total = 18;
    const halfSteps = 9;
    _puzzleTintTimer = Timer.periodic(const Duration(milliseconds: 42), (t) {
      step++;
      final intensity = step <= halfSteps
          ? step / 9.0
          : (total - step) / 9.0;
      state = state.copyWith(
        puzzleBoardTint: intensity.clamp(0.0, 1.0),
        puzzleBoardTintGreen: green,
      );
      if (step >= total) {
        t.cancel();
        state = state.copyWith(clearPuzzleTint: true);
      }
    });
  }

  Future<void> _handlePuzzleSuccess() async {
    if (_puzzleFeedbackBusy) return;
    _puzzleFeedbackBusy = true;
    try {
      _runTintPulse(true);
      final prefs = _ref.read(prefsRepositoryProvider);
      final rating = state.puzzleChallenge?.rating;
      final delta = await prefs.applyPuzzleSolveElo(puzzleRating: rating);
      await prefs.recordPuzzleActivity(solved: true);
      _ref.invalidate(prefsRepositoryProvider);
      final snack = delta > 0
          ? _l10n.puzzleSuccessLineWithElo(delta)
          : _l10n.puzzleSuccessLine;
      showTransientBoardMessage(snack);
      state = state.copyWith(puzzleSnackText: snack);
      await Future<void>.delayed(const Duration(milliseconds: 920));
      state = state.copyWith(clearPuzzleChallenge: true);
    } finally {
      _puzzleFeedbackBusy = false;
    }
  }

  Future<void> _handlePuzzleFailure() async {
    if (_puzzleFeedbackBusy) return;
    _puzzleFeedbackBusy = true;
    try {
      state = state.copyWith(
        puzzleSnackText: _l10n.puzzleWrongResetting,
      );
      _runTintPulse(false);
      await _ref.read(prefsRepositoryProvider).recordPuzzleActivity(solved: false);
      _ref.invalidate(prefsRepositoryProvider);
      await Future<void>.delayed(const Duration(milliseconds: 720));
      _resetPuzzlePositionOnly();
      showTransientBoardMessage(_l10n.gameSandboxPositionRestored);
    } finally {
      _puzzleFeedbackBusy = false;
    }
  }

  List<List<String>> effectiveBoard(List<List<String>> serverBoard) {
    if (state.sandboxMode && state.sandboxFenOverride != null) {
      return boardFromPlacementFen(state.sandboxFenOverride!.split(' ').first);
    }
    if (state.historyReviewMoveIndex != null && !state.sandboxMode) {
      final snap = _ref.read(boardSessionNotifierProvider).snapshot;
      if (snap != null) {
        final chess = ch.Chess(); // Initial board
        // Start from initial FEN if present, but our ESP only returns moves from start
        // A full robust implementation would start from puzzle/setup FEN if active,
        // but for now we apply standard history index.
        for (int i = 0; i <= state.historyReviewMoveIndex!; i++) {
          final m = snap.history.moves[i];
          if (m.from != null && m.to != null) {
            chess.move({'from': m.from!, 'to': m.to!, 'promotion': 'q'});
          }
        }
        return boardFromPlacementFen(chess.generate_fen().split(' ').first);
      }
    }
    return serverBoard;
  }

  Future<void> onSquareTapped(
    String algebraic,
    BoardSessionState session,
    void Function(String msg) snack,
  ) async {
    if (state.hintFrom != null || state.hintTo != null) {
      state = state.copyWith(clearHint: true);
    }
    if (state.historyReviewMoveIndex != null && !state.sandboxMode) {
      state = state.copyWith(clearHistoryIndex: true);
    }
    if (state.sandboxMode) {
      _sandboxTap(algebraic, snack);
      return;
    }
    if (state.remoteMovesEnabled &&
        (session.transport == BoardTransport.wifi ||
            session.transport == BoardTransport.ble ||
            session.transport == BoardTransport.mock)) {
      await _remoteTwoTap(algebraic, snack, session.snapshot);
      return;
    }
    if (state.selectedSquare == algebraic) {
      _moveFeedback(selectionTap: true);
      state = state.copyWith(clearSelection: true);
    } else {
      _moveFeedback(selectionTap: true);
      state = state.copyWith(selectedSquare: algebraic);
    }
  }

  Future<void> completePromotion(
      String pieceLower, void Function(String) snack) async {
    final from = state.pendingPromotionFrom;
    final to = state.pendingPromotionTo;
    if (from == null || to == null) return;

    if (state.sandboxMode) {
      final chess = _sandboxChess;
      if (chess == null || _puzzleFeedbackBusy) return;
      final before = chess.generate_fen();
      final ok = chess.move({
        'from': from,
        'to': to,
        'promotion': pieceLower,
      });
      if (!ok) {
        _moveFeedback(invalid: true);
        snack(_l10n.gamePromotionFailedSnack);
      } else {
        _moveFeedback(successMove: true);
        var next = [...state.sandboxUndoStack, before];
        if (next.length > 80) next = next.sublist(next.length - 80);
        state = state.copyWith(
          sandboxFenOverride: chess.generate_fen(),
          sandboxUndoStack: next,
          sandboxUndoCount: next.length,
          clearPromotion: true,
          clearSelection: true,
          clearMessage: true,
        );
        _notePuzzleMove(from, to, pieceLower);
      }
      return;
    }

    final sess = _ref.read(boardSessionNotifierProvider);
    if (!state.remoteMovesEnabled ||
        (sess.transport != BoardTransport.wifi &&
            sess.transport != BoardTransport.ble &&
            sess.transport != BoardTransport.mock)) {
      state = state.copyWith(clearPromotion: true);
      return;
    }

    try {
      await _ref.read(boardSessionNotifierProvider.notifier).postRemoteMove(
            from,
            to,
            promotion: pieceLower,
          );
      _moveFeedback(successMove: true);
      state = state.copyWith(
        clearPromotion: true,
        clearSelection: true,
        clearMessage: true,
      );
    } catch (e) {
      if (e is BoardApiException && e.statusCode == 400) {
        _moveFeedback(invalid: true);
        final msg = e.detail ?? _l10n.gameSandboxIllegalMove;
        showTransientBoardMessage(msg);
        snack(msg);
      } else {
        snack('$e');
      }
      state = state.copyWith(clearPromotion: true);
    }
  }

  void _sandboxTap(String algebraic, void Function(String) snack) {
    final chess = _sandboxChess;
    if (_puzzleFeedbackBusy) {
      return;
    }
    if (chess == null) {
      snack(_l10n.gameSandboxLoadPositionFirst);
      return;
    }
    final sel = state.selectedSquare;
    if (sel == null) {
      final piece = chess.get(algebraic);
      if (piece == null) {
        state = state.copyWith(sandboxMessage: _l10n.gameSandboxSelectPiece);
        return;
      }
      if (piece.color != chess.turn) {
        state = state.copyWith(sandboxMessage: _l10n.gameSandboxWrongSide);
        return;
      }
      _moveFeedback(selectionTap: true);
      state = state.copyWith(selectedSquare: algebraic, clearMessage: true);
      return;
    }
    if (sel == algebraic) {
      _moveFeedback(selectionTap: true);
      state = state.copyWith(clearSelection: true, clearMessage: true);
      return;
    }
    if (_needsPromotion(chess, sel, algebraic)) {
      state = state.copyWith(
        pendingPromotionFrom: sel,
        pendingPromotionTo: algebraic,
        clearMessage: true,
      );
      return;
    }
    final before = chess.generate_fen();
    final ok = chess.move({'from': sel, 'to': algebraic});
    if (!ok) {
      _moveFeedback(invalid: true);
      state = state.copyWith(sandboxMessage: _l10n.gameSandboxIllegalMove);
      showTransientBoardMessage(_l10n.gameSandboxIllegalMoveSandbox);
    } else {
      _moveFeedback(successMove: true);
      var next = [...state.sandboxUndoStack, before];
      if (next.length > 80) next = next.sublist(next.length - 80);
      state = state.copyWith(
        sandboxFenOverride: chess.generate_fen(),
        sandboxUndoStack: next,
        sandboxUndoCount: next.length,
        clearSelection: true,
        clearMessage: true,
      );
      _notePuzzleMove(sel, algebraic, null);
    }
  }

  bool _needsPromotion(ch.Chess chess, String from, String to) {
    final piece = chess.get(from);
    if (piece?.type != ch.PieceType.PAWN) return false;
    final toRank = int.tryParse(to.substring(1)) ?? 0;
    return (piece!.color == ch.Color.WHITE && toRank == 8) ||
        (piece.color == ch.Color.BLACK && toRank == 1);
  }

  bool _needsPromotionRemote(GameSnapshot? snap, String from, String to) {
    if (snap == null) return false;
    final chess = ch.Chess();
    if (!chess.load(fenFromSnapshot(snap))) return false;
    return _needsPromotion(chess, from, to);
  }

  /// `null` = výběr OK; jinak důvod pro snack / transient (remote tahy z aplikace).
  String? _remoteFirstTapReject(GameSnapshot? snap, String algebraic) {
    if (snap == null) return null;
    final chess = ch.Chess();
    if (!chess.load(fenFromSnapshot(snap))) return null;
    final piece = chess.get(algebraic);
    if (piece == null) return 'empty';
    if (piece.color != chess.turn) return 'turn';
    return null;
  }

  Future<void> _remoteTwoTap(
    String algebraic,
    void Function(String) snack,
    GameSnapshot? snap,
  ) async {
    if (_remoteFrom == null) {
      final rej = _remoteFirstTapReject(snap, algebraic);
      if (rej == 'empty') {
        snack(_l10n.gameRemoteEmptySquareSnack);
        showTransientBoardMessage(_l10n.gameRemoteEmptySquareHud);
        return;
      }
      if (rej == 'turn') {
        _moveFeedback(invalid: true);
        snack(_l10n.gameRemoteWrongTurnSnack);
        showTransientBoardMessage(_l10n.gameRemoteWrongTurnHud);
        return;
      }
      _remoteFrom = algebraic;
      _moveFeedback(selectionTap: true);
      state = state.copyWith(selectedSquare: algebraic);
      return;
    }
    final from = _remoteFrom!;
    final to = algebraic;
    // Stejné pole znovu = zrušení výběru (neodesílat d1→d1 — firmware hlásí MOVE_ERROR_DESTINATION_OCCUPIED / zmizí figurka v UI).
    if (from == to) {
      _remoteFrom = null;
      state = state.copyWith(clearSelection: true);
      _moveFeedback(selectionTap: true);
      return;
    }
    if (snap != null && _needsPromotionRemote(snap, from, to)) {
      _remoteFrom = null;
      state = state.copyWith(
        pendingPromotionFrom: from,
        pendingPromotionTo: to,
        clearSelection: true,
      );
      return;
    }
    _remoteFrom = null;
    state = state.copyWith(clearSelection: true);
    try {
      await _ref
          .read(boardSessionNotifierProvider.notifier)
          .postRemoteMove(from, to);
      _moveFeedback(successMove: true);
    } catch (e) {
      if (e is BoardApiException && e.statusCode == 400) {
        _moveFeedback(invalid: true);
        _flashInvalidMove(from, to);
        final msg = e.detail ?? _l10n.gameSandboxIllegalMove;
        showTransientBoardMessage(msg);
        snack(msg);
      } else {
        snack('$e');
      }
    }
  }
}
