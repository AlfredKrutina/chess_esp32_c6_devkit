#!/usr/bin/env python3
"""
Re-embed chess_app.js into web_server_task.c (replaces chess_app_js_content array).
Run from project root or from components/web_server_task.
"""
import os
import re
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
JS_FILE = os.path.join(SCRIPT_DIR, "chess_app.js")
C_FILE = os.path.join(SCRIPT_DIR, "web_server_task.c")

def js_to_c_lines(js_path):
    with open(js_path, "r", encoding="utf-8") as f:
        lines = f.readlines()
    c_lines = []
    for line in lines:
        line = line.rstrip("\n")
        line = line.replace("\\", "\\\\").replace('"', '\\"')
        c_lines.append(f'    "{line}\\n"')
    return c_lines

def main():
    if not os.path.isfile(JS_FILE):
        print(f"Error: {JS_FILE} not found", file=sys.stderr)
        sys.exit(1)
    if not os.path.isfile(C_FILE):
        print(f"Error: {C_FILE} not found", file=sys.stderr)
        sys.exit(1)

    with open(JS_FILE, "rb") as f:
        byte_count = len(f.read())
    c_lines = js_to_c_lines(JS_FILE)
    line_count = len(c_lines)
    header = (
        "// ============================================================================\n"
        "// TEST PAGE - MINIMAL TIMER TEST (for debugging)\n"
        f"// chess_app.js embedded ({byte_count} bytes, {line_count} lines)\n"
        "static const char chess_app_js_content[] =\n"
    )
    array_body = "\n".join(c_lines[:-1]) + "\n" + c_lines[-1] + ";\n\n"

    with open(C_FILE, "r", encoding="utf-8") as f:
        content = f.read()

    start_marker = (
        "// ============================================================================\n"
        "// TEST PAGE - MINIMAL TIMER TEST (for debugging)\n"
    )
    end_marker = "static esp_err_t http_get_chess_js_handler(httpd_req_t *req)"

    start_idx = content.find(start_marker)
    if start_idx == -1:
        print("Error: start marker not found in web_server_task.c", file=sys.stderr)
        sys.exit(1)
    end_idx = content.find(end_marker, start_idx)
    if end_idx == -1:
        print("Error: end marker not found in web_server_task.c", file=sys.stderr)
        sys.exit(1)

    new_block = header + array_body
    new_content = content[:start_idx] + new_block + content[end_idx:]
    with open(C_FILE, "w", encoding="utf-8") as f:
        f.write(new_content)
    print(f"Updated web_server_task.c: embedded chess_app.js ({byte_count} bytes, {line_count} lines)")

if __name__ == "__main__":
    main()
