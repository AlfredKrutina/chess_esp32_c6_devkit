#!/usr/bin/env python3
"""One-shot patch: add validated common_mistakes to openings_master.json (Fáze 5b)."""

from __future__ import annotations

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MASTER = ROOT / "data" / "openings_master.json"

MISTAKES_BY_ID: dict[str, list[dict]] = {
    "italian_giuoco_white": [
        {
            "wrong_uci": "f1b5",
            "at_ply_index": 4,
            "hint": {
                "cs": "Nejdřív c4 — španělská je jiná lekce.",
                "en": "Play c4 first — the Spanish is a separate lesson.",
            },
        },
        {
            "wrong_uci": "f1c4",
            "at_ply_index": 2,
            "hint": {
                "cs": "Nejdřív Nf3 — střelec až po jezdci.",
                "en": "Develop the knight to f3 before the bishop.",
            },
        },
    ],
    "spanish_berlin_white": [
        {
            "wrong_uci": "f1c4",
            "at_ply_index": 4,
            "hint": {
                "cs": "V berlínské španělské hrajeme Bb5, ne Bc4.",
                "en": "In the Berlin Spanish we play Bb5, not Bc4.",
            },
        }
    ],
    "london_system_white": [
        {
            "wrong_uci": "c2c4",
            "at_ply_index": 2,
            "hint": {
                "cs": "Londýn drží d4 + Bf4, ne klasické c4 hned.",
                "en": "London keeps d4 + Bf4, not an early c4.",
            },
        }
    ],
    "four_knights_white": [
        {
            "wrong_uci": "f1c4",
            "at_ply_index": 4,
            "hint": {
                "cs": "Ve čtyřech jezdcích je typické d4, ne Bc4.",
                "en": "In Four Knights d4 is typical, not Bc4.",
            },
        }
    ],
    "queens_gambit_white": [
        {
            "wrong_uci": "d2d3",
            "at_ply_index": 0,
            "hint": {
                "cs": "Dámský gambit začíná d4, ne d3.",
                "en": "The Queen's Gambit starts with d4, not d3.",
            },
        }
    ],
    "scotch_game_white": [
        {
            "wrong_uci": "f1b5",
            "at_ply_index": 4,
            "hint": {
                "cs": "Ve skotské hře je d4 klíčové, ne Bb5.",
                "en": "In the Scotch d4 is key, not Bb5.",
            },
        }
    ],
    "vienna_white": [
        {
            "wrong_uci": "f2f4",
            "at_ply_index": 2,
            "hint": {
                "cs": "Ve vídeňské nejdřív Nc3, pak až f4.",
                "en": "In the Vienna play Nc3 before f4.",
            },
        }
    ],
    "sicilian_odb_black": [
        {
            "wrong_uci": "g8f6",
            "at_ply_index": 3,
            "hint": {
                "cs": "Nejdřív ...d6 — typická sicílie, ne ...Nf6 tak brzy.",
                "en": "Play ...d6 first — typical Sicilian, not ...Nf6 so early.",
            },
        }
    ],
    "petrov_black": [
        {
            "wrong_uci": "d7d6",
            "at_ply_index": 3,
            "hint": {
                "cs": "V petrově pokračuj ...Nf6, ne ...d6.",
                "en": "In the Petrov continue ...Nf6, not ...d6.",
            },
        }
    ],
    "caro_kann_classical_black": [
        {
            "wrong_uci": "g8f6",
            "at_ply_index": 3,
            "hint": {
                "cs": "Nejdřív ...d5 v karu — ne ...Nf6 před pevným centrem.",
                "en": "Play ...d5 in the Caro first — not ...Nf6 before a solid center.",
            },
        }
    ],
    "slav_defence_black": [
        {
            "wrong_uci": "g8f6",
            "at_ply_index": 3,
            "hint": {
                "cs": "Ve slavské ...c6 drží strukturu, ne ...Nf6 hned.",
                "en": "In the Slav ...c6 holds the structure, not ...Nf6 so soon.",
            },
        }
    ],
}


def main() -> int:
    data = json.loads(MASTER.read_text(encoding="utf-8"))
    for opening in data["openings"]:
        oid = opening["id"]
        if oid in MISTAKES_BY_ID:
            opening["common_mistakes"] = MISTAKES_BY_ID[oid]
    MASTER.write_text(
        json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    print(f"Patched {len(MISTAKES_BY_ID)} openings in {MASTER}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
