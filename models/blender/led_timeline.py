"""Presentation timeline: piece motion, board LEDs, shot markers, captions.

  py -3.12 led_timeline.py      (needs: pip install chess)

Legal targets, movable pieces and check come from python-chess. What the
board lights at each moment follows the firmware drop/pickup paths, see
docs/reference/BLENDER_VIDEO_SCENARIO.md. Writes timeline_presentation.json.
"""

import json
import math
from pathlib import Path

import chess

import led_effects as fx

FPS = 30
LIFT = 0.098
RISE = 13
# Same pace, hop and foot sizes as build_predstaveni. A diagonal passes 25 mm
# from the squares beside it, inside the two feet, so the window has to fit the
# higher arch or the piece speeds up and still cuts through.
SPEED = 0.0028
MIN_HOP = 0.015
CLEAR_OVER = 0.018
HEIGHT = {
    chess.PAWN: 0.048,
    chess.KNIGHT: 0.056,
    chess.BISHOP: 0.064,
    chess.ROOK: 0.060,
    chess.QUEEN: 0.072,
    chess.KING: 0.084,
}
FOOT = {
    chess.PAWN: 0.014,
    chess.KNIGHT: 0.014,
    chess.BISHOP: 0.014,
    chess.ROOK: 0.014,
    chess.QUEEN: 0.017,
    chess.KING: 0.017,
}
OUT = Path(__file__).resolve().parent / "timeline_presentation.json"

BACK = ("rook", "knight", "bishop", "queen", "king", "bishop", "knight", "rook")


def sq(text):
    return chess.parse_square(text)


def lift_leds(board, square):
    piece = board.piece_at(sq(square))
    state = {square: fx.KING_LIFT if piece.piece_type == chess.KING else fx.YELLOW}
    for move in board.legal_moves:
        if move.from_square != sq(square):
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


def after_quiet_move(board):
    """Quiet drop: pink on a checked king first, then yellow overwrites it if the king can move."""
    state = {}
    if board.is_check():
        state[chess.square_name(board.king(board.turn))] = fx.PINK
    state.update(movable(board))
    return state


class Show:
    def __init__(self):
        self.f = 1
        self.board = chess.Board()
        self.ids = {}
        self.kinds = {}
        self.states = []
        self.keys = {}
        self.hidden = []
        self.markers = {}
        self.captions = []
        self.notes = []
        for col, kind in enumerate(BACK):
            f = "abcdefgh"[col]
            for color, back_rank, pawn_rank in (("w", 1, 2), ("b", 8, 7)):
                self.add(f"{color}_{kind}_{f}", kind, color, f"{f}{back_rank}")
                self.add(f"{color}_pawn_{f}", "pawn", color, f"{f}{pawn_rank}")

    def add(self, pid, kind, color, square):
        self.ids[square] = pid
        self.kinds[pid] = (kind, color)
        self.keys[pid] = [{"f": 1, "sq": square, "h": 0.0}]

    def mark(self, label, frame=None):
        self.markers[label] = self.f if frame is None else frame

    def caption(self, text, start, length, style="main"):
        self.captions.append({"f0": start, "f1": start + length, "text": text, "style": style})

    def led(self, frame, state):
        self.states.append({"f": int(frame), "leds": {k: list(v) for k, v in state.items()}})

    def play(self, start, seq, speed=1.0):
        end = start
        for t_ms, state in seq:
            frame = start + round(t_ms / 1000.0 * FPS / speed)
            self.led(frame, state)
            end = max(end, frame)
        return end

    def key(self, pid, frame, square, height, **extra):
        entry = {"f": int(frame), "sq": square, "h": height}
        entry.update(extra)
        self.keys[pid].append(entry)

    def lift(self, square, rise=RISE):
        pid = self.ids[square]
        self.key(pid, self.f, square, 0.0)
        self.key(pid, self.f + rise, square, LIFT)
        return pid, self.f + 2, self.f + rise

    def carry(self, pid, frm, to, start, hold, travel, drop):
        f_hold = start + hold
        self.key(pid, f_hold, frm, LIFT)
        f_over = f_hold + travel
        self.key(pid, f_over, to, LIFT)
        f_land = f_over + drop
        self.key(pid, f_land, to, 0.0)
        return f_land

    def push(self, uci):
        move = chess.Move.from_uci(uci)
        frm = chess.square_name(move.from_square)
        to = chess.square_name(move.to_square)
        pid = self.ids.pop(frm)
        self.ids.pop(to, None)
        self.ids[to] = pid
        if self.board.is_castling(move):
            rook_from, rook_to = ("h", "f") if move.to_square > move.from_square else ("a", "d")
            rank = frm[1]
            self.ids[rook_to + rank] = self.ids.pop(rook_from + rank)
        self.board.push(move)

    def square_xy(self, square):
        return ((ord(square[0]) - ord("d") - 0.5) * 0.036, (int(square[1]) - 4.5) * 0.036)

    def travel_frames(self, frm, to):
        """Frames a flat move needs at the film pace, plus a small hop."""
        if isinstance(to, (tuple, list)):
            x0, y0 = self.square_xy(frm)
            dist = math.hypot(to[0] - x0, to[1] - y0)
        else:
            dist = math.hypot(ord(frm[0]) - ord(to[0]), int(frm[1]) - int(to[1])) * 0.036
        dist += 0.045
        return max(16, int(round(dist / SPEED)))

    def arc_frames(self, frm, to):
        """Frames the real arch needs. The foot clears anyone the straight line passes."""
        mover = self.board.piece_at(sq(frm))
        if mover is None:
            return 10
        x0, y0 = self.square_xy(frm)
        x1, y1 = self.square_xy(to)
        delta_x, delta_y = x1 - x0, y1 - y0
        span2 = delta_x * delta_x + delta_y * delta_y
        horizontal = math.sqrt(span2)
        if horizontal < 1e-8:
            return 10
        foot = FOOT[mover.piece_type]
        peak = MIN_HOP
        enter = 1.0
        leave = 0.0
        for square in chess.SQUARES:
            other = self.board.piece_at(square)
            if other is None or square == sq(frm):
                continue
            ox, oy = self.square_xy(chess.square_name(square))
            along = ((ox - x0) * delta_x + (oy - y0) * delta_y) / span2
            u = max(0.0, min(1.0, along))
            dist = math.hypot(x0 + delta_x * u - ox, y0 + delta_y * u - oy)
            reach = foot + FOOT[other.piece_type] + 0.002
            if dist > reach or u < 0.02 or u > 0.98:
                continue
            peak = max(peak, HEIGHT[other.piece_type] + CLEAR_OVER)
            half = math.sqrt(max(0.0, reach * reach - dist * dist)) / horizontal
            enter = min(enter, max(0.0, u - half))
            leave = max(leave, min(1.0, u + half))
        if peak <= MIN_HOP + 0.001:
            rise = fall = 0.48
        else:
            rise = min(0.35, enter * 0.65)
            fall = min(0.35, max(0.0, 1.0 - leave) * 0.65)
            if rise + fall > 0.98:
                rise = max(0.0, 0.98 - fall)

        def factor(u):
            if rise > 1e-4 and u < rise:
                t = u / rise
            elif fall > 1e-4 and u > 1.0 - fall:
                t = (1.0 - u) / fall
            else:
                return 1.0
            return t * t * (3.0 - 2.0 * t)

        prev = (0.0, 0.0)
        acc = 0.0
        for i in range(1, 65):
            s = i / 64.0
            pt = (s * horizontal, peak * factor(s))
            acc += math.hypot(pt[0] - prev[0], pt[1] - prev[1])
            prev = pt
        # Two spare frames absorb the difference between this estimate and the
        # rig's real square pitch.
        return max(10, int(round(acc / SPEED))) + 2

    def blitz(self, frm, to):
        """One legal move. The piece is back on the board and the sped-up
        lights have finished before this returns, so the next move cannot overlap."""
        if chess.Move.from_uci(frm + to) not in self.board.legal_moves:
            raise RuntimeError(f"{frm}{to} is not legal from {self.board.fen()}")
        pid, detect, top = self.lift(frm, rise=6)
        self.led(detect, lift_leds(self.board, frm))
        # Rise, hold and drop already spend 12 frames. A knight hops over a piece,
        # so its path is longer than the flat gap.
        knight = self.board.piece_at(sq(frm)).piece_type == chess.KNIGHT
        budget = max(self.travel_frames(frm, to) + (28 if knight else 0), self.arc_frames(frm, to))
        travel = max(8, budget - 12)
        land = self.carry(pid, frm, to, top, 2, travel, 4)
        self.push(frm + to)
        t = self.play(land, fx.move_path(frm, to), 3.0)
        if self.board.is_checkmate():
            self.f = t
            return {"detect": detect, "land": land, "mate": t}
        t = self.play(t, fx.player_change(self.board.turn == chess.WHITE), 4.0)
        self.led(t, after_quiet_move(self.board))
        self.f = t + 3
        return {"detect": detect, "land": land, "idle": t}

    def aside(self, square, dest):
        """Lift a captured piece clear of the board before the attacker arrives."""
        pid = self.ids[square]
        start = self.f
        span = max(38, self.travel_frames(square, dest))
        if span > 80:
            span += 32
        self.key(pid, start, square, 0.0)
        self.key(pid, start + 8, square, LIFT + 0.02)
        self.key(pid, start + span - 18, square, 0.0, aside=list(dest))
        # parse_moves lands the piece 18 frames after the last key.
        return start + span

    def quiet(self, frm, to, hold=18, travel=22, drop=12, path_speed=1.0, wave_speed=1.0, after=14):
        pid, detect, top = self.lift(frm)
        self.led(detect, lift_leds(self.board, frm))
        overhead = RISE + hold + drop
        need = max(self.travel_frames(frm, to), self.arc_frames(frm, to))
        travel = max(travel, need - overhead)
        land = self.carry(pid, frm, to, top, hold, travel, drop)
        self.push(frm + to)
        t = self.play(land, fx.move_path(frm, to), path_speed)
        events = {"detect": detect, "land": land, "path_end": t}
        if self.board.is_checkmate():
            events["mate"] = t
            self.f = t
            return events
        t = self.play(t, fx.player_change(self.board.turn == chess.WHITE), wave_speed)
        self.led(t, after_quiet_move(self.board))
        events["idle"] = t
        self.f = t + after
        return events


def build():
    s = Show()

    # Dark wall, then the sandblasted glass, then the boot trail. No charger.
    # The name is withheld until the camera has pulled back.
    s.mark("intro")
    s.led(1, {})
    s.mark("glass", 120)
    s.f = 210
    s.mark("boot")
    boot_f = s.f
    s.caption("Šachovnice,", boot_f + 36, 28, "hero")
    s.caption("která ví, co se na ní děje.", boot_f + 60, 34, "hero")
    end = s.play(s.f, fx.boot(), speed=2.4)
    s.f = end + 6
    s.led(s.f, movable(s.board))
    s.mark("idle")
    s.caption("Žlutá: tyhle figurky můžou táhnout.", s.f + 6, 48)
    s.f += 60

    # Hall V2: piece identity. Board LEDs stay on the idle highlight.
    s.mark("hall")
    s.caption("Každá figurka nese magnetický kód.", s.f + 6, 46)
    s.caption("Hall senzory poznají, která to je.", s.f + 52, 42)
    s.caption("Soupeř je má otočené opačně.", s.f + 108, 58)
    s.f += 175

    # 1. e4 — the full quiet-move cycle. Every firmware animation runs in real
    # time: LEDs only change when the board detects a lift or a drop.
    s.mark("e4")
    ev = s.quiet("e2", "e4", hold=28, travel=30, after=36)
    s.caption("Zvedni figurku — zelená ukáže, kam smí.", ev["detect"] + 2, ev["land"] - ev["detect"] - 2)
    s.caption("Polož ji — modrý záblesk a šedá vlna předají tah.", ev["land"], ev["idle"] - ev["land"] + 6)
    s.mark("e4_land", ev["land"])

    s.mark("d6")
    s.quiet("d7", "d6", hold=14, travel=24, drop=12, after=28)

    # 2. Bb5+ — pink stays because the black king has no legal move.
    s.mark("bb5")
    ev = s.quiet("f1", "b5", hold=16, travel=34, after=48)
    s.mark("check", ev["idle"])
    s.caption("Šach. Růžová na králi, žlutá na tazích, které ho kryjí.", ev["idle"] + 4, 36)

    s.mark("c6")
    ev = s.quiet("c7", "c6", hold=20, travel=22, drop=12, after=28)
    s.caption("Při šachu svítí jen tahy, které ho kryjí.", ev["detect"] + 2, ev["land"] - ev["detect"] + 4)

    # 3. Ng1-g3 is illegal. Red blink, then recovery to f3.
    s.mark("illegal")
    pid, detect, top = s.lift("g1")
    s.led(detect, lift_leds(s.board, "g1"))
    land = s.carry(pid, "g1", "g3", top, 12, 36, 12)
    s.mark("illegal_land", land)
    t = s.play(land, fx.error_blink("g3"), 1.0)
    s.caption("Tah mimo pravidla?", land + 2, 40)
    s.caption("Červená ho hned zastaví.", land + 44, t - land - 30)
    s.f = t + 28
    s.key(pid, s.f, "g3", 0.0)
    s.key(pid, s.f + RISE, "g3", LIFT)
    recover = {"g3": fx.YELLOW, "g1": fx.BLUE}
    for square, color in lift_leds(s.board, "g1").items():
        if square != "g1":
            recover.setdefault(square, color)
    s.led(s.f + 2, recover)
    s.mark("recover", s.f + 2)
    s.caption("Modrá ukáže, odkud figurka přišla.", s.f + 2, 50)
    land = s.carry(pid, "g3", "f3", s.f + RISE, 30, 18, 11)
    s.push("g1f3")
    s.led(land, movable(s.board))
    s.f = land + 20

    # 3... cxb5 — capture: orange target, then purple on the lifted victim.
    s.mark("capture")
    attacker, detect, top = s.lift("c6")
    s.led(detect, lift_leds(s.board, "c6"))
    s.caption("Braní: oranžová ukáže soupeřovu figurku.", detect + 2, 38)
    victim_time = top + 22
    victim = s.ids["b5"]
    s.key(victim, victim_time, "b5", 0.0)
    s.key(victim, victim_time + 12, "b5", LIFT * 1.3)
    s.led(victim_time + 2, {"c6": fx.YELLOW, "b5": fx.PURPLE})
    s.caption("Zvednutá oběť zfialoví.", victim_time + 2, 38)
    s.key(victim, victim_time + 30, "b5", 0.0, off=True, spin=True)
    s.mark("capture_victim", victim_time + 2)
    land = s.carry(attacker, "c6", "b5", top, 58, 20, 11)
    s.push("c6b5")
    s.led(land, movable(s.board))
    s.f = land + 20

    # 4. O-O — king first, then the board guides the rook.
    s.mark("castle")
    king, detect, top = s.lift("e1")
    s.led(detect, lift_leds(s.board, "e1"))
    s.caption("Rošáda: modrá ukáže cíl krále.", detect + 2, 40)
    land = s.carry(king, "e1", "g1", top, 22, 24, 12)
    s.mark("castle_king", land)
    # The board names the rook as the king settles. The hand takes it at once,
    # instead of leaving h1 and f1 lit through the whole pulse train.
    s.led(land, {"h1": fx.SILVER, "f1": fx.GREEN})
    s.caption("Stříbrná věž, zelený cíl.", land + 2, 36)
    s.f = land + 16
    rook, detect, top = s.lift("h1")
    s.led(detect, {"h1": fx.YELLOW, "f1": fx.GREEN})
    # The rook clears the king on a 102 mm arc, about 260 mm of path.
    land = s.carry(rook, "h1", "f1", top, 10, 63, 11)
    s.push("e1g1")
    seq, length = fx.castling_completion("f1", {"h1": fx.YELLOW})
    t = s.play(land, seq)
    s.mark("castle_done", land)
    s.caption("Zlatá potvrdí dokončení.", land + 2, 40)
    t = s.play(t, fx.player_change(s.board.turn == chess.WHITE))
    s.led(t, after_quiet_move(s.board))
    s.f = t + 20

    # Legal continuation. One move finishes, lights included, before the next.
    s.mark("endgame_cut")
    s.caption("Zbytek partie, zrychleně.", s.f + 2, 90, "small")
    for uci in ("b8c6", "d2d4", "e7e6", "c1g5", "c8d7", "f3e5", "a7a6", "d1h5", "g8f6"):
        s.blitz(uci[:2], uci[2:])
    s.mark("timelapse_end")
    s.mark("mate_move")
    # The f7 pawn is off the square, and clear of the captured bishop, before the queen.
    s.f = s.aside("f7", (-0.235, 0.10, -0.0002))
    ev = s.quiet("h5", "f7", hold=16, travel=22, drop=12)
    if "mate" not in ev:
        raise RuntimeError("Qxf7 is not mate")
    s.mark("mate", ev["mate"])
    winner = chess.WHITE

    def owner(square):
        piece = s.board.piece_at(sq(square))
        if piece is None:
            return None
        return "own" if piece.color == winner else "opp"

    wave_ms = 6200
    end = s.play(ev["mate"], fx.endgame_wave("g1", owner, wave_ms), 1.0)
    s.caption("Mat.", ev["mate"] + 2, 40, "hero")
    s.caption("Vlna od vítězného krále.", ev["mate"] + 44, 50)
    s.mark("outro", ev["mate"] + 100)
    s.caption("CzechMate", ev["mate"] + 108, end - ev["mate"] - 108, "logo")
    s.caption("chytrá šachovnice", ev["mate"] + 118, end - ev["mate"] - 118, "tagline")
    s.f = end
    s.mark("end")

    s.states.sort(key=lambda item: item["f"])
    labels = list(s.captions)
    labels.insert(2, {"f0": boot_f, "f1": boot_f + 90, "text": "Start desky · zelená stopa", "style": "small"})
    return {
        "fps": FPS,
        "frame_end": s.f,
        "lift": LIFT,
        "pieces": {pid: {"kind": k, "color": c} for pid, (k, c) in s.kinds.items()},
        "keys": s.keys,
        "hidden": s.hidden,
        "leds": s.states,
        "markers": s.markers,
        "captions": s.captions,
        "labels": labels,
    }


def main():
    data = build()
    cut = data["markers"]["endgame_cut"]
    spans = []
    for pid, keys in data["keys"].items():
        i = 0
        while i < len(keys) - 1:
            if keys[i]["h"] > 0 or keys[i + 1]["h"] <= 0:
                i += 1
                continue
            j = next(n for n in range(i + 1, len(keys)) if keys[n]["h"] <= 0)
            end = keys[j]["f"]
            if "aside" in keys[j] or "off" in keys[j]:
                end += 18
            if keys[i]["f"] >= cut:
                spans.append((keys[i]["f"], end, pid, keys[i]["sq"], keys[j]["sq"]))
            i = j
    spans.sort()
    for left, right in zip(spans, spans[1:]):
        if right[0] < left[1]:
            raise RuntimeError(f"two pieces move at once: {left} and {right}")
    OUT.write_text(json.dumps(data, ensure_ascii=False, indent=1), encoding="utf-8")
    print("frames", data["frame_end"], "seconds", round(data["frame_end"] / FPS, 1))
    for label, frame in sorted(data["markers"].items(), key=lambda kv: kv[1]):
        print(f"  {frame:5d}  {label}")


if __name__ == "__main__":
    main()
