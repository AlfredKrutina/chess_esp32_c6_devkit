#!/usr/bin/env python3
"""
Convert chess_app.js to C string array for embedding in web_server_task.c
"""

import sys

def js_to_c_string(js_file):
    """Convert JavaScript file to C string array"""
    with open(js_file, 'r', encoding='utf-8') as f:
        lines = f.readlines()
    
    c_lines = []
    for line in lines:
        # Escape special characters
        line = line.rstrip('\n')
        line = line.replace('\\', '\\\\')  # Backslash first
        line = line.replace('"', '\\"')    # Quote
        c_lines.append(f'    "{line}\\n"')
    
    return c_lines

def main():
    js_file = 'chess_app.js'
    
    # Read JS file
    c_lines = js_to_c_string(js_file)
    
    # Count lines and bytes
    with open(js_file, 'rb') as f:
        byte_count = len(f.read())
    
    line_count = len(c_lines)
    
    # Generate C array
    print(f'// chess_app.js embedded ({byte_count} bytes, {line_count} lines)')
    print('static const char chess_app_js_content[] =')
    for i, line in enumerate(c_lines):
        if i < len(c_lines) - 1:
            print(line)
        else:
            print(line + ';')

if __name__ == '__main__':
    main()
