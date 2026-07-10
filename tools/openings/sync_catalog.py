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
RATIONALE = ROOT / "data" / "openings_rationale.json"
RATIONALE_SCHEMA = ROOT / "data" / "openings_rationale.schema.json"
WEB_OUT = ROOT / "components" / "web_server_task" / "web" / "data" / "openings_catalog.json"
FLUTTER_OUT = ROOT / "flutter_czechmate" / "assets" / "data" / "openings_catalog.json"
WEB_JS_CATALOG = ROOT / "components" / "web_server_task" / "web" / "js" / "opening_catalog.js"

UCI_RE = re.compile(r"^[a-h][1-8][a-h][1-8][qrbn]?$")
CASTLE_UCI = frozenset({"e1g1", "e1c1", "e8g8", "e8c8"})


def load_json(path: Path) -> dict:
    with path.open(encoding="utf-8") as f:
        return json.load(f)


def validate_schema(data: dict, *, schema_path: Path = SCHEMA) -> None:
    try:
        import jsonschema
    except ImportError:
        print("WARN: jsonschema not installed — skipping schema validation", file=sys.stderr)
        return
    schema = load_json(schema_path)
    jsonschema.validate(data, schema)


def load_rationale() -> dict:
    if not RATIONALE.exists():
        raise FileNotFoundError(f"missing rationale sidecar: {RATIONALE}")
    rationale = load_json(RATIONALE)
    validate_schema(rationale, schema_path=RATIONALE_SCHEMA)
    return rationale


def validate_rationale_coverage(data: dict, rationale: dict) -> None:
    opening_ids = {o["id"] for o in data["openings"]}
    entries = rationale.get("entries") or {}
    entry_ids = set(entries.keys())
    missing = sorted(opening_ids - entry_ids)
    extra = sorted(entry_ids - opening_ids)
    if missing:
        raise ValueError(f"rationale missing entries for: {', '.join(missing)}")
    if extra:
        raise ValueError(f"rationale has unknown opening ids: {', '.join(extra)}")

    for opening in data["openings"]:
        rat = entries[opening["id"]]
        for related_id in rat.get("related_line_ids") or []:
            if related_id not in opening_ids:
                raise ValueError(
                    f"{opening['id']}: related_line_id not in catalog: {related_id}"
                )


def merge_rationale(data: dict, rationale: dict) -> dict:
    """Return export payload with rationale merged per opening (master stays sidecar)."""
    entries = rationale["entries"]
    export = json.loads(json.dumps(data))
    for opening in export["openings"]:
        opening["rationale"] = entries[opening["id"]]
    return export


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


def validate_common_mistakes(data: dict) -> None:
    try:
        import chess
    except ImportError:
        print("WARN: python-chess not installed — skipping common_mistakes", file=sys.stderr)
        return

    for opening in data["openings"]:
        oid = opening["id"]
        mistakes = opening.get("common_mistakes") or []
        if len(mistakes) > 4:
            raise ValueError(f"{oid}: too many common_mistakes ({len(mistakes)} > 4)")
        line = opening["line_uci"]
        player_set = set(opening["player_ply_indices"])
        board = chess.Board(opening["start_fen"])
        for entry in mistakes:
            wrong = entry["wrong_uci"]
            ply = entry["at_ply_index"]
            if not UCI_RE.match(wrong):
                raise ValueError(f"{oid}: bad common_mistakes uci {wrong}")
            if ply not in player_set:
                raise ValueError(f"{oid}: common_mistakes ply {ply} not a player ply")
            if ply >= len(line):
                raise ValueError(f"{oid}: common_mistakes ply {ply} out of range")
            expected = line[ply]
            if wrong == expected:
                raise ValueError(f"{oid}: common_mistake {wrong} equals expected at ply {ply}")
            b = board.copy()
            for i in range(ply):
                b.push(chess.Move.from_uci(line[i]))
            move = chess.Move.from_uci(wrong)
            if move not in b.legal_moves:
                raise ValueError(
                    f"{oid}: common_mistake {wrong} illegal at ply {ply} ({b.fen()})"
                )
            hint = entry.get("hint") or {}
            if not hint.get("cs") or not hint.get("en"):
                raise ValueError(f"{oid}: common_mistake hint missing cs/en at ply {ply}")


def validate_opponent_annotations(data: dict) -> None:
    for opening in data["openings"]:
        oid = opening["id"]
        annotations = opening.get("opponent_annotations") or []
        if len(annotations) > 4:
            raise ValueError(f"{oid}: too many opponent_annotations ({len(annotations)} > 4)")
        player_set = set(opening["player_ply_indices"])
        ply_count = len(opening["line_uci"])
        for entry in annotations:
            ply = entry["ply_index"]
            if ply in player_set:
                raise ValueError(
                    f"{oid}: opponent_annotations ply {ply} must not be a player ply"
                )
            if ply < 0 or ply >= ply_count:
                raise ValueError(f"{oid}: opponent_annotations ply {ply} out of range")
            comment = entry.get("comment") or {}
            if not comment.get("cs") or not comment.get("en"):
                raise ValueError(f"{oid}: opponent_annotations missing cs/en at ply {ply}")


def validate_mirror_symmetric(data: dict) -> None:
    """Each mirror_line_id must exist, opposite side, and reciprocal link."""
    by_id = {o["id"]: o for o in data["openings"]}
    seen: set[tuple[str, str]] = set()

    for opening in data["openings"]:
        oid = opening["id"]
        mirror_id = opening.get("mirror_line_id")
        if not mirror_id:
            continue
        pair = tuple(sorted((oid, mirror_id)))
        if pair in seen:
            continue
        seen.add(pair)

        partner = by_id.get(mirror_id)
        if partner is None:
            raise ValueError(f"{oid}: mirror_line_id not in catalog: {mirror_id}")

        side = opening.get("side")
        partner_side = partner.get("side")
        if side == partner_side:
            raise ValueError(
                f"{oid}: mirror pair {mirror_id} must be opposite side "
                f"(both {side})"
            )
        if partner.get("mirror_line_id") != oid:
            raise ValueError(
                f"{oid} -> {mirror_id} but {mirror_id} -> "
                f"{partner.get('mirror_line_id')!r} (expected {oid})"
            )

    pair_count = len(seen)
    if pair_count < 10:
        raise ValueError(f"mirror pairs: expected at least 10, got {pair_count}")


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


def copy_outputs(data: dict, rationale: dict) -> None:
    export = merge_rationale(data, rationale)
    payload = json.dumps(export, ensure_ascii=False, indent=2) + "\n"
    WEB_OUT.parent.mkdir(parents=True, exist_ok=True)
    FLUTTER_OUT.parent.mkdir(parents=True, exist_ok=True)
    WEB_OUT.write_text(payload, encoding="utf-8")
    FLUTTER_OUT.write_text(payload, encoding="utf-8")
    print(f"Wrote {WEB_OUT}")
    print(f"Wrote {FLUTTER_OUT}")
    write_web_catalog_js(export)


def write_web_catalog_js(data: dict) -> None:
    """Generate web/js/opening_catalog.js from master (no bundler)."""
    lines = data.get("openings", [])
    curricula = data.get("curricula", [])
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
        if o.get("common_mistakes"):
            entry["common_mistakes"] = o["common_mistakes"]
        if o.get("opponent_annotations"):
            entry["opponent_annotations"] = o["opponent_annotations"]
        if o.get("rationale"):
            entry["rationale"] = o["rationale"]
        compact.append(entry)

    lines_json = json.dumps(compact, ensure_ascii=False, indent=4)
    curricula_json = json.dumps(curricula, ensure_ascii=False, indent=4)
    body = f"""// ============================================================================
// OPENING CATALOG — generated by tools/openings/sync_catalog.py --copy
// ============================================================================
(function (global) {{
    'use strict';

    /** @type {{Array<object>}} */
    var OPENING_CURRICULA = {curricula_json};

    /** @type {{Array<object>}} */
    var OPENING_LINES = {lines_json};

    function findOpeningById(id) {{
        return OPENING_LINES.filter(function (x) {{ return x.id === id; }})[0] || null;
    }}

    function openingStartPayload(line, mode, opponentMode) {{
        var body = {{
            action: 'start',
            line_id: line.id,
            mode: mode || 'learn',
            opponent_mode: opponentMode || 'physical',
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

    global.OPENING_CURRICULA = OPENING_CURRICULA;
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
    parser.add_argument(
        "--mirror-symmetric",
        action="store_true",
        help="Require reciprocal mirror_line_id pairs with opposite side (§8.8)",
    )
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
        validate_common_mistakes(data)
        validate_opponent_annotations(data)
        rationale = load_rationale()
        validate_rationale_coverage(data, rationale)
        if args.physical_rules:
            validate_physical_rules(data)
        if args.mirror_symmetric:
            validate_mirror_symmetric(data)
    except Exception as exc:
        print(f"VALIDATION FAILED: {exc}", file=sys.stderr)
        return 1

    print(f"OK: {len(data['openings'])} openings validated")
    print(f"OK: {len(rationale['entries'])} rationale entries")

    if args.copy:
        copy_outputs(data, rationale)
        if WEB_OUT.exists() and FLUTTER_OUT.exists():
            if file_hash(WEB_OUT) != file_hash(FLUTTER_OUT):
                print("ERROR: web/flutter catalog hash mismatch", file=sys.stderr)
                return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
