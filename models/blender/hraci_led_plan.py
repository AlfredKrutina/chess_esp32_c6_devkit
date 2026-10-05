"""LED states for the players film. System Python only (needs python-chess).

Prints one JSON object on stdout. Blender's Python does not have chess.
"""

import json
import sys

import chess

import led_effects as fx


def lift_leds(board, square):
    piece = board.piece_at(chess.parse_square(square))
    state = {square: fx.KING_LIFT if piece.piece_type == chess.KING else fx.YELLOW}
    for move in board.legal_moves:
        if move.from_square != chess.parse_square(square):
            continue
        target = chess.square_name(move.to_square)
        if board.is_castling(move):
            state[target] = fx.BLUE
        elif board.piece_at(move.to_square) is not None:
            state[target] = fx.ORANGE
        else:
            state.setdefault(target, fx.GREEN)
    return state


def movable(board):
    return {chess.square_name(m.from_square): fx.YELLOW for m in board.legal_moves}


def after_quiet(board):
    state = {}
    if board.is_check():
        state[chess.square_name(board.king(board.turn))] = fx.PINK
    state.update(movable(board))
    return state


def rgb(state):
    return {sq: [int(c) for c in color] for sq, color in state.items()}


def rook_squares(move):
    rank = chess.square_rank(move.from_square)
    kingside = chess.square_file(move.to_square) == 6
    src_file = 7 if kingside else 0
    dst_file = 5 if kingside else 3
    return chess.square_name(chess.square(src_file, rank)), chess.square_name(chess.square(dst_file, rank))


def plan(ucis):
    board = chess.Board()
    out = {"idle": rgb(movable(board)), "moves": []}
    i = 0
    while i < len(ucis):
        uci = ucis[i]
        move = chess.Move.from_uci(uci)
        if move not in board.legal_moves:
            raise SystemExit(f"{uci} is not legal from {board.fen()}")
        src, dst = uci[:2], uci[2:]
        entry = {"uci": uci, "src": src, "dst": dst, "lift": rgb(lift_leds(board, src))}
        if board.is_castling(move):
            rook_from, rook_to = rook_squares(move)
            entry["castle"] = "king"
            entry["rook_from"] = rook_from
            entry["rook_to"] = rook_to
            out["moves"].append(entry)
            i += 1
            if i >= len(ucis) or ucis[i] != rook_from + rook_to:
                raise SystemExit(f"castling {uci} needs the rook {rook_from}{rook_to} next")
            rook = {
                "uci": ucis[i],
                "src": rook_from,
                "dst": rook_to,
                "castle": "rook",
                "lift": rgb({rook_from: fx.YELLOW, rook_to: fx.GREEN}),
            }
            board.push(move)
            rook["to_white"] = board.turn == chess.WHITE
            rook["idle"] = rgb(after_quiet(board))
            out["moves"].append(rook)
        else:
            board.push(move)
            entry["to_white"] = board.turn == chess.WHITE
            entry["idle"] = rgb(after_quiet(board))
            out["moves"].append(entry)
        i += 1
    return out


if __name__ == "__main__":
    print(json.dumps(plan(sys.argv[1:])))
