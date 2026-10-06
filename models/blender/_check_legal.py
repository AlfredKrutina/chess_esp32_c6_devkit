import chess

b = chess.Board()
seq = [
    "e2e4",
    "e7e5",
    "g1f3",
    "b8c6",
    "f1c4",
    "d7d6",
    "b1c3",
    "c8g4",
    "f3e5",
    "g4d1",
    "c4f7",
    "e8e7",
    "c3d5",
]
for u in seq:
    m = chess.Move.from_uci(u)
    ok = m in b.legal_moves
    san = b.san(m) if ok else "ILLEGAL"
    print(u, san, "check", b.is_check())
    if not ok:
        print(b)
        print([b.san(x) for x in b.legal_moves])
        raise SystemExit(1)
    b.push(m)
print("mate", b.is_checkmate())
print(b)
print("wk", chess.square_name(b.king(chess.WHITE)))
