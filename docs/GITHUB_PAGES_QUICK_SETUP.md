# GitHub Pages - Rychlý návod na nastavení

## ⚠️ Důležité nastavení

Když aktivujete GitHub Pages, **musíte nastavit správně**:

### ✅ Správné nastavení:

1. **Settings → Pages**
2. **Build and deployment** → **Source**: `Deploy from a branch`
3. **Branch**:
   - **Branch**: `gh-pages` ← **DŮLEŽITÉ: ne `main`!**
   - **Folder**: `/ (root)` ← **DŮLEŽITÉ: ne `/docs`!**
4. Klikněte na **Save**

### ❌ Špatné nastavení (co NEDĚLAT):

- ❌ Branch: `main` + Folder: `/docs` ← **Toto je pro Jekyll, ne pro naši dokumentaci!**
- ❌ Branch: `main` + Folder: `/ (root)` ← **Dokumentace není v main branch!**

### 📋 Proč?

- Naše dokumentace je v **gh-pages branch** v **root složce** (ne v `/docs`)
- Složka `/docs` je pro Jekyll (statický generátor webů), ale my používáme **statickou HTML dokumentaci** z Doxygenu
- Dokumentace musí být v **root gh-pages branch**, protože tam ji kopíruje náš skript

## 🎯 Postup krok za krokem

### Pokud máte git repozitář:

1. Spusťte: `./update_gh_pages.sh`
2. Jděte na GitHub → Settings → Pages
3. Nastavte:
   - Source: `Deploy from a branch`
   - Branch: `gh-pages`
   - Folder: `/ (root)`
4. Save

### Pokud nemáte git repozitář:

1. Spusťte: `./prepare_gh_pages.sh`
2. Nahrajte obsah `gh-pages-ready/` do gh-pages branch na GitHubu
3. Jděte na GitHub → Settings → Pages
4. Nastavte:
   - Source: `Deploy from a branch`
   - Branch: `gh-pages`
   - Folder: `/ (root)`
5. Save

## 🔍 Jak zkontrolovat, že je to správně:

Po nastavení by mělo být:
- ✅ Branch: `gh-pages`
- ✅ Folder: `/ (root)`
- ✅ V gh-pages branch by měl být soubor `index.html` v root (ne v `/docs/`)

## 📸 Obrázek správného nastavení:

```
Build and deployment
└── Source: Deploy from a branch
    └── Branch:
        ├── Branch: gh-pages  ← TADY
        └── Folder: / (root)  ← TADY
```

---

**Verze:** 2.4.0  
**Datum:** 2025-01-10
