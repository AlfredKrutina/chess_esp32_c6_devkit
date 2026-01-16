# Obnovení GitHub Repozitáře - Instrukce

## ❌ Co se stalo

Skript `deploy_to_gh_pages.sh` omylem smazal soubory projektu v git repo, když pracoval s gh-pages branch.

**Problém v kódu (opraveno):** Skript mazal soubory bez ověření, že jsme na gh-pages branch.

## ✅ Oprava

Skript byl opraven - nyní:
- ✅ **Kontroluje**, že jsme opravdu na gh-pages branch před mazáním
- ✅ **Zastaví se**, pokud není na gh-pages branch
- ✅ **Zobrazí varování** před mazáním souborů

## 🔧 Jak obnovit repo

### Možnost 1: Zkopírovat projekt z lokální složky (DOPORUČENO)

Pokud máte projekt v `/Users/alfred/Documents/my_local_projects/free_chess_v1 copy_working_10.1.2026`:

```bash
# 1. Přejít do git repo
cd /Users/alfred/Documents/GitHub/chess_esp32_c6_devkit

# 2. Přepnout na main branch (pokud existuje)
git checkout main 2>/dev/null || git checkout -b main

# 3. Zkopírovat všechny soubory projektu (kromě .git)
cp -r "/Users/alfred/Documents/my_local_projects/free_chess_v1 copy_working_10.1.2026"/* .
cp -r "/Users/alfred/Documents/my_local_projects/free_chess_v1 copy_working_10.1.2026"/.* . 2>/dev/null || true

# 4. Odstranit .git, aby se nepřepsal
rm -rf .git

# 5. Commit a push
git add .
git commit -m "Restore project files"
git push origin main
```

### Možnost 2: Klonovat z GitHubu (pokud je projekt tam)

```bash
# 1. Zálohovat současný repo
mv /Users/alfred/Documents/GitHub/chess_esp32_c6_devkit /Users/alfred/Documents/GitHub/chess_esp32_c6_devkit_backup

# 2. Klonovat z GitHubu
cd /Users/alfred/Documents/GitHub
git clone https://github.com/AlfredKrutina/chess_esp32_c6_devkit.git

# 3. Přejít do repo
cd chess_esp32_c6_devkit

# 4. Vytvořit/aktualizovat gh-pages branch
git checkout -b gh-pages 2>/dev/null || git checkout gh-pages
# ... kopírovat dokumentaci (viz deploy_to_gh_pages.sh, ale opravený)
```

### Možnost 3: Obnovit z git historie

```bash
cd /Users/alfred/Documents/GitHub/chess_esp32_c6_devkit

# Zjistit commit před smazáním
git log --oneline --all | grep -v "gh-pages\|pages repair" | head -5

# Obnovit soubory z konkrétního commitu
git checkout <commit-hash> -- .
```

## ✅ Ověření

Po obnovení zkontrolujte:

```bash
cd /Users/alfred/Documents/GitHub/chess_esp32_c6_devkit
ls -la | head -20  # Měly by být vidět soubory projektu (components/, main/, atd.)
git branch -a      # Měly by být vidět main i gh-pages
```

## 🎯 Odpověď na vaši otázku

**Ano, zkopírování projektu do repo by mělo fungovat**, ALE:

1. **Ujistěte se, že jste na main branch** (ne na gh-pages):
   ```bash
   cd /Users/alfred/Documents/GitHub/chess_esp32_c6_devkit
   git checkout main  # nebo git checkout -b main
   ```

2. **Zkopírujte soubory projektu:**
   ```bash
   cp -r "/Users/alfred/Documents/my_local_projects/free_chess_v1 copy_working_10.1.2026"/* .
   ```

3. **Odstraňte .git z projektu** (aby se nepřepsal git repo):
   ```bash
   rm -rf .git  # Zkopírovaný .git z projektu
   ```

4. **Commit a push:**
   ```bash
   git add .
   git commit -m "Restore project files"
   git push origin main
   ```

**⚠️ DŮLEŽITÉ:**
- Ujistěte se, že jste na **main branch** (ne gh-pages) před kopírováním!
- gh-pages branch má zůstat jen s dokumentací
- main branch má obsahovat projekt

---

**Datum:** 2025-01-16  
**Status:** Skript opraven, repo potřebuje obnovit
