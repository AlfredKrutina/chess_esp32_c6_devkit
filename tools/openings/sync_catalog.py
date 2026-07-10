#!/usr/bin/env python3
"""Validate and sync openings_master.json to web + Flutter assets."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MASTER = ROOT / "data" / "openings_master.json"
SCHEMA = ROOT / "data" / "openings_catalog.schema.json"
WEB_OUT = ROOT / "components" / "web_server_task" / "web" / "data" / "openings_catalog.json"
FLUTTER_OUT = ROOT / "flutter_czechmate" / "assets" / "data" / "openings_catalog.json"
WEB_JS_CATALOG = ROOT / "components" / "web_server_task" / "web" / "js" / "opening_catalog.js"

UCI_RE = re.compile(r"^[a-h][1-8][a-h][1-8][qrbn]?$")
CASTLE_UCI = frozenset({"e1g1", "e1c1", "e8g8", "e8c8"})


def load_json(path: Path) -> dict:
    with path.open(encoding="utf-8") as f:
        return json.load(f)


def validate_schema(data: dict) -> None:
    try:
        import jsonschema
    except ImportError:
        print("WARN: jsonschema not installed — skipping schema validation", file=sys.stderr)
        return
    schema = load_json(SCHEMA)
    jsonschema.validate(data, schema)


def parse_uci(uci: str) -> tuple[str, str, str | None]:
    promo = None
    if len(uci) == 5:
        promo = uci[4]
        uci = uci[:4]
    if len(uci) != 4:
        raise ValueError(f"invalid uci length: {uci}")
    return uci[:2], uci[2:4], promo


def validate_chess_moves(data: dict) -> None:
    try:
        import chess
    except ImportError:
        print("WARN: python-chess not installed — skipping move legality", file=sys.stderr)
        return

    for opening in data["openings"]:
        board = chess.Board(opening["start_fen"])
        for uci in opening["line_uci"]:
            move = chess.Move.from_uci(uci)
            if move not in board.legal_moves:
                raise ValueError(
                    f"{opening['id']}: illegal move {uci} in position {board.fen()}"
                )
            board.push(move)


def validate_physical_rules(data: dict) -> None:
    try:
        import chess
    except ImportError:
        print("WARN: python-chess not installed — skipping physical-rules", file=sys.stderr)
        return

    for opening in data["openings"]:
        oid = opening["id"]
        line = opening["line_uci"]
        player_set = set(opening["player_ply_indices"])
        checkpoints = set(opening.get("checkpoint_ply_indices") or [])

        for uci in line:
            if uci in CASTLE_UCI:
                raise ValueError(f"{oid}: castling not allowed in v1 catalog: {uci}")

        board = chess.Board(opening["start_fen"])
        virtual_only: set[chess.Square] = set()

        for ply_idx, uci in enumerate(line):
            move = chess.Move.from_uci(uci)
            from_sq, to_sq, _ = parse_uci(uci)
            is_player = ply_idx in player_set
            is_capture = board.is_capture(move)

            if is_player and is_capture:
                target_occ = from_sq if move.uci()[2:4] == from_sq else to_sq
                # capture target square on physical board must not be virtual-only
                to_file = chess.parse_square(to_sq)
                if to_file in virtual_only:
                    if ply_idx not in checkpoints and not any(
                        c < ply_idx for c in checkpoints
                    ):
                        raise ValueError(
                            f"{oid}: player capture {uci} at ply {ply_idx} "
                            f"targets virtual-only square without prior checkpoint"
                        )

            if not is_player:
                # virtual move: pieces on from/to become virtual on physical layer
                virtual_only.add(move.from_square)
                virtual_only.add(move.to_square)

            board.push(move)

        if len(line) > 12:
            raise ValueError(f"{oid}: line too long ({len(line)} > 12)")


def validate_ids(data: dict) -> None:
    ids = [o["id"] for o in data["openings"]]
    if len(ids) != len(set(ids)):
        raise ValueError("duplicate opening ids")

    for opening in data["openings"]:
        ply_count = len(opening["line_uci"])
        for idx in opening["player_ply_indices"]:
            if idx < 0 or idx >= ply_count:
                raise ValueError(f"{opening['id']}: player_ply_index {idx} out of range")
        for uci in opening["line_uci"]:
            if not UCI_RE.match(uci):
                raise ValueError(f"{opening['id']}: bad uci {uci}")

        side = opening["side"]
        for idx in opening["player_ply_indices"]:
            mover_white = (idx % 2 == 0)
            if side == "white" and not mover_white:
                raise ValueError(f"{opening['id']}: white line but player index {idx} is black move")
            if side == "black" and mover_white:
                raise ValueError(f"{opening['id']}: black line but player index {idx} is white move")


def file_hash(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def copy_outputs(data: dict) -> None:
    payload = json.dumps(data, ensure_ascii=False, indent=2) + "\n"
    WEB_OUT.parent.mkdir(parents=True, exist_ok=True)
    FLUTTER_OUT.parent.mkdir(parents=True, exist_ok=True)
    WEB_OUT.write_text(payload, encoding="utf-8")
    FLUTTER_OUT.write_text(payload, encoding="utf-8")
    print(f"Wrote {WEB_OUT}")
    print(f"Wrote {FLUTTER_OUT}")
    write_web_catalog_js(data)


def write_web_catalog_js(data: dict) -> None:
    """Generate web/js/opening_catalog.js from master (no bundler)."""
    lines = data.get("openings", [])
    compact = []
    for o in lines:
        entry = {
            "id": o["id"],
            "eco": o.get("eco", ""),
            "name": o["name"],
            "side": o["side"],
            "difficulty": o.get("difficulty", 1),
            "idea": o.get("idea"),
            "start_fen": o["start_fen"],
            "line_uci": o["line_uci"],
            "player_ply_indices": o["player_ply_indices"],
            "checkpoint_ply_indices": o.get("checkpoint_ply_indices") or [],
        }
        if o.get("mirror_line_id"):
            entry["mirror_line_id"] = o["mirror_line_id"]
        compact.append(entry)

    lines_json = json.dumps(compact, ensure_ascii=False, indent=4)
    body = f"""// ============================================================================
// OPENING CATALOG — generated by tools/openings/sync_catalog.py --copy
// ============================================================================
(function (global) {{
    'use strict';

    /** @type {{Array<object>}} */
    var OPENING_LINES = {lines_json};

    function findOpeningById(id) {{
        return OPENING_LINES.filter(function (x) {{ return x.id === id; }})[0] || null;
    }}

    function openingStartPayload(line, mode) {{
        var body = {{
            action: 'start',
            line_id: line.id,
            mode: mode || 'learn',
            side: line.side,
            start_fen: line.start_fen,
            line_uci: line.line_uci,
            player_ply_indices: line.player_ply_indices
        }};
        if (line.checkpoint_ply_indices && line.checkpoint_ply_indices.length) {{
            body.checkpoint_ply_indices = line.checkpoint_ply_indices;
        }}
        return body;
    }}

    global.OPENING_LINES = OPENING_LINES;
    global.findOpeningById = findOpeningById;
    global.openingStartPayload = openingStartPayload;
}}(typeof window !== 'undefined' ? window : globalThis));
"""
    WEB_JS_CATALOG.parent.mkdir(parents=True, exist_ok=True)
    WEB_JS_CATALOG.write_text(body, encoding="utf-8")
    print(f"Wrote {WEB_JS_CATALOG}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--validate", action="store_true", help="Validate master JSON")
    parser.add_argument("--physical-rules", action="store_true", help="Enforce v1 physical constraints")
    parser.add_argument("--copy", action="store_true", help="Copy to web + flutter assets")
    args = parser.parse_args()

    if not MASTER.exists():
        print(f"ERROR: missing {MASTER}", file=sys.stderr)
        return 1

    data = load_json(MASTER)

    try:
        validate_schema(data)
        validate_ids(data)
        validate_chess_moves(data)
        if args.physical_rules:
            validate_physical_rules(data)
    except Exception as exc:
        print(f"VALIDATION FAILED: {exc}", file=sys.stderr)
        return 1

    print(f"OK: {len(data['openings'])} openings validated")

    if args.copy:
        copy_outputs(data)
        if WEB_OUT.exists() and FLUTTER_OUT.exists():
            if file_hash(WEB_OUT) != file_hash(FLUTTER_OUT):
                print("ERROR: web/flutter catalog hash mismatch", file=sys.stderr)
                return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
