import 'package:chess/chess.dart' as ch;
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../../core/models/game_snapshot.dart';
import '../../../core/utils/fen_board_parser.dart';
import '../../../core/utils/fen_from_board.dart';
import '../../connection/board_session_notifier.dart';
import '../../connection/board_session_state.dart';
import 'game_ui_state.dart';

final gameUiNotifierProvider =
    StateNotifierProvider<GameUiNotifier, GameUiState>((ref) {
  return GameUiNotifier(ref);
});

class GameUiNotifier extends StateNotifier<GameUiState> {
  GameUiNotifier(this._ref) : super(const GameUiState());

  final Ref _ref;

  ch.Chess? _sandboxChess;
  String? _remoteFrom;

  void toggleFlip() =>
      state = state.copyWith(boardFlipped: !state.boardFlipped);

  void toggleCoordinates() =>
      state = state.copyWith(showCoordinates: !state.showCoordinates);

  void toggleRemoteMoves() =>
      state = state.copyWith(remoteMovesEnabled: !state.remoteMovesEnabled);

  void enterSandbox(GameSnapshot? snap) {
    state = state.copyWith(sandboxMode: true, clearMessage: true);
    if (snap != null) {
      _loadSandbox(snap);
    }
  }

  void exitSandbox() {
    _sandboxChess = null;
    _remoteFrom = null;
    state = state.copyWith(
      sandboxMode: false,
      clearSelection: true,
      clearSandboxFen: true,
      clearPromotion: true,
      clearMessage: true,
    );
  }

  void toggleSandbox(GameSnapshot? snap) {
    if (state.sandboxMode) {
      exitSandbox();
    } else {
      enterSandbox(snap);
    }
  }

  void _loadSandbox(GameSnapshot snap) {
    final fen = fenFromSnapshot(snap);
    final chess = ch.Chess();
    if (!chess.load(fen)) {
      state = state.copyWith(sandboxMessage: 'FEN nelze načíst');
      return;
    }
    _sandboxChess = chess;
    state = state.copyWith(
      sandboxFenOverride: chess.generate_fen(),
      clearMessage: true,
      clearSelection: true,
    );
  }

  List<List<String>> effectiveBoard(List<List<String>> serverBoard) {
    if (state.sandboxMode && state.sandboxFenOverride != null) {
      return boardFromPlacementFen(state.sandboxFenOverride!.split(' ').first);
    }
    return serverBoard;
  }

  Future<void> onSquareTapped(
    String algebraic,
    BoardSessionState session,
    void Function(String msg) snack,
  ) async {
    if (state.sandboxMode) {
      _sandboxTap(algebraic, snack);
      return;
    }
    if (state.remoteMovesEnabled &&
        session.transport == BoardTransport.wifi &&
        session.wifiBaseUrl != null) {
      await _remoteTwoTap(algebraic, snack);
      return;
    }
    if (state.selectedSquare == algebraic) {
      state = state.copyWith(clearSelection: true);
    } else {
      state = state.copyWith(selectedSquare: algebraic);
    }
  }

  void completePromotion(String pieceLower, void Function(String) snack) {
    final from = state.pendingPromotionFrom;
    final to = state.pendingPromotionTo;
    final chess = _sandboxChess;
    if (from == null || to == null || chess == null) return;
    final ok = chess.move({
      'from': from,
      'to': to,
      'promotion': pieceLower,
    });
    if (!ok) {
      snack('Promoce selhala');
    } else {
      state = state.copyWith(
        sandboxFenOverride: chess.generate_fen(),
        clearPromotion: true,
        clearSelection: true,
        clearMessage: true,
      );
    }
  }

  void _sandboxTap(String algebraic, void Function(String) snack) {
    final chess = _sandboxChess;
    if (chess == null) {
      snack('Nejdřív načti pozici (připoj desku nebo mock)');
      return;
    }
    final sel = state.selectedSquare;
    if (sel == null) {
      final piece = chess.get(algebraic);
      if (piece == null) {
        state = state.copyWith(sandboxMessage: 'Prázdné pole');
        return;
      }
      if (piece.color != chess.turn) {
        state = state.copyWith(sandboxMessage: 'Není tah této barvy');
        return;
      }
      state = state.copyWith(selectedSquare: algebraic, clearMessage: true);
      return;
    }
    if (sel == algebraic) {
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
    final ok = chess.move({'from': sel, 'to': algebraic});
    if (!ok) {
      state = state.copyWith(sandboxMessage: 'Neplatný tah');
    } else {
      state = state.copyWith(
        sandboxFenOverride: chess.generate_fen(),
        clearSelection: true,
        clearMessage: true,
      );
    }
  }

  bool _needsPromotion(ch.Chess chess, String from, String to) {
    final piece = chess.get(from);
    if (piece?.type != ch.PieceType.PAWN) return false;
    final toRank = int.tryParse(to.substring(1)) ?? 0;
    return (piece!.color == ch.Color.WHITE && toRank == 8) ||
        (piece.color == ch.Color.BLACK && toRank == 1);
  }

  Future<void> _remoteTwoTap(String algebraic, void Function(String) snack) async {
    if (_remoteFrom == null) {
      _remoteFrom = algebraic;
      state = state.copyWith(selectedSquare: algebraic);
      return;
    }
    final from = _remoteFrom!;
    final to = algebraic;
    _remoteFrom = null;
    state = state.copyWith(clearSelection: true);
    try {
      await _ref
          .read(boardSessionNotifierProvider.notifier)
          .postRemoteMove(from, to);
    } catch (e) {
      snack('$e');
    }
  }
}
