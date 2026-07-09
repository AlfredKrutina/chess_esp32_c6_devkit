#!/usr/bin/env python3
"""
Convert web/chess_app.js to C string array for embedding in web_server_task.c (stdout).

Spuštění z kořene repa:
  python3 components/web_server_task/tools/js_to_c.py
"""

import os
import sys

TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
COMPONENT_DIR = os.path.dirname(TOOLS_DIR)
JS_FILE = os.path.join(COMPONENT_DIR, "web", "chess_app.js")

def js_to_c_string(js_file):
    with open(js_file, 'r', encoding='utf-8') as f:
        lines = f.readlines()
    c_lines = []
    for line in lines:
        line = line.rstrip('\n')
        line = line.replace('\\', '\\\\')
        line = line.replace('"', '\\"')
        c_lines.append(f'    "{line}\\n"')
    return c_lines

def main():
    if not os.path.isfile(JS_FILE):
        print(f"Error: {JS_FILE} not found", file=sys.stderr)
        sys.exit(1)

    c_lines = js_to_c_string(JS_FILE)
    with open(JS_FILE, 'rb') as f:
        byte_count = len(f.read())
    line_count = len(c_lines)

    print(f'// chess_app.js embedded ({byte_count} bytes, {line_count} lines)')
    print('static const char chess_app_js_content[] =')
    for i, line in enumerate(c_lines):
        if i < len(c_lines) - 1:
            print(line)
        else:
            print(line + ';')

if __name__ == '__main__':
    main()
