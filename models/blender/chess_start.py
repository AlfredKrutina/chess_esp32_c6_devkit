"""Standard starting position for the scenes, without python-chess."""

import led_effects as fx

BACK = ("rook", "knight", "bishop", "queen", "king", "bishop", "knight", "rook")
FILES = "abcdefgh"


def layout():
    pieces = []
    for col, kind in enumerate(BACK):
        f = FILES[col]
        pieces.append((f"w_{kind}_{f}", kind, "w", f"{f}1"))
        pieces.append((f"w_pawn_{f}", "pawn", "w", f"{f}2"))
        pieces.append((f"b_pawn_{f}", "pawn", "b", f"{f}7"))
        pieces.append((f"b_{kind}_{f}", kind, "b", f"{f}8"))
    return pieces


def movable():
    """White to move: all pawns and both knights have a legal move."""
    state = {f"{f}2": fx.YELLOW for f in FILES}
    state["b1"] = fx.YELLOW
    state["g1"] = fx.YELLOW
    return state
