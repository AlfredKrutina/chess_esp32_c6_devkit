# CzechMate — what I need from Blender for marketing / Pages

This brief is for me or anyone helping with videos on [`downloads.html`](../../gh-pages-ready/downloads.html). It complements static WebP exports noted in that page header.

---

## Shared technical requirements

| Parameter | What I want | Why |
|----------|-----------------|------|
| **Format for self-hosted Pages** | **WebM** (VP9 or AV1 if the encoder makes sense), secondarily **H.264 MP4** in one file | Pages usually serves those MIME types; WebM is often smaller than MP4 at similar quality. |
| **File size** | ideally **under 8–12 MB** per short clip; **max ~15 MB** | large binaries slow clone and load; GitHub hard stop is **100 MB**. |
| **Resolution** | **1920×1080** or **1600×900** (16∶9) for hero; section clips **1280×720** OK | enough for web hero; don’t put 4K in git. |
| **FPS** | **24** or **30** | consistent in the project; 24 feels more “cinematic”. |
| **Length** | per video — typically **6–20 s** | short loop + fade for `<video loop muted playsinline>`. |
| **Audio** | **no audio track** (or music added later) | autoplay in browser wants `muted`; silent video is simpler. |
| **Color space** | **sRGB**, standard web gamma | matches WebP screenshots. |
| **Loop** | first and last frame **match visually**, optional **crossfade** in edit | HTML `loop` without a jump. |
| **Alternative outside git** | **YouTube / Vimeo** embed | when the scene is long or bitrate high — no need to store binary in repo. |

### File naming (proposal)

Target folder after export: [`gh-pages-ready/landing/assets/`](../../gh-pages-ready/landing/assets/).

| File | Content |
|--------|--------|
| `hero-loop.webm` | main hero loop |
| `hero-loop.mp4` | Safari / older engine fallback (optional second `<source>`) |
| `board-detail-loop.webm` | board / LED detail |
| `app-ui-loop.webm` | device mock with UI (optional) |

Static images stay separate: `board-render.webp`, `app-mock.webp` per [`downloads.html`](../../gh-pages-ready/downloads.html).

---

## Video 1 — Hero: “CzechMate in space”

**Purpose:** first impression on [`downloads.html`](../../gh-pages-ready/downloads.html) — replace text hero / background behind the title.

| Item | Specification |
|---------|-------------|
| **Working title** | `hero-loop` |
| **Duration** | **10–16 s** loop |
| **Aspect** | **16∶9** (1920×1080 or 1600×900) |
| **Content** | wide shot of **physical chessboard** (CAD / block model): squares, frame, hint of **button row**; optional small **ESP module** in background (not dominant). |
| **Lighting** | night / studio — **RGB accent** on brand (cyan–blue, soft bloom on LEDs); **low-key**, readable board silhouette. |
| **Camera** | slow **orbit** (15–30°) **or** gentle **dolly in**; no aggressive handheld. |
| **LED** | short **sequence**: edge squares → **move highlight** (e.g. e2→e4 as two glowing squares) → back to idle; loopable. |
| **Post** | light **glare / bloom** on LEDs only; **grain** very subtle. |
| **Export** | WebM VP9, ideally **2-pass**, target **≤ 10 MB**; optional H.264 for Safari. |

**Storyboard (rough):**

1. **0–3 s:** wide shot, board in shadow, first LEDs “wake up”.
2. **3–8 s:** camera moves / rotates; move highlight.
3. **8–end:** rest position — **matches** frame 0 for loop (or short fade).

---

## Video 2 — Hardware detail: “LED square and depth”

**Purpose:** hardware section — loop beside text or under static WebP.

| Item | Specification |
|---------|-------------|
| **Working title** | `board-detail-loop` |
| **Duration** | **8–12 s** |
| **Aspect** | **16∶9** or **4∶3** (must fit web CSS container — split often ~4∶3) |
| **Content** | **macro** on a square: WS2812 / diffuser, section cut or shallow DOF. |
| **Animation** | slow color **scan** along row/column or **pulse** “check” on one square. |
| **Camera** | static or micro **pan**. |
| **Export** | WebM, target **≤ 6–8 MB** (1280×720 OK). |

**Storyboard:** dark → row lights up → one cell changes → back to idle → matches loop start.

---

## Video 3 — App on device: “Mock glass UI”

**Purpose:** **App** section — complement to [`app-mock.webp`](../../gh-pages-ready/landing/assets/app-placeholder.svg).

| Item | Specification |
|---------|-------------|
| **Working title** | `app-ui-loop` |
| **Duration** | **12–18 s** |
| **Aspect** | **16∶9** (phone centered letterbox) or **9∶16** inside 16∶9 with dark background |
| **Content** | **phone** model; display simple shader / UV animated texture with a few “screens” (game, connection, settings). Need not be pixel-perfect — **similar palette** and Material-like typography enough. |
| **Animation** | slow **scroll** or **crossfade** between 2–3 states; subtle **frame gloss**. |
| **Export** | WebM, target **≤ 10 MB**. |

**Storyboard:** CzechMate splash → connection (BLE/Wi‑Fi abstract) → board in app → back to idle → loop.

---

## Video 4 (optional) — “Board ↔ phone link”

**Purpose:** social teaser / second page.

| Item | Specification |
|---------|-------------|
| **Working title** | `sync-concept` |
| **Duration** | **6–10 s** |
| **Content** | split frame or graphic “line” between board and phone — data flow symbolism. |
| **Style** | more abstract than V1–3. |
| **Export** | WebM **≤ 5 MB** or **YouTube** only. |

---

## Web distribution

### Option A — file in repo (short loops)

1. WebM (± MP4) into [`gh-pages-ready/landing/assets/`](../../gh-pages-ready/landing/assets/).
2. In [`downloads.html`](../../gh-pages-ready/downloads.html) hero e.g.:

```html
<video class="hero__video" data-hero-video autoplay muted loop playsinline>
  <source src="landing/assets/hero-loop.webm" type="video/webm">
  <source src="landing/assets/hero-loop.mp4" type="video/mp4">
</video>
```

3. [`landing.js`](../../gh-pages-ready/landing/landing.js) already handles `prefers-reduced-motion` for `data-hero-video`.

### Option B — YouTube / Vimeo

- video as **Unlisted** / **Not listed**
- iframe per comment in `downloads.html`
- no large binary in git

---

## Before committing to git

- [ ] length and file size in reasonable range  
- [ ] loop without jarring jump  
- [ ] no audio track (or consciously muted in `<video>`)  
- [ ] filenames match [`downloads.html`](../../gh-pages-ready/downloads.html)  
- [ ] main subject readable on large monitor and mobile  

---

## Related

- [`gh-pages-ready/downloads.html`](../../gh-pages-ready/downloads.html)  
- [`gh-pages-ready/README.md`](../../gh-pages-ready/README.md)  
- [`docs/README.md`](../README.md)  

Brief version: **1.0** — updated from real exports (bitrate, final names).
