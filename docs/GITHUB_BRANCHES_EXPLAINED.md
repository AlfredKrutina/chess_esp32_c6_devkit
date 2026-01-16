# GitHub Branche - Jednoduché vysvětlení

## 🎯 Jak to má fungovat

GitHub repozitář má **dva branche** pro dvě různé věci:

### 1. `main` branch (hlavní projekt)
- **Obsahuje:** Celý projekt (kód, komponenty, soubory)
- **Co tam je:** `components/`, `main/`, `CMakeLists.txt`, `README.md`, atd.
- **Účel:** Hlavní projektový kód
- **Nepoužívá se pro:** GitHub Pages

### 2. `gh-pages` branch (dokumentace)
- **Obsahuje:** JEN HTML dokumentaci z Doxygenu
- **Co tam je:** `index.html`, `.nojekyll`, `*.html`, `*.js`, `*.css`, `search/`, atd.
- **Účel:** GitHub Pages - zobrazuje dokumentaci na webu
- **NEPOUŽÍVÁ se pro:** Projektový kód

## 📊 Jak to zkontrolovat

### Zkontrolovat, na kterém branchi jste:

```bash
cd /Users/alfred/Documents/GitHub/chess_esp32_c6_devkit
git branch --show-current
```

### Zkontrolovat, co je v aktuálním branchi:

```bash
# Pokud jste na main - měly by být vidět:
ls -la  # components/, main/, CMakeLists.txt, atd.

# Pokud jste na gh-pages - měly by být vidět:
ls -la  # index.html, *.html, .nojekyll, search/, atd. (JEN dokumentace)
```

## 🔄 Jak přepnout mezi branchi

### Přepnout na main (projekt):

```bash
git checkout main
```

### Přepnout na gh-pages (dokumentace):

```bash
git checkout gh-pages
```

## ⚠️ DŮLEŽITÉ PRAVIDLO

**NIKDY nemazat soubory projektu když jste na main branch!**

- Na `main` branch → **NEŠAHAT** na soubory projektu
- Na `gh-pages` branch → **MŮŽETE** mazat (obsahuje jen dokumentaci)

## ✅ Jak to správně nastavit

### Krok 1: Obnovit main branch s projektem

```bash
cd /Users/alfred/Documents/GitHub/chess_esp32_c6_devkit

# Přepnout na main (nebo vytvořit, pokud neexistuje)
git checkout main 2>/dev/null || git checkout -b main

# Zkopírovat projekt (kromě .git)
cp -r "/Users/alfred/Documents/my_local_projects/free_chess_v1 copy_working_10.1.2026"/* .
rm -rf .git  # Odstranit .git z projektu (aby se nepřepsal git repo)

# Commit a push
git add .
git commit -m "Restore project files"
git push -u origin main
```

### Krok 2: Nastavit gh-pages branch s dokumentací

```bash
# Přepnout na gh-pages
git checkout gh-pages

# gh-pages už by měl obsahovat dokumentaci (pokud ne, použijte deploy_to_gh_pages.sh)
```

## 🎯 Kontrolní seznam

Po nastavení by mělo být:

- [ ] `main` branch obsahuje projekt (components/, main/, atd.)
- [ ] `gh-pages` branch obsahuje jen dokumentaci (index.html, *.html, atd.)
- [ ] GitHub Pages je nastaveno na `gh-pages` branch, folder `/ (root)`

---

**Datum:** 2025-01-16  
**Verze:** 2.4.0
