"""Board LED effects ported from the firmware, frame by frame.

Every function returns a list of (t_ms, state). A state maps square names
("e2") to RGB 0..255 and always describes the whole board, like the firmware
after led_clear_board_only(). Sources:
  boot trail / fade      led_task.c  led_boot_animation_step, fade-out (main.c 25 ms steps)
  move path              led_task.c  led_anim_move_path
  player change          led_task.c  led_anim_player_change
  endgame wave           led_task.c  led_update_endgame_wave
  castling rook pulses   game_move_exec.c  castling branch
  castling completion    game_move_exec.c  show_castling_completion_animation
  error blink            game_task.c  game_show_invalid_move_error_with_blink
"""

import math

YELLOW = (255, 255, 0)
GREEN = (0, 255, 0)
ORANGE = (255, 165, 0)
RED = (255, 0, 0)
BLUE = (0, 0, 255)
PURPLE = (128, 0, 128)
PINK = (255, 192, 203)
GOLD = (255, 215, 0)
SILVER = (192, 192, 192)
KING_LIFT = (255, 128, 0)
BOOT_GREEN = (0, 128, 0)

FILES = "abcdefgh"


def name(row, col):
    return f"{FILES[col]}{row + 1}"


def rc(square):
    return int(square[1]) - 1, FILES.index(square[0])


def led_to_square(index):
    row = index // 8
    offset = index % 8
    col = 7 - offset if row % 2 == 0 else offset
    return name(row, col)


def scaled(color, k):
    return tuple(max(0, min(255, int(channel * k))) for channel in color)


def boot():
    """201 steps x 25 ms of a cumulative green trail, then 20 x 30 ms fade."""
    out = []
    lit = {}
    for step in range(201):
        progress = step / 2.0
        index = int(progress * 64 / 100)
        if index < 64:
            lit[led_to_square(index)] = BOOT_GREEN
        out.append((step * 25, dict(lit)))
    start = 201 * 25
    every = {led_to_square(i): BOOT_GREEN for i in range(64)}
    for step in range(20):
        level = 1.0 - (step + 1) / 20.0
        out.append((start + step * 30, {sq: scaled(c, level) for sq, c in every.items()}))
    out.append((start + 20 * 30, {}))
    return out


def move_path(from_sq, to_sq):
    fr, fc = rc(from_sq)
    tr, tc = rc(to_sq)
    out = []
    t = 0
    for frame in range(25):
        state = {}
        progress = frame / 24.0
        for trail in range(6):
            tp = progress - trail * 0.08
            if tp < 0:
                continue
            if tp > 1:
                break
            eased = tp * tp * (3.0 - 2.0 * tp)
            row = int(fr + (tr - fr) * eased)
            col = int(fc + (tc - fc) * eased)
            intensity = 0.5 + (tp / 0.2) * 0.5 if tp < 0.2 else 1.0
            brightness = (1.0 - trail * 0.15) ** 1.5
            p1 = 0.6 + 0.4 * math.sin(progress * 12.56 + trail * 1.26)
            p2 = 0.8 + 0.2 * math.sin(progress * 25.12 + trail * 2.51)
            p3 = 0.9 + 0.1 * math.sin(progress * 50.24 + trail * 3.77)
            blue = int(255 * intensity * brightness * p1 * p2 * p3)
            state[name(row, col)] = (0, 0, max(0, blue))
        out.append((t, state))
        t += 2
    for breath in range(8):
        level = 0.5 + 0.5 * math.sin(breath * 0.785)
        out.append((t, {to_sq: (0, 0, int(255 * level))}))
        t += 20
    out.append((t, {}))
    return out


def player_change(to_white):
    out = [(0, {})]
    start, end = (7, 0) if to_white else (0, 7)
    t = 30
    for frame in range(50):
        progress = frame / 49.0
        eased = 0.5 * (1.0 - math.cos(progress * math.pi))
        pos = start + (end - start) * eased
        startup = 1.0
        if frame < 15:
            startup = 0.5 * (1.0 - math.cos(frame / 15.0 * math.pi))
        state = {}
        for row in range(8):
            g = math.exp(-((row - pos) ** 2) / (2.0 * 2.5 * 2.5)) * startup
            if g > 0.15:
                level = int(31 * g)
                for col in range(8):
                    state[name(row, col)] = (level, level, level)
        out.append((t, state))
        t += 12
    out.append((t, {}))
    out.append((t + 30, {}))
    return out


def endgame_wave(king_sq, owner, duration_ms):
    """owner(square) -> 'own', 'opp' or None, relative to the winner."""
    kr, kc = rc(king_sq)
    out = []
    radius = 1
    t = 100
    while t <= duration_ms:
        state = {}
        for ring in range(4):
            current = radius - ring * 0.3
            if current < 0.2:
                continue
            for dy in range(-radius, radius + 1):
                for dx in range(-radius, radius + 1):
                    dist = math.sqrt(dx * dx + dy * dy)
                    ring_distance = abs(dist - current)
                    if ring_distance > 1.2:
                        continue
                    row, col = kr + dy, kc + dx
                    if not (0 <= row < 8 and 0 <= col < 8):
                        continue
                    intensity = max(0.15, 1.0 - ring_distance / 1.2)
                    side = owner(name(row, col))
                    if side == "opp":
                        color = (255, 30, 30)
                    elif side == "own":
                        color = (30, 255, 80)
                    else:
                        color = (30, 100, 255)
                    state[name(row, col)] = scaled(color, intensity)
        state[king_sq] = GOLD
        out.append((t, state))
        radius += 1
        if radius > 14:
            radius = 1
        t += 100
    return out


def castling_rook_pulses(rook_from, rook_to):
    out = []
    for pulse in range(3):
        phase = pulse * 2.0 * 3.14159 / 3.0
        level = 0.5 + 0.5 * (1.0 + math.sin(phase)) / 2.0
        out.append((pulse * 200, {rook_from: scaled(SILVER, level), rook_to: (0, int(255 * level), 0)}))
    out.append((600, {rook_from: SILVER, rook_to: GREEN}))
    return out


def castling_completion(rook_to, keep):
    """Gold blinks on the rook square. The lift highlights stay underneath."""
    out = []
    for i in range(5):
        on = dict(keep)
        on[rook_to] = GOLD
        off = dict(keep)
        off.pop(rook_to, None)
        out.append((i * 200, on))
        out.append((i * 200 + 100, off))
    return out, 1000


def error_blink(square):
    """Dark, then 10 toggles every 300 ms starting with ON, then solid red."""
    out = [(0, {})]
    on = False
    for toggle in range(1, 11):
        on = not on
        out.append((toggle * 300, {square: RED} if on else {}))
    out.append((3000, {square: RED}))
    return out
