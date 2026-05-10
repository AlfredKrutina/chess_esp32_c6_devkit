/// JSON servírovaný z větve **gh-pages** (URL `github.io`), nikoli přímo z `main`.
/// Zdroj v repu je `gh-pages-ready/app_update.json`; na web se dostane až po úspěšném
/// běhu [`.github/workflows/gh-pages.yml`](../../../../.github/workflows/gh-pages.yml) (push na `main` / `master` s relevantními cestami, nebo `workflow_dispatch`).
/// Po změně `version` v aplikaci srovnej manifest v `gh-pages-ready/` a pushni — dokud
/// workflow nenasadí nový build, klienti vidí starý manifest.
const kAppUpdateJsonUrl =
    'https://alfredkrutina.github.io/chess_esp32_c6_devkit/app_update.json';

/// Výchozí stránka ke stažení / poznámkám k verzi, pokud JSON nemá vlastní `release_page_url`.
const kDefaultAppReleasePageUrl =
    'https://alfredkrutina.github.io/chess_esp32_c6_devkit/downloads.html';
