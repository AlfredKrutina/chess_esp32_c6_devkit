# Jednoduché řešení - Vše v jednom branchi

## 🎯 Co je jednodušší?

**ANO!** Můžete mít vše v **jednom branchi** (`main`). Místo dvou branchů stačí:

### ✅ Možnost 1: Vše v main branch (DOPORUČENO pro vás)

**Struktura:**
```
main branch obsahuje:
├── components/              ← Projekt
├── main/                    ← Projekt
├── CMakeLists.txt           ← Projekt
├── README.md                ← Projekt
├── docs/                    ← Dokumentace
│   ├── doxygen/
│   │   └── html/           ← Doxygen HTML výstup (tady)
│   └── ...
└── ...                      ← Všechno z projektu
```

**Nastavení GitHub Pages:**
- Settings → Pages → Source: `main` branch
- Folder: `/docs` (ne `/ (root)`!)

**Jak nasadit dokumentaci:**

1. **Vygenerovat dokumentaci:**
   ```bash
   ./generate_docs.sh
   ```

2. **Zkopírovat do repo/docs:**
   ```bash
   cd /Users/alfred/Documents/GitHub/chess_esp32_c6_devkit
   cp -r "/Users/alfred/Documents/my_local_projects/free_chess_v1 copy_working_10.1.2026/docs/doxygen/html" docs/
   ```

3. **Commit a push:**
   ```bash
   git add docs/
   git commit -m "Update documentation"
   git push origin main
   ```

**Výhody:**
- ✅ Vše v jednom branchi (jednodušší)
- ✅ Nemusíte řešit dva branche
- ✅ Projek i dokumentace spolu

**Nevýhody:**
- ⚠️ Dokumentace je v `/docs` složce (ne v root)
- ⚠️ URL bude: `https://alfredkrutina.github.io/chess_esp32_c6_devkit/docs/html/index.html` (ne jen `/`)

### ⚠️ Možnost 2: gh-pages branch (současné řešení)

**Výhody:**
- ✅ Dokumentace je v root (čistější URL)
- ✅ Oddělené od projektu

**Nevýhody:**
- ❌ Dva branche (složitější)
- ❌ Může se poplest (jak se stalo)

## 🎯 Doporučení pro vás

**Pro vás by bylo jednodušší: Vše v main branch s `/docs` složkou**

Proč:
1. Nemusíte řešit dva branche
2. Všechno je na jednom místě
3. Jednodušší správa

## 📋 Jak to nastavit (krok za krokem)

### Krok 1: Zkopírovat projekt do main branch

```bash
cd /Users/alfred/Documents/GitHub/chess_esp32_c6_devkit
git checkout main

# Zkopírovat projekt
cp -r "/Users/alfred/Documents/my_local_projects/free_chess_v1 copy_working_10.1.2026"/* .
rm -rf .git  # Odstranit .git z projektu

git add .
git commit -m "Restore project"
git push origin main
```

### Krok 2: Zkopírovat dokumentaci do docs/ složky

```bash
# Vytvořit docs/html/ složku
mkdir -p docs/html

# Zkopírovat dokumentaci
cp -r "/Users/alfred/Documents/my_local_projects/free_chess_v1 copy_working_10.1.2026/docs/doxygen/html"/* docs/html/

git add docs/
git commit -m "Add documentation to docs/"
git push origin main
```

### Krok 3: Nastavit GitHub Pages

1. Settings → Pages
2. Source: `main` branch
3. **Folder: `/docs`** (ne `/ (root)`!)
4. Save

### Krok 4: Dokumentace bude dostupná na:

```
https://alfredkrutina.github.io/chess_esp32_c6_devkit/docs/html/index.html
```

(Nebo pokud chcete v `/docs` root, pak jen `/docs/index.html`)

## 🔄 Alternativa: Dokumentace přímo v /docs (ne /docs/html)

Pokud chcete dokumentaci přímo v `/docs` (ne `/docs/html/`):

```bash
# Zkopírovat přímo do docs/
cp -r "/Users/alfred/Documents/my_local_projects/free_chess_v1 copy_working_10.1.2026/docs/doxygen/html"/* docs/

# Pak GitHub Pages nastavíte:
# Folder: /docs
# A dokumentace bude na: https://alfredkrutina.github.io/chess_esp32_c6_devkit/docs/index.html
```

## ✅ Shrnutí

**Pro vás doporučuji:**
- ✅ Vše v **main branch**
- ✅ Dokumentace v **`docs/`** složce
- ✅ GitHub Pages: `main` branch, folder `/docs`

**Výsledek:**
- Jednoduché (jeden branch)
- Všechno na jednom místě
- Nemusíte řešit gh-pages branch

---

**Datum:** 2025-01-16  
**Verze:** 2.4.0
