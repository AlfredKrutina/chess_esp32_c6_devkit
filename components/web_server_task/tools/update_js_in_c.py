from pathlib import Path

_TOOLS_DIR = Path(__file__).resolve().parent
_COMPONENT_DIR = _TOOLS_DIR.parent
JS_PATH = str(_COMPONENT_DIR / "web" / "chess_app.js")
C_PATH = str(_COMPONENT_DIR / "web_server_task.c")

def format_js_as_c_string(js_content):
    lines = js_content.split('\n')
    c_lines = []
    for line in lines:
        escaped_line = line.replace('\\', '\\\\').replace('"', '\\"')
        c_lines.append(f'    "{escaped_line}\\n"')
    return '\n'.join(c_lines) + ';\n'

def main():
    with open(JS_PATH, 'r', encoding='utf-8') as f:
        js_content = f.read()

    formatted_js = format_js_as_c_string(js_content)

    with open(C_PATH, 'r', encoding='utf-8') as f:
        c_content = f.read()

    start_marker = 'static const char chess_app_js_content[] ='
    end_marker = 'static esp_err_t http_get_chess_js_handler(httpd_req_t *req) {'

    start_idx = c_content.find(start_marker)
    if start_idx == -1:
        print("Error: Start marker not found")
        return

    end_idx = c_content.find(end_marker)
    if end_idx == -1:
        print("Error: End marker not found")
        return

    pre_content = c_content[:start_idx]
    post_content = c_content[end_idx:]

    new_content = pre_content + start_marker + '\n' + formatted_js + '\n' + post_content

    with open(C_PATH, 'w', encoding='utf-8') as f:
        f.write(new_content)

    print(f"Successfully updated {C_PATH} with content from {JS_PATH}")

if __name__ == '__main__':
    main()
