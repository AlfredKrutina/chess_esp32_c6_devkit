# Lokální backlog diagramů

Soubor **`LOCAL_DIAGRAM_BACKLOG.md`** je v **`.gitignore`** — do gitu nepatří (dlouhé poznámky, nápady, rozpracované seznamy).

**Co udělat lokálně**

1. Zkopíruj tento soubor:

   `cp docs/diagrams/DIAGRAM_BACKLOG.local.example.md docs/diagrams/LOCAL_DIAGRAM_BACKLOG.md`

2. Obsah **`LOCAL_DIAGRAM_BACKLOG.md`** si rozšiřuj sám (škola diagramů, TODO, barvy). Šablona s „skoro vším“ je v repu jen jako start — plnou verzi si drž lokálně nebo ji vygeneruj podle vlastních potřeb.

**Minimum v `LOCAL_DIAGRAM_BACKLOG.md`**

- TODO diagramů (firmware / Flutter / CI / GPIO).
- Paleta barev + zkopírovatelné `%%{init}%%` / `classDef`.
- Odkazy na zdrojové soubory.

Po prvním naklonování repa si soubor **vytvoř** (git ho neobsahuje):

`cp docs/diagrams/DIAGRAM_BACKLOG.local.example.md docs/diagrams/LOCAL_DIAGRAM_BACKLOG.md`

…a doplň obsah — dlouhou šablonu s řadou nápadů máš ideálně právě v tom gitignored souboru u sebe na disku (není sdílená přes remote).

Veřejně v repu jsou barevné diagramy v **`README.md`** tady a v **`sources/*.mmd`** (+ SVG po `./scripts/render_docs.sh`).
