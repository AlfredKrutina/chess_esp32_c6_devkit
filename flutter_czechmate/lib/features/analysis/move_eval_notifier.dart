import 'dart:async';

import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app_providers.dart';
import '../../core/analysis/move_evaluation.dart';
import '../../core/models/game_snapshot.dart';
import '../../core/utils/fen_from_board.dart';
import '../connection/board_session_notifier.dart';
import '../connection/board_session_state.dart';

class MoveEvalState {
  const MoveEvalState({
    this.entries = const [],
    this.lastMessage,
    this.lastGrade,
    this.lastBusy = false,
  });

  final List<MoveEvalHistoryEntry> entries;
  final String? lastMessage;
  final MoveGrade? lastGrade;
  final bool lastBusy;

  MoveEvalState copyWith({
    List<MoveEvalHistoryEntry>? entries,
    String? lastMessage,
    MoveGrade? lastGrade,
    bool? lastBusy,
    bool clearLast = false,
  }) {
    return MoveEvalState(
      entries: entries ?? this.entries,
      lastMessage: clearLast ? null : (lastMessage ?? this.lastMessage),
      lastGrade: clearLast ? null : (lastGrade ?? this.lastGrade),
      lastBusy: lastBusy ?? this.lastBusy,
    );
  }
}

final moveEvalNotifierProvider = StateNotifierProvider<MoveEvalNotifier, MoveEvalState>((ref) {
  return MoveEvalNotifier(ref);
});

class MoveEvalNotifier extends StateNotifier<MoveEvalState> {
  MoveEvalNotifier(this._ref) : super(const MoveEvalState()) {
    // Bez okamžitého výstupu zůstane `_prevSnap` null až do první změny session —
    // pak první tah na desce přijde s `prevSnap == null` a eval se přeskočí (včetně demo).
    _ref.listen<BoardSessionState>(
      boardSessionNotifierProvider,
      _onSession,
      fireImmediately: true,
    );
  }

  final Ref _ref;
  int _taskGen = 0;
  GameSnapshot? _prevSnap;

  void clearHistory() {
    _prevSnap = null;
    state = const MoveEvalState();
  }

  void _onSession(BoardSessionState? prev, BoardSessionState next) {
    final cur = next.snapshot;
    final prevSnap = _prevSnap;
    _prevSnap = cur;

    if (cur == null) {
      state = state.copyWith(entries: const [], clearLast: true);
      return;
    }

    final emptied = cur.history.moves.isEmpty && (prevSnap?.history.moves.isNotEmpty ?? false);
    final shorter = prevSnap != null && cur.history.moves.length < prevSnap.history.moves.length;
    final counterReset = cur.status.moveCount == 0 && (prevSnap?.status.moveCount ?? 0) > 0;
    if (emptied || shorter || counterReset) {
      state = const MoveEvalState();
    }

    if (!_ref.read(prefsRepositoryProvider).moveEvaluationEnabled) return;
    if (prevSnap == null) return;

    final newLen = cur.history.moves.length;
    final oldLen = prevSnap.history.moves.length;
    if (newLen != oldLen + 1) return;

    final last = cur.history.moves.last;
    final pf = last.from;
    final pt = last.to;
    if (pf == null || pt == null) return;

    final gen = ++_taskGen;
    unawaited(_runMoveEvaluation(gen, previous: prevSnap, current: cur, playedFrom: pf, playedTo: pt));
  }

  Future<void> _runMoveEvaluation(
    int gen, {
    required GameSnapshot previous,
    required GameSnapshot current,
    required String playedFrom,
    required String playedTo,
  }) async {
    state = state.copyWith(lastBusy: true);
    try {
      final fenBefore = fenFromSnapshot(previous);
      final fenAfter = fenFromSnapshot(current);
      final hint = _ref.read(prefsRepositoryProvider).hintDepth;
      final depth = hint.clamp(12, 18);
      final client = _ref.read(stockfishApiClientProvider);

      final bestLine = await client.fetchBestMove(fenBefore, depth: depth);
      if (gen != _taskGen) {
        state = state.copyWith(lastBusy: false);
        return;
      }

      final played = MoveEvaluation.normalizeUci(playedFrom, playedTo);
      final bestUci = MoveEvaluation.normalizeUci(bestLine.from, bestLine.to);
      final eb = bestLine.evalPawns != null ? MoveEvaluation.evalToWhitePerspective(fenBefore, bestLine.evalPawns!) : null;

      final afterProbe = await client.fetchBestMove(fenAfter, depth: depth);
      if (gen != _taskGen) {
        state = state.copyWith(lastBusy: false);
        return;
      }

      final ea = afterProbe.evalPawns != null ? MoveEvaluation.evalToWhitePerspective(fenAfter, afterProbe.evalPawns!) : null;
      final idx = current.history.moves.length;

      if (played == bestUci) {
        const msg = 'Výborný tah! Byl to nejlepší tah.';
        _record(idx, ea, MoveGrade.best, 0);
        state = state.copyWith(lastMessage: msg, lastGrade: MoveGrade.best, lastBusy: false);
        return;
      }

      final bestFmt = '${bestLine.from}-${bestLine.to}';
      final ev = MoveEvaluation.classify(
        playedFrom: playedFrom,
        playedTo: playedTo,
        bestFrom: bestLine.from,
        bestTo: bestLine.to,
        evalBeforeWhite: eb,
        evalAfterWhite: ea,
        moveIndex1Based: idx,
        bestMoveFormatted: bestFmt,
      );

      double? lossPawns;
      if (eb != null && ea != null) {
        final whiteJustMoved = (idx - 1) % 2 == 0;
        var scoreDrop = whiteJustMoved ? (eb - ea) : (ea - eb);
        if (scoreDrop < 0) scoreDrop = 0;
        lossPawns = scoreDrop;
      }

      _record(idx, ea, ev.grade, lossPawns);
      state = state.copyWith(lastMessage: ev.message, lastGrade: ev.grade, lastBusy: false);
    } catch (e) {
      if (gen != _taskGen) return;
      state = state.copyWith(
        lastMessage: 'Eval selhala: $e',
        lastGrade: MoveGrade.error,
        lastBusy: false,
      );
    }
  }

  void _record(int moveIndex1Based, double? evalWhitePawns, MoveGrade grade, double? lossPawns) {
    final list = [...state.entries]..removeWhere((e) => e.moveIndex1Based == moveIndex1Based);
    list.add(MoveEvalHistoryEntry(
      moveIndex1Based: moveIndex1Based,
      evalWhitePawns: evalWhitePawns,
      grade: grade,
      lossPawns: lossPawns,
    ));
    list.sort((a, b) => a.moveIndex1Based.compareTo(b.moveIndex1Based));
    while (list.length > 400) {
      list.removeAt(0);
    }
    state = state.copyWith(entries: list);
  }
}
