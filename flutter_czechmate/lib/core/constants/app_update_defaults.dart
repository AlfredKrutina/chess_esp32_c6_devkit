/// JSON served from the **gh-pages** branch (URL `github.io`), not directly from `main`.
/// The source in the repo is `gh-pages-ready/app_update.json`; it gets to the website only after success
/// runtime [`.github/workflows/gh-pages.yml`](../../../../.github/workflows/gh-pages.yml) (push to `main` / `master` with relevant paths, or `workflow_dispatch`).
/// After changing the `version` in the application, compare the manifest in `gh-pages-ready/` and push — until
/// workflow does not deploy the new build, clients see the old manifest.
const kAppUpdateJsonUrl =
    'https://alfredkrutina.github.io/chess_esp32_c6_devkit/app_update.json';

/// Default downloads / release notes page if JSON doesn't have its own `release_page_url`.
const kDefaultAppReleasePageUrl =
    'https://alfredkrutina.github.io/chess_esp32_c6_devkit/downloads.html';
