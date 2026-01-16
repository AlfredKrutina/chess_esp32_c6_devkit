# Oprava GitHub Repozitáře - Řešení

## 🔍 Problém

Lokální repozitář `/Users/alfred/Documents/GitHub/chess_esp32_c6_devkit` obsahuje **jen dokumentaci** (gh-pages branch), ale ne celý projekt. 

**Správná struktura by měla být:**
- Repo má více branchů:
  - `main` branch - obsahuje celý projekt (kód, komponenty, atd.)
  - `gh-pages` branch - obsahuje jen dokumentaci (HTML soubory)

## ✅ Řešení

### Možnost 1: Klonovat celý projekt z GitHubu (DOPORUČENO)

1. **Zálohovat současný gh-pages obsah:**
   ```bash
   cp -r /Users/alfred/Documents/GitHub/chess_esp32_c6_devkit /Users/alfred/Documents/GitHub/chess_esp32_c6_devkit_backup
   ```

2. **Smaž nebo přejmenuj současný lokální repo:**
   ```bash
   cd /Users/alfred/Documents/GitHub
   mv chess_esp32_c6_devkit chess_esp32_c6_devkit_old
   ```

3. **Klonovat celý projekt z GitHubu:**
   ```bash
   git clone https://github.com/AlfredKrutina/chess_esp32_c6_devkit.git
   cd chess_esp32_c6_devkit
   ```

4. **Vytvořit gh-pages branch s dokumentací:**
   ```bash
   git checkout -b gh-pages
   # Smaž vše kromě .git
   find . -mindepth 1 -maxdepth 1 ! -name '.git' -exec rm -rf {} +
   # Zkopíruj dokumentaci z projektu
   cp -r "/Users/alfred/Documents/my_local_projects/free_chess_v1 copy_working_10.1.2026/docs/doxygen/html"/* .
   touch .nojekyll
   git add .
   git commit -m "Add documentation"
   git push -u origin gh-pages
   ```

### Možnost 2: Nechat současný repo jen pro gh-pages (jednodušší)

Pokud hlavní projekt není na GitHubu, můžete:
1. Nechat `/Users/alfred/Documents/GitHub/chess_esp32_c6_devkit` jen pro gh-pages branch
2. Hlavní projekt zůstane v `/Users/alfred/Documents/my_local_projects/free_chess_v1 copy_working_10.1.2026`

**To je v pořádku**, pokud:
- GitHub repo `chess_esp32_c6_devkit` má jen gh-pages branch (s dokumentací)
- Hlavní projekt není na GitHubu nebo je jinde

## 📋 Co zkontrolovat

1. **Na GitHubu** - otevřete: https://github.com/AlfredKrutina/chess_esp32_c6_devkit
   - Jaké branche jsou tam?
   - Je tam `main` branch s projektem, nebo jen `gh-pages` s dokumentací?

2. **Lokálně:**
   - Pokud máte jen dokumentaci v repo - to je OK pro gh-pages branch
   - Ale pak by GitHub Pages mělo fungovat, pokud je správně pushnuté

## 🎯 Doporučení

**Pokud chcete mít na GitHubu celý projekt:**
- Použijte Možnost 1 (klonovat celý projekt)

**Pokud stačí jen dokumentace na GitHub Pages:**
- Možnost 2 je v pořádku
- Jen se ujistěte, že gh-pages branch je správně pushnutý

---

**Verze:** 2.4.0  
**Datum:** 2025-01-16
