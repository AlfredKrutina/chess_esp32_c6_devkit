from pathlib import Path

p = Path("components/uart_task/uart_task.c")
text = p.read_text(encoding="utf-8")
needle = '#include "../uart_commands_extended/include/uart_commands_extended.h"\n'
text = text.replace(needle, "")
names = """
uart_cmd_eval uart_cmd_performance uart_cmd_config uart_cmd_demo uart_cmd_demo_status
uart_cmd_component_off uart_cmd_component_on uart_cmd_endgame_white uart_cmd_endgame_black
uart_cmd_test_move_anim uart_cmd_test_player_anim uart_cmd_test_castle_anim
uart_cmd_test_promote_anim uart_cmd_test_endgame_anim uart_cmd_endgame_wave
uart_cmd_endgame_circles uart_cmd_endgame_cascade uart_cmd_endgame_fireworks
uart_cmd_endgame_draw_spiral uart_cmd_endgame_draw_pulse uart_cmd_stop_endgame
uart_cmd_help_web uart_cmd_help_app uart_cmd_help_game uart_cmd_help_system
uart_cmd_help_beginner uart_cmd_help_debug uart_cmd_verbose uart_cmd_quiet
uart_cmd_version uart_cmd_memory uart_cmd_history uart_cmd_start_pos_check
uart_display_advantage_graph uart_display_led_board uart_cmd_cli uart_cli_print_help
""".split()
lines = text.splitlines(keepends=True)
out = []
i = 0
removed = []
while i < len(lines):
    line = lines[i]
    stripped = line.strip()
    hit = None
    for n in names:
        token = n + "("
        if token in stripped and (
            stripped.startswith("command_result_t")
            or stripped.startswith("void")
            or stripped.startswith("static void")
        ):
            hit = n
            break
    if not hit:
        out.append(line)
        i += 1
        continue
    if stripped.endswith(";"):
        removed.append(hit + " decl")
        i += 1
        continue
    depth = 0
    in_str = None
    escape = False
    started = False
    while i < len(lines):
        s = lines[i]
        j = 0
        while j < len(s):
            c = s[j]
            if in_str:
                if escape:
                    escape = False
                elif c == "\\":
                    escape = True
                elif c == in_str:
                    in_str = None
            else:
                if c in ('"', "'"):
                    in_str = c
                elif c == "/" and j + 1 < len(s) and s[j + 1] == "/":
                    break
                elif c == "{":
                    depth += 1
                    started = True
                elif c == "}":
                    depth -= 1
            j += 1
        i += 1
        if started and depth <= 0:
            break
    removed.append(hit)
text2 = "".join(out)
while "\n\n\n\n" in text2:
    text2 = text2.replace("\n\n\n\n", "\n\n\n")
p.write_text(text2, encoding="utf-8")
print("removed", len(removed))
for item in removed:
    print(item)
