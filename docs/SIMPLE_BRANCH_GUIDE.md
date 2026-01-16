# Jednoduchý návod - GitHub Branche

## 🎯 Jak to funguje - jednoduše

**Dva branche = Dvě věci:**

### 📁 `main` branch = PROJEKT
```
main branch obsahuje:
├── components/        ← Projektové soubory
├── main/             ← Projektové soubory
├── CMakeLists.txt    ← Projektové soubory
├── README.md         ← Projektové soubory
└── ...               ← Všechno z projektu
```

### 📄 `gh-pages` branch = DOKUMENTACE
```
gh-pages branch obsahuje:
├── index.html        ← Jen dokumentace
├── .nojekyll         ← Jen dokumentace
├── *.html            ← Jen dokumentace
├── search/           ← Jen dokumentace
└── ...               ← JEN HTML soubory z Doxygenu
```

## ✅ Jak to správně nastavit (krok za krokem)

### Krok 1: Zkontrolovat, kde jste

```bash
cd /Users/alfred/Documents/GitHub/chess_esp32_c6_devkit
git branch --show-current
```

**Pokud vidíte `gh-pages`** → Jste na dokumentaci (správně pro GitHub Pages)
**Pokud vidíte `main`** → Jste na projektu (správně pro projekt)

### Krok 2: Nastavit main branch (projekt)

```bash
# Přepnout na main
git checkout main

# Pokud main neexistuje, vytvoř ho:
# git checkout -b main

# Zkopírovat projekt (kromě .git)
cp -r "/Users/alfred/Documents/my_local_projects/free_chess_v1 copy_working_10.1.2026"/* .
rm -rf .git  # Odstranit .git z projektu

# Commit
git add .
git commit -m "Project files"
git push -u origin main
```

### Krok 3: Nastavit gh-pages branch (dokumentace)

```bash
# Přepnout na gh-pages
git checkout gh-pages

# Odstranit vše kromě .git (gh-pages má obsahovat JEN dokumentaci)
find . -mindepth 1 -maxdepth 1 ! -name '.git' -exec rm -rf {} + 2>/dev/null

# Zkopírovat dokumentaci
cp -r "/Users/alfred/Documents/my_local_projects/free_chess_v1 copy_working_10.1.2026/docs/doxygen/html"/* .
touch .nojekyll

# Commit
git add .
git commit -m "Documentation"
git push -u origin gh-pages
```

## 🔄 Jak přepnout mezi branchi

### Na projekt (main):
```bash
git checkout main
```

### Na dokumentaci (gh-pages):
```bash
git checkout gh-pages
```

## ⚠️ DŮLEŽITÉ PRAVIDLO

**Když jste na `gh-pages` branch:**
- ✅ Můžete mazat soubory (obsahuje jen dokumentaci)
- ❌ Neměly by tam být složky projektu (components/, main/, atd.)

**Když jste na `main` branch:**
- ✅ Můžete pracovat s projektem
- ❌ NIKDY nemazat soubory projektu!

## 📊 Kontrola - jak zjistit, co je kde

### Zkontrolovat main branch:
```bash
git checkout main
ls -la | head -10
# Měly by být vidět: components/, main/, CMakeLists.txt, atd.
```

### Zkontrolovat gh-pages branch:
```bash
git checkout gh-pages
ls -la | head -10
# Měly by být vidět: index.html, *.html, .nojekyll, search/, atd.
# NEMĚLY by být: components/, main/, CMakeLists.txt
```

## 🎯 Shrnutí

- **main branch** = Projektový kód → používá se pro normální vývoj
- **gh-pages branch** = Dokumentace HTML → používá se pro GitHub Pages

**Tyto dva branche jsou ÚPLNĚ ODSEJENÉ** - každý má jiný obsah!

---

**Datum:** 2025-01-16  
**Verze:** 2.4.0
