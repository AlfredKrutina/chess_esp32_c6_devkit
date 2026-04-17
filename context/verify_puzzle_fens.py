#!/usr/bin/env python3
"""
Ověření puzzle v game_task.c: právě jeden tah dává mat (python-chess).
Spustit: .venv_puzzle/bin/python context/verify_puzzle_fens.py
"""
import sys
import os

try:
    import chess
except ImportError:
    print("pip install python-chess nebo použij .venv_puzzle", file=sys.stderr)
    sys.exit(1)

# Musí odpovídat game_puzzles[] v components/game_task/game_task.c
PUZZLES = [
    (1, "7k/7p/8/8/8/8/5Q2/6K1 w - - 0 1", "f2f8"),
    (2, "6k1/5ppp/8/8/8/8/1Q6/6K1 w - - 0 1", "b2b8"),
    (3, "4r1k1/5ppp/8/8/8/8/4R3/4K3 w - - 0 1", "e2e8"),
    (
        4,
        "r1bqkb1r/pppp1ppp/2n2n2/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR w KQkq - 0 1",
        "h5f7",
    ),
    (5, "6k1/5ppp/8/8/3Q4/8/6PP/6K1 w - - 0 1", "d4d8"),
]


def main():
    ok_all = True
    for pid, fen, uci in PUZZLES:
        b = chess.Board(fen)
        mates = []
        for mv in b.legal_moves:
            b.push(mv)
            if b.is_checkmate():
                mates.append(mv.uci())
            b.pop()
        ok = mates == [uci]
        ok_all = ok_all and ok
        print(f"id {pid}: {'OK' if ok else 'FAIL'} mates={mates} need={uci}")
    sys.exit(0 if ok_all else 1)


if __name__ == "__main__":
    main()
