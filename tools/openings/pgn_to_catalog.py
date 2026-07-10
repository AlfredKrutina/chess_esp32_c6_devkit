#!/usr/bin/env python3
"""Import opening line from PGN — phase 0b stub.

Full parser (studies, comments, arrows) is backlog. This stub validates
inputs and prints guidance for manual entry into data/openings_master.json.

Usage:
    python3 tools/openings/pgn_to_catalog.py --pgn path/to/game.pgn --id my_line_white
    python3 tools/openings/pgn_to_catalog.py --pgn game.pgn --id my_line --max-plies 8 --side white
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MASTER = ROOT / "data" / "openings_master.json"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pgn", required=True, help="PGN file with at least one game")
    parser.add_argument("--id", required=True, help="Target opening id (snake_case)")
    parser.add_argument("--side", choices=["white", "black"], default="white")
    parser.add_argument("--max-plies", type=int, default=8, help="Max full moves * 2 plies")
    parser.add_argument("--dry-run", action="store_true", help="Extract UCI only, do not write master")
    args = parser.parse_args()

    pgn_path = Path(args.pgn)
    if not pgn_path.is_file():
        print(f"ERROR: PGN not found: {pgn_path}", file=sys.stderr)
        return 1

    try:
        import chess
        import chess.pgn
    except ImportError:
        print("ERROR: install python-chess: pip install python-chess", file=sys.stderr)
        return 1

    with pgn_path.open(encoding="utf-8") as f:
        game = chess.pgn.read_game(f)
    if game is None:
        print("ERROR: no game in PGN", file=sys.stderr)
        return 1

    board = game.board()
    uci_list: list[str] = []
    for _ in range(args.max_plies):
        move = game.move(board)
        if move is None:
            break
        uci_list.append(move.uci())
        board.push(move)

    if len(uci_list) < 2:
        print("ERROR: need at least 2 plies", file=sys.stderr)
        return 1

    player_indices = list(range(0 if args.side == "white" else 1, len(uci_list), 2))
    draft = {
        "id": args.id,
        "eco": (game.headers.get("ECO") or "A00").strip(),
        "name": {
            "cs": game.headers.get("Opening") or args.id,
            "en": game.headers.get("Opening") or args.id,
        },
        "side": args.side,
        "difficulty": 2,
        "start_fen": chess.STARTING_FEN,
        "line_uci": uci_list,
        "player_ply_indices": player_indices,
        "checkpoint_ply_indices": [4] if len(uci_list) > 6 else [],
    }

    print(json.dumps(draft, ensure_ascii=False, indent=2))
    print(
        "\n# Stub: merge the JSON above into data/openings_master.json, "
        "then run:\n"
        "python3 tools/openings/sync_catalog.py --validate --physical-rules --copy",
        file=sys.stderr,
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
