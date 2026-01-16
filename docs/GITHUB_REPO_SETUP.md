# Nastavení GitHub Repozitáře - Kompletní řešení

## 🎯 Situace

Máte lokální repo `/Users/alfred/Documents/GitHub/chess_esp32_c6_devkit` s jen dokumentací (gh-pages branch), ale GitHub repo možná neexistuje nebo není správně nastavené.

## ✅ Řešení - Krok za krokem

### Krok 1: Zkontrolovat, jestli repo existuje na GitHubu

1. Otevřete: https://github.com/AlfredKrutina/chess_esp32_c6_devkit
2. Pokud vidíte 404, repo neexistuje → musíte ho vytvořit
3. Pokud repo existuje, přejděte na Krok 2

### Krok 2A: Pokud repo NEexistuje - Vytvořit ho

**Na GitHubu:**
1. Přejděte na https://github.com/new
2. Repository name: `chess_esp32_c6_devkit`
3. Nastavte na Private nebo Public (podle potřeby)
4. **NECHCEŠTE** inicializovat s README, .gitignore nebo licencí
5. Klikněte "Create repository"

**Pak lokálně pushněte gh-pages:**
```bash
cd /Users/alfred/Documents/GitHub/chess_esp32_c6_devkit
git remote add origin https://github.com/AlfredKrutina/chess_esp32_c6_devkit.git
git push -u origin gh-pages
```

### Krok 2B: Pokud repo EXISTUJE - Nastavit správně

**Možnost 1: Chcete jen dokumentaci na GitHub Pages (DOPORUČENO)**

Pokud hlavní projekt není na GitHubu:
1. Ujistěte se, že gh-pages branch je pushnutý:
   ```bash
   cd /Users/alfred/Documents/GitHub/chess_esp32_c6_devkit
   git push -u origin gh-pages
   ```
2. V GitHub → Settings → Pages nastavte:
   - Branch: `gh-pages`
   - Folder: `/ (root)`
3. Hotovo! Dokumentace bude na GitHub Pages.

**Možnost 2: Chcete celý projekt na GitHubu**

1. **Zálohovat současný gh-pages repo:**
   ```bash
   cp -r /Users/alfred/Documents/GitHub/chess_esp32_c6_devkit /Users/alfred/Documents/GitHub/chess_esp32_c6_devkit_backup
   ```

2. **Smaž nebo přejmenuj současný lokální repo:**
   ```bash
   cd /Users/alfred/Documents/GitHub
   mv chess_esp32_c6_devkit chess_esp32_c6_devkit_old
   ```

3. **Klonovat repo z GitHubu:**
   ```bash
   git clone https://github.com/AlfredKrutina/chess_esp32_c6_devkit.git
   cd chess_esp32_c6_devkit
   ```

4. **Zkopírovat projekt do main branch:**
   ```bash
   # Přepnout na main branch
   git checkout main
   # Zkopírovat celý projekt
   cp -r "/Users/alfred/Documents/my_local_projects/free_chess_v1 copy_working_10.1.2026"/* .
   # Commit a push
   git add .
   git commit -m "Add project files"
   git push origin main
   ```

5. **Vytvořit gh-pages branch s dokumentací:**
   ```bash
   git checkout -b gh-pages
   # Smaž vše kromě .git
   find . -mindepth 1 -maxdepth 1 ! -name '.git' -exec rm -rf {} +
   # Zkopíruj dokumentaci
   cp -r "/Users/alfred/Documents/my_local_projects/free_chess_v1 copy_working_10.1.2026/docs/doxygen/html"/* .
   touch .nojekyll
   git add .
   git commit -m "Add documentation"
   git push -u origin gh-pages
   ```

## 📋 Kontrolní seznam

- [ ] GitHub repo `chess_esp32_c6_devkit` existuje
- [ ] gh-pages branch je pushnutý na GitHub
- [ ] `index.html` je v root gh-pages branch
- [ ] `.nojekyll` je v root gh-pages branch
- [ ] GitHub Pages je nastaveno na gh-pages branch, folder `/ (root)`

## 🎯 Pro vaši situaci (jen dokumentace)

**Nejjednodušší řešení:**

1. **Zkontrolovat, jestli repo existuje na GitHubu**
2. **Pokud ne, vytvořit ho** (bez README/licence)
3. **Pushnout gh-pages branch:**
   ```bash
   cd /Users/alfred/Documents/GitHub/chess_esp32_c6_devkit
   git remote add origin https://github.com/AlfredKrutina/chess_esp32_c6_devkit.git
   git push -u origin gh-pages
   ```
4. **Nastavit GitHub Pages** (Settings → Pages → gh-pages branch → / (root))
5. **Počkat 1-2 minuty na nasazení**

---

**Verze:** 2.4.0  
**Datum:** 2025-01-16
