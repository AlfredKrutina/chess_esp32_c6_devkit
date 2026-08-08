# CzechMate — GitHub Pages (`gh-pages-ready/`)

This folder holds static files for the public site: **`downloads.html`** (product page and app links), **`app_update.json`** (semver manifest for update checks in the Flutter client), **`app_update.html`** (JSON schema description), **`landing/`** (CSS, JavaScript, images, and videos), and **`.nojekyll`** so GitHub Pages does not run Jekyll over this output.

On push to **`main`** or **`master`**, if the commit changes any path listed in the workflow (see below), [`.github/workflows/gh-pages.yml`](../.github/workflows/gh-pages.yml) runs, assembles `_site` (Doxygen, Mermaid, this page, and optionally firmware `.bin` files), and writes the result to the **`gh-pages`** branch. Changes on branches other than `main`/`master` do not trigger deploy (except manual **workflow_dispatch** in Actions).

## GitHub Pages — one-time setup

In **Settings → Pages → Build and deployment**: source **Deploy from a branch**, branch **`gh-pages`**, folder **`/`** (root). The public URL looks like `https://<user>.github.io/<repo>/`.

### `downloads.html` returns 404

Check that Pages reads from branch **`gh-pages`**, not **`main`** + **`/docs`**. With the wrong source, the page never serves the deployed content from this workflow. In the HTML source of the landing page, `<meta name="generator" content="Jekyll">` means a different root is being served than the artifact from this repo.

## Local preview of `downloads.html`

YouTube in the hero requires HTTP(S). The **`file://`** protocol often disables embeds; in that case `landing.js` replaces the area with a static image from the YouTube CDN.

Relative paths `landing/assets/…` are valid relative to **document root = `gh-pages-ready`**. The HTTP server must use this folder as its document root; otherwise media returns **404** and `<video>` elements show only the poster.

- **Recommended:** `cd gh-pages-ready` → `serve.cmd` / `serve.ps1` / `./serve.sh` → open `http://127.0.0.1:8765/downloads.html` in the browser
- **Server from repo root:** open `http://127.0.0.1:<port>/gh-pages-ready/downloads.html`, or run **`serve-downloads.cmd`** from the root

**Windows (PowerShell / CMD, `gh-pages-ready` directory):**

```powershell
.\serve.cmd
```

Optional port: `.\serve.cmd 9000`. If `serve.ps1` is blocked by execution policy, use `serve.cmd` or run `python -m http.server 8765` directly in the same folder.

**Git Bash / WSL / Linux / macOS:**

```bash
cd gh-pages-ready && ./serve.sh
```

## Manual deploy (without waiting for Actions)

```bash
./scripts/docs/generate_docs.sh
./scripts/render_docs.sh
rm -rf /tmp/czm-pages && mkdir -p /tmp/czm-pages
cp -a docs/doxygen/html/. /tmp/czm-pages/
cp -f docs/diagrams/diagrams_mermaid.html /tmp/czm-pages/diagrams_mermaid.html
cp -f gh-pages-ready/downloads.html /tmp/czm-pages/downloads.html
cp -f gh-pages-ready/app_update.json /tmp/czm-pages/app_update.json
cp -f gh-pages-ready/app_update.html /tmp/czm-pages/app_update.html
mkdir -p /tmp/czm-pages/landing
cp -a gh-pages-ready/landing/. /tmp/czm-pages/landing/
cp -f gh-pages-ready/.nojekyll /tmp/czm-pages/.nojekyll
git checkout --orphan gh-pages
git rm -rf . 2>/dev/null || true
cp -r /tmp/czm-pages/. .
git add .
git commit -m "Pages"
git push -u origin gh-pages --force
git checkout main
```

## Folder contents (on branch `main`)

| File / folder | Purpose |
|---------------|---------|
| `.nojekyll` | Copied to the root of the deployed site. |
| `app_update.json` | `latest_version`, optionally `min_supported_version`, `release_page_url`. Client: `flutter_czechmate/lib/core/constants/app_update_defaults.dart`. After an app release, keep in sync with `flutter_czechmate/pubspec.yaml`. Changes reach users only after deploy to **`gh-pages`**. |
| `app_update.html` | Human-readable description of the JSON schema. |
| `downloads.html` | Product page, APK/DMG/Windows installer downloads, interest section. V1/V2 differences and forms are described on the page itself. Feature modals: `landing/landing.js` (`FEATURE_PAGES`). Download links: default target is the release page; `landing.js` adds direct `browser_download_url` from GitHub API `releases/latest` for `.apk`, `.dmg`, and `windows-setup.exe`. |
| `landing/` | `landing.css`, `landing.js`, `assets/` (SVG, WebP, MP4, OG image). |

## Media (MP4)

- Hero: embedded YouTube (`loading="lazy"`, `fetchpriority="low"`, `preconnect` to `youtube.com` and `i.ytimg.com`).
- Board section: `landing/assets/czm-v2-led-loop.mp4`.
- App section: `landing/assets/czm-v2-app-iphone.mp4` in a `split__media` + `split__video` frame (4:3 aspect ratio, `object-fit: cover`). Playback **without autoplay** — starts when the video is **sufficiently visible in the viewport** (`data-play-when-visible` + `initAppDemoVideoPlayWhenVisible` in `landing.js`, Intersection Observer); stays paused when `prefers-reduced-motion: reduce`.
- Both local MP4s: `preload="none"`, source via `data-src` and `data-lazy-local` + `IntersectionObserver` in `landing.js` (load before entering viewport; start of downloads for the two files is staggered by ~220 ms). Native video controls appear only after **clicking the video** (`split__video--controls-on-click` in `landing.js`).
- AI coach section: image `#ai-coach-app-preview` is generated from the same MP4 in `landing.js` (canvas → JPEG) at the time given by attribute **`data-ai-preview-at`** on `#app-phone-demo-video` (decimal fraction of duration, default **0.52** — end of file is often black or empty). Layout: `split__media--ai-preview` + `split__media--ai-preview__pan` (center portion of width, **full frame height**, no vertical crop).

General conversion (including audio):

```bash
ffmpeg -y -i input.mp4 -c:v libx264 -profile:v high -pix_fmt yuv420p -movflags +faststart -c:a aac -b:a 128k output.mp4
```

**Lighter MP4 for web** (recommended for loops on `downloads.html`: lower resolution, CRF 26–28, `faststart`; for silent clips add `-an`):

```bash
ffmpeg -y -i input.mp4 -an -vf "scale=min(1280\,iw):-2" -c:v libx264 -preset medium -crf 26 -profile:v high -pix_fmt yuv420p -movflags +faststart output-web.mp4
```

## “Interest” section and Google Forms

The page uses **links only** (no custom POST from JavaScript).

1. **Pre-order:** `https://docs.google.com/forms/d/18ns5uSUSzr5zcHsiZwD1HWfY15xBa-folmE-oH86BsY/viewform` (respondents: `/viewform`; in the Forms editor the same form is under `/edit`).
2. **Interest survey:** `https://docs.google.com/forms/d/e/1FAIpQLSck_q6sjN1nnUs9aV2CsY0MyPNo9puLcncW603iEJz6BMLjPw/viewform`

Responses stay in the Google account that owns the forms; deploying a new static page version does not change them.

**Placement in HTML:** in the Interest section, the survey link is in the left panel; the pre-order button opens a modal with a link to the pre-order form.

## Links after deploy

- `index.html` — technical documentation (from CI).
- `diagrams_mermaid.html` — diagrams.
- `downloads.html` — marketing and downloads.
- `app_update.json` — machine-readable app version check.
- `app_update.html` — manifest documentation.

YouTube (same ID as in hero): [youtu.be/_MS6OP3x6Z4](https://youtu.be/_MS6OP3x6Z4).

---

See the root [README.md](../README.md) for coordination. Diagrams: `./scripts/render_docs.sh` or `python3 scripts/docs/generate_mermaid_html.py` per the repo workflow.

A push to `main` or `master` that changes at least one path in [`.github/workflows/gh-pages.yml`](../.github/workflows/gh-pages.yml) (including the entire `gh-pages-ready/**` folder) redeploys content to the `gh-pages` branch. App binaries (APK, DMG, EXE) are on GitHub Releases; the download section on `downloads.html` links to them via the `releases/latest` API. Manual run: Actions → “Deploy GitHub Pages” → **Run workflow**.
