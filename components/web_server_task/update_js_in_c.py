import os

JS_PATH = '/Users/alfred/Documents/my_local_projects/free_chess_v1_mqtt_HA/components/web_server_task/chess_app.js'
C_PATH = '/Users/alfred/Documents/my_local_projects/free_chess_v1_mqtt_HA/components/web_server_task/web_server_task.c'

def format_js_as_c_string(js_content):
    lines = js_content.split('\n')
    c_lines = []
    for line in lines:
        # Escape backslashes first, then quotes
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

    # Find where the variable content actually starts (skip the marker line)
    content_start_idx = c_content.find('\n', start_idx) + 1
    
    # Find the end marker
    end_idx = c_content.find(end_marker)
    if end_idx == -1:
        print("Error: End marker not found")
        return
        
    # The variable definition ends before the function definition. 
    # We need to find the specific semicolon before the function definition.
    # But actually, my format_js_as_c_string appends ';\n'.
    # So we just need to replace everything between start_marker line end and end_marker start.
    
    # Let's double check the existing space. 
    # The existing file has `;\n\nstatic esp_err_t ...`
    # So if we replace up to `static esp_err_t`, we should be careful about newlines.
    
    # Look backwards from end_marker for the semicolon of the previous statement?
    # No, risky. 
    
    # Let's look at the file content again.
    # line 5383:     "}\n";
    # line 5384: 
    # line 5385: static esp_err_t http_get_chess_js_handler...
    
    # So we want to replace from (start_idx + len(start_marker)) up to (end_idx).
    # But wait, start_marker line in C file ends with ` =`.
    # Then next line starts the string.
    
    # Actually, simpler:
    # 1. Keep everything before `static const char chess_app_js_content[] =`
    # 2. Append `static const char chess_app_js_content[] =\n`
    # 3. Append `formatted_js` (which ends with `;\n`)
    # 4. Append `\n`
    # 5. Keep everything including and after `static esp_err_t http_get_chess_js_handler`
    
    pre_content = c_content[:start_idx]
    post_content = c_content[end_idx:]
    
    new_content = pre_content + start_marker + '\n' + formatted_js + '\n' + post_content
    
    with open(C_PATH, 'w', encoding='utf-8') as f:
        f.write(new_content)
        
    print(f"Successfully updated {C_PATH} with content from {JS_PATH}")

if __name__ == '__main__':
    main()
