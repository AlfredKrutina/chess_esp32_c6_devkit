# GitHub Pages Setup - CZECHMATE Dokumentace

Tento dokument popisuje, jak nastavit a nasadit Doxygen dokumentaci na GitHub Pages.

## 📋 Přehled

GitHub Pages umožňuje zobrazit HTML dokumentaci přímo na GitHubu. Dokumentace je automaticky dostupná na URL:
```
https://[username].github.io/[repo-name]/
```

## 🚀 Rychlý start

### ⚠️ Důležité: Git Repozitář

**Pokud nemáte tuto složku jako git repozitář**, použijte `prepare_gh_pages.sh` místo `update_gh_pages.sh`. Více informací v sekci [Nasazení bez git repozitáře](#nasazení-bez-git-repozitáře).

### 1. Vygenerovat dokumentaci

Ujistěte se, že máte vygenerovanou Doxygen dokumentaci:

```bash
./generate_docs.sh
```

Tento skript:
- Vygeneruje HTML dokumentaci do `docs/doxygen/html/`
- Zkopíruje dokumentaci do `docs/public_doxygen/html/` (pro GitHub)
- Zajistí, že `diagrams_mermaid.html` je součástí výstupu

### 2. Nasazení na GitHub Pages

**Metoda A: S git repozitářem (automatické)**

Spusťte skript pro nasazení:

```bash
./update_gh_pages.sh
```

Tento skript:
- Zkontroluje, že dokumentace existuje
- Vytvoří nebo aktualizuje `gh-pages` branch
- Zkopíruje HTML dokumentaci do root gh-pages branch
- Vytvoří `.nojekyll` soubor (důležité pro GitHub Pages)
- Pushne změny na GitHub

**Metoda B: Bez git repozitáře (manuální)**

Pokud nemáte tuto složku jako git repozitář, použijte:

```bash
./prepare_gh_pages.sh
```

Tento skript:
- Zkontroluje, že dokumentace existuje
- Vytvoří složku `gh-pages-ready/` s připravenou dokumentací
- Zkopíruje HTML dokumentaci do `gh-pages-ready/`
- Vytvoří `.nojekyll` soubor
- Vytvoří `README.md` s instrukcemi pro nasazení

Pak nahrajte obsah složky `gh-pages-ready/` na GitHub (viz sekce [Nasazení bez git repozitáře](#nasazení-bez-git-repozitáře)).

### 3. Aktivace GitHub Pages v GitHub UI

1. Přejděte do vašeho GitHub repozitáře
2. Klikněte na **Settings** (vpravo nahoře)
3. V levém menu klikněte na **Pages**
4. V sekci **Build and deployment** → **Source** vyberte:
   - **Deploy from a branch** (dropdown)
5. V sekci **Branch** nastavte:
   - **Branch**: `gh-pages` (ne `main`!)
   - **Folder**: `/ (root)` (ne `/docs`!)
6. Klikněte na **Save**

**⚠️ Důležité:**
- **Branch musí být `gh-pages`**, ne `main`
- **Folder musí být `/ (root)`**, ne `/docs`
- Složka `/docs` je pro Jekyll, ale my používáme statickou HTML dokumentaci v root gh-pages branch

### 4. Čekání na nasazení

- GitHub Pages obvykle nasadí dokumentaci během 1-2 minut
- Můžete sledovat stav nasazení v **Settings → Pages → Build and deployment**
- Po nasazení bude dokumentace dostupná na: `https://[username].github.io/[repo-name]/`

## 📁 Struktura dokumentace na GitHub Pages

Po nasazení bude struktura v gh-pages branch vypadat takto:

```
gh-pages/
├── .nojekyll              # Zakáže Jekyll processing (důležité!)
├── index.html             # Hlavní stránka dokumentace
├── diagrams_mermaid.html   # Mermaid diagramy
├── *.html                 # Všechny další HTML stránky
├── *.css                  # CSS soubory
├── *.js                   # JavaScript soubory
└── search/                # Vyhledávací funkce
```

## 🔗 Odkazy v dokumentaci

Dokumentace obsahuje následující odkazy:

- **Hlavní stránka**: `index.html` - Kompletní Doxygen dokumentace
- **Mermaid diagramy**: `diagrams_mermaid.html` - Sekvenční diagramy všech programových flow
- **README.md**: Je použito jako hlavní stránka v Doxygen (nastaveno v Doxyfile)

### Jak fungují odkazy

- Odkazy v README.md (např. `[Mermaid Sequence Diagramy](diagrams_mermaid.html)`) jsou zpracovány Doxygenem
- Doxygen automaticky upraví relativní cesty tak, aby fungovaly v HTML kontextu
- `diagrams_mermaid.html` je přidán do HTML výstupu pomocí `HTML_EXTRA_FILES` v Doxyfile

## 🔧 Konfigurace

### Doxyfile nastavení

Důležité nastavení v `Doxyfile`:

```ini
# Použít README.md jako hlavní stránku
USE_MDFILE_AS_MAINPAGE = README.md

# Přidat diagrams_mermaid.html do HTML výstupu
HTML_EXTRA_FILES = docs/diagrams_mermaid.html

# Výstupní adresář
OUTPUT_DIRECTORY = docs/doxygen
HTML_OUTPUT = html
```

### update_gh_pages.sh

Skript automaticky:
- Zkontroluje existenci dokumentace
- Vytvoří `.nojekyll` soubor (důležité pro GitHub Pages)
- Zkopíruje všechny soubory z `docs/doxygen/html/` do root gh-pages branch
- Pushne změny na GitHub

## 🐛 Řešení problémů

### Dokumentace se nezobrazuje

1. **Zkontrolujte GitHub Pages nastavení**:
   - Settings → Pages → Source musí být nastaveno na `gh-pages` branch
   - Počkejte 1-2 minuty na nasazení

2. **Zkontrolujte .nojekyll soubor**:
   - V gh-pages branch musí existovat `.nojekyll` soubor
   - Tento soubor zakáže Jekyll processing, který by mohl rozbít odkazy

3. **Zkontrolujte, že dokumentace byla vygenerována**:
   ```bash
   ls -la docs/doxygen/html/index.html
   ```

4. **Zkontrolujte gh-pages branch**:
   ```bash
   git checkout gh-pages
   ls -la index.html
   ```

### Odkazy nefungují

1. **Zkontrolujte, že diagrams_mermaid.html existuje**:
   ```bash
   ls -la docs/doxygen/html/diagrams_mermaid.html
   ```

2. **Zkontrolujte Doxyfile**:
   - Mělo by obsahovat: `HTML_EXTRA_FILES = docs/diagrams_mermaid.html`

3. **Regenerujte dokumentaci**:
   ```bash
   ./generate_docs.sh
   ```

### Push selhal

1. **Zkontrolujte oprávnění**:
   - Musíte mít oprávnění push do gh-pages branch

2. **Zkuste push ručně**:
   ```bash
   git checkout gh-pages
   git push -u origin gh-pages
   ```

## 📝 Aktualizace dokumentace

Při každé změně v kódu nebo dokumentaci:

1. **Vygenerujte novou dokumentaci**:
   ```bash
   ./generate_docs.sh
   ```

2. **Nasajte na GitHub Pages**:
   ```bash
   ./update_gh_pages.sh
   ```

3. **Počkejte na nasazení** (1-2 minuty)

## 🔍 Ověření

Po nasazení můžete ověřit, že vše funguje:

1. **Otevřete hlavní stránku**:
   ```
   https://[username].github.io/[repo-name]/
   ```

2. **Otevřete Mermaid diagramy**:
   ```
   https://[username].github.io/[repo-name]/diagrams_mermaid.html
   ```

3. **Zkontrolujte odkazy**:
   - V README.md by měl být odkaz na `diagrams_mermaid.html`
   - Kliknutím na odkaz by se měly otevřít Mermaid diagramy

## 📦 Nasazení bez git repozitáře

Pokud nemáte tuto složku jako git repozitář, použijte `prepare_gh_pages.sh`:

### Krok 1: Připravit dokumentaci

```bash
./prepare_gh_pages.sh
```

Tento skript vytvoří složku `gh-pages-ready/` s připravenou dokumentací.

### Krok 2: Nahrát na GitHub

**Možnost A: Přes GitHub Web UI (nejjednodušší)**

1. Vytvořte nový repozitář na GitHubu (nebo použijte existující)
2. V repozitáři vytvořte nový branch s názvem `gh-pages`:
   - GitHub → Branches → New branch → název: `gh-pages`
3. Nahrajte obsah složky `gh-pages-ready/` do root gh-pages branch:
   - GitHub → Upload files → vyberte všechny soubory z `gh-pages-ready/`
4. Commit změny
5. Aktivujte GitHub Pages:
   - Settings → Pages → Source: `gh-pages` branch → Save

**Možnost B: Přes Git (pokud vytvoříte git repo)**

```bash
cd gh-pages-ready
git init
git checkout -b gh-pages
git add .
git commit -m "Add documentation"
git remote add origin https://github.com/[username]/[repo-name].git
git push -u origin gh-pages
```

### Krok 3: Aktivace GitHub Pages

1. Přejděte do vašeho GitHub repozitáře
2. Klikněte na **Settings** (vpravo nahoře)
3. V levém menu klikněte na **Pages**
4. V sekci **Build and deployment** → **Source** vyberte:
   - **Deploy from a branch** (dropdown)
5. V sekci **Branch** nastavte:
   - **Branch**: `gh-pages` (ne `main`!)
   - **Folder**: `/ (root)` (ne `/docs`!)
6. Klikněte na **Save**

**⚠️ Důležité:**
- **Branch musí být `gh-pages`**, ne `main`
- **Folder musí být `/ (root)`**, ne `/docs`
- Složka `/docs` je pro Jekyll, ale my používáme statickou HTML dokumentaci v root gh-pages branch

### Krok 4: Čekání na nasazení

- GitHub Pages obvykle nasadí dokumentaci během 1-2 minut
- Můžete sledovat stav nasazení v **Settings → Pages → Build and deployment**
- Po nasazení bude dokumentace dostupná na: `https://[username].github.io/[repo-name]/`

## 📚 Další informace

- [GitHub Pages dokumentace](https://docs.github.com/en/pages)
- [Doxygen dokumentace](https://www.doxygen.nl/manual/index.html)
- [Mermaid diagramy](https://mermaid.js.org/)

---

**Verze dokumentace:** 2.4.0  
**Poslední aktualizace:** 2025-01-10
