#!/usr/bin/env python3
"""
Skript pro generování HTML stránky s Mermaid diagramy z docs/diagrams/mermaid_diagrams.txt
"""

import re
import html


def _is_txt_section_rule_line(line_stripped):
    """Oddělovač sekcí v mermaid_diagrams.txt (# jen znaky =). Nesmí se vkládat do Mermaid."""
    return bool(re.match(r"^#\s*=+\s*$", line_stripped))


def parse_mermaid_file(filename):
    """Parsuje soubor s Mermaid diagramy a vrací strukturovaná data"""
    with open(filename, 'r', encoding='utf-8') as f:
        content = f.read()
    
    sections = []
    lines = content.split('\n')
    i = 0
    
    while i < len(lines):
        line = lines[i]
        
        # Sekce: # ČÁST A: NÁZEV
        section_match = re.match(r'^# ČÁST ([A-J]): (.+)$', line)
        if section_match:
            section = {
                'letter': section_match.group(1),
                'name': section_match.group(2),
                'subsections': []
            }
            sections.append(section)
            i += 1
            continue
        
        # Podsekce: # A1. Název
        subsection_match = re.match(r'^# ([A-J][0-9]+)\. (.+)$', line)
        if subsection_match and sections:
            subsection_id = subsection_match.group(1)
            subsection_name = subsection_match.group(2)
            
            # Najít komentář popis (řádky po nadpisu, které začínají #)
            description_lines = []
            i += 1
            while i < len(lines) and lines[i].startswith('#'):
                desc_line = lines[i].strip()
                if desc_line.startswith('# ') and not desc_line.startswith('# =') and not desc_line.startswith('# -'):
                    description_lines.append(desc_line[2:].strip())
                i += 1
            
            # Najít Mermaid kód (sequenceDiagram až do dalšího # nebo konce)
            mermaid_code = []
            while i < len(lines):
                if lines[i].strip().startswith('sequenceDiagram'):
                    # Začátek Mermaid kódu
                    mermaid_code.append(lines[i])
                    i += 1
                    # Pokračovat až do dalšího # (kromě komentářů v kódu)
                    while i < len(lines):
                        line_stripped = lines[i].strip()
                        if _is_txt_section_rule_line(line_stripped):
                            break
                        # Pokud je to nová sekce/podsekce (začíná # a není to pokračování kódu)
                        if line_stripped.startswith('# ') and not line_stripped.startswith('#    '):
                            # Zkontrolovat, jestli to není komentář v kódu (začíná na začátku řádku)
                            if not line_stripped.startswith('# =') and not line_stripped.startswith('# -'):
                                # Možná nová sekce/podsekce, ale zkontrolovat kontext
                                if not any(c in line_stripped for c in ['participant', 'Note over', '->>', '-->>', 'activate', 'deactivate', 'loop', 'alt', 'opt', 'rect']):
                                    break
                        mermaid_code.append(lines[i])
                        i += 1
                    break
                elif lines[i].strip() and not lines[i].strip().startswith('#'):
                    # Ne-prázdný řádek, který není komentář - možná začátek kódu
                    if 'sequenceDiagram' in lines[i] or lines[i].strip().startswith('sequenceDiagram'):
                        mermaid_code.append(lines[i])
                        i += 1
                        while i < len(lines):
                            line_stripped = lines[i].strip()
                            if _is_txt_section_rule_line(line_stripped):
                                break
                            if line_stripped.startswith('# ') and not line_stripped.startswith('#    ') and not line_stripped.startswith('# =') and not line_stripped.startswith('# -'):
                                if not any(keyword in line_stripped for keyword in ['participant', 'Note', '->>', '-->>']):
                                    break
                            mermaid_code.append(lines[i])
                            i += 1
                        break
                    else:
                        i += 1
                else:
                    i += 1
            
            mermaid_text = '\n'.join(mermaid_code).strip()
            
            subsection = {
                'id': subsection_id,
                'name': subsection_name,
                'description': ' '.join(description_lines) if description_lines else '',
                'mermaid_code': mermaid_text
            }
            
            sections[-1]['subsections'].append(subsection)
            continue
        
        i += 1
    
    return sections

def generate_html(sections, output_file):
    """Generuje HTML stránku s diagramy"""
    
    # Generovat navigaci (TOC)
    toc_html = ['<nav class="toc">', '<h2>Navigace</h2>', '<ul>']
    
    for section in sections:
        section_id = f"section-{section['letter'].lower()}"
        toc_html.append(f'  <li class="section"><a href="#{section_id}">{section["letter"]}. {section["name"]}</a>')
        if section['subsections']:
            toc_html.append('    <ul class="subsection-list">')
            for sub in section['subsections']:
                anchor_id = f"diagram-{sub['id'].lower()}"
                toc_html.append(f'      <li><a href="#{anchor_id}">{sub["id"]}. {html.escape(sub["name"])}</a></li>')
            toc_html.append('    </ul>')
        toc_html.append('  </li>')
    
    toc_html.extend(['</ul>', '</nav>'])
    toc_html_str = '\n'.join(toc_html)
    
    # Generovat obsah
    content_html = ['<main class="content">']
    content_html.append('<h1>CZECHMATE firmware — sekvenční diagramy</h1>')
    content_html.append('<p class="intro">Sekvence z <code>mermaid_diagrams.txt</code>. Obrázky tasků a front: <a href="README.md">README.md</a> ve stejné složce (nebo <code>docs/diagrams/README.md</code> v repu). Verze firmware je v kořenovém <code>CMakeLists.txt</code>.</p>')
    
    for section in sections:
        section_id = f"section-{section['letter'].lower()}"
        content_html.append(f'<section id="{section_id}" class="section">')
        content_html.append(f'  <h2>{section["letter"]}. {html.escape(section["name"])}</h2>')
        
        for sub in section['subsections']:
            anchor_id = f"diagram-{sub['id'].lower()}"
            content_html.append(f'  <div id="{anchor_id}" class="diagram-container">')
            content_html.append(f'    <h3>{sub["id"]}. {html.escape(sub["name"])}</h3>')
            
            if sub['description']:
                content_html.append(f'    <p class="diagram-description">{html.escape(sub["description"])}</p>')
            
            # Mermaid diagram
            mermaid_code_escaped = html.escape(sub['mermaid_code'])
            content_html.append('    <div class="mermaid">')
            content_html.append(mermaid_code_escaped)
            content_html.append('    </div>')
            
            content_html.append('  </div>')
            content_html.append('')
        
        content_html.append('</section>')
        content_html.append('')
    
    content_html.append('</main>')
    content_html_str = '\n'.join(content_html)
    
    # Kompletní HTML
    html_template = f'''<!DOCTYPE html>
<html lang="cs">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>CZECHMATE — Mermaid diagramy (sekvenční)</title>
    <script src="https://cdn.jsdelivr.net/npm/mermaid@10/dist/mermaid.min.js"></script>
    <style>
        * {{
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }}
        
        body {{
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif;
            line-height: 1.6;
            color: #e2e8f0;
            background-color: #0f172a;
        }}
        
        .container {{
            display: flex;
            min-height: 100vh;
        }}
        
        .toc {{
            position: fixed;
            left: 0;
            top: 0;
            width: 280px;
            height: 100vh;
            background-color: #2d3748;
            color: #e2e8f0;
            padding: 20px;
            overflow-y: auto;
            box-shadow: 2px 0 5px rgba(0,0,0,0.1);
        }}
        
        .toc h2 {{
            color: #fff;
            font-size: 1.2em;
            margin-bottom: 15px;
            padding-bottom: 10px;
            border-bottom: 2px solid #4a5568;
        }}
        
        .toc ul {{
            list-style: none;
        }}
        
        .toc ul ul {{
            margin-left: 15px;
            margin-top: 5px;
        }}
        
        .toc li {{
            margin: 5px 0;
        }}
        
        .toc a {{
            color: #cbd5e0;
            text-decoration: none;
            display: block;
            padding: 5px 10px;
            border-radius: 4px;
            transition: background-color 0.2s;
        }}
        
        .toc a:hover {{
            background-color: #4a5568;
            color: #fff;
        }}
        
        .toc .section > a {{
            font-weight: bold;
            color: #fff;
        }}
        
        .content {{
            margin-left: 280px;
            padding: 40px;
            max-width: 1200px;
            background-color: #111827;
            min-height: 100vh;
        }}
        
        .content h1 {{
            color: #f8fafc;
            font-size: 1.75em;
            margin-bottom: 0.75em;
        }}
        
        .content a {{
            color: #38bdf8;
        }}
        
        .content a:hover {{
            color: #7dd3fc;
        }}
        
        .intro {{
            font-size: 1.1em;
            color: #cbd5e1;
            margin-bottom: 30px;
            padding: 15px;
            background-color: #1e293b;
            border-left: 4px solid #38bdf8;
            border-radius: 4px;
        }}
        
        .section {{
            margin-bottom: 50px;
        }}
        
        .section h2 {{
            color: #f8fafc;
            font-size: 2em;
            margin-bottom: 30px;
            padding-bottom: 10px;
            border-bottom: 3px solid #38bdf8;
        }}
        
        .diagram-container {{
            margin-bottom: 60px;
            padding: 20px;
            background-color: #1e293b;
            border-radius: 8px;
            box-shadow: 0 2px 8px rgba(0,0,0,0.35);
            border: 1px solid #334155;
        }}
        
        .diagram-container h3 {{
            color: #f1f5f9;
            font-size: 1.5em;
            margin-bottom: 15px;
            padding-bottom: 10px;
            border-bottom: 2px solid #475569;
        }}
        
        .diagram-description {{
            color: #cbd5e1;
            margin-bottom: 20px;
            font-style: italic;
            padding: 10px;
            background-color: #0f172a;
            border-left: 3px solid #4ade80;
            border-radius: 4px;
        }}
        
        .mermaid {{
            background-color: #0f172a;
            padding: 20px;
            border-radius: 4px;
            margin: 20px 0;
            overflow-x: auto;
            border: 1px solid #334155;
        }}
        
        @media (max-width: 768px) {{
            .toc {{
                position: relative;
                width: 100%;
                height: auto;
                max-height: 300px;
            }}
            
            .content {{
                margin-left: 0;
                padding: 20px;
            }}
        }}
    </style>
</head>
<body>
    <div class="container">
        {toc_html_str}
        {content_html_str}
    </div>
    
    <script>
        mermaid.initialize({{
            startOnLoad: true,
            theme: 'dark',
            securityLevel: 'loose',
            sequence: {{
                useMaxWidth: true,
                mirrorActors: false
            }},
            flowchart: {{
                useMaxWidth: true,
                htmlLabels: true,
                curve: 'basis'
            }}
        }});
    </script>
</body>
</html>
'''
    
    with open(output_file, 'w', encoding='utf-8') as f:
        f.write(html_template)
    
    print(f"HTML stránka vygenerována: {output_file}")

if __name__ == '__main__':
    import sys
    
    input_file = 'docs/diagrams/mermaid_diagrams.txt'
    output_file = 'docs/diagrams/diagrams_mermaid.html'
    
    if len(sys.argv) > 1:
        input_file = sys.argv[1]
    if len(sys.argv) > 2:
        output_file = sys.argv[2]
    
    print(f"Parsování {input_file}...")
    sections = parse_mermaid_file(input_file)
    
    print(f"Nalezeno {len(sections)} sekcí:")
    total_diagrams = 0
    for section in sections:
        count = len(section['subsections'])
        total_diagrams += count
        print(f"  {section['letter']}: {section['name']} ({count} diagramů)")
    
    print(f"\nCelkem {total_diagrams} diagramů")
    print(f"\nGenerování HTML do {output_file}...")
    generate_html(sections, output_file)
    print("Hotovo!")
