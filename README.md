# Blue Heart (Web)

A browser port of the "Blue Heart" C++/SFML particle animation: a heart made
of drifting "love you" text, glowing in and filling out to a pulsing center
title, synced with a song.

Real C++ compiled to **WebAssembly** with Emscripten. SFML has no official web
backend, so the rendering/audio were rewritten from SFML to
**SDL2 + SDL2_ttf + SDL2_mixer**, keeping the original heart-curve math,
particle placement/glow logic, and pulsing center title.

`index.html` is the entire site — one self-contained file with the compiled
WASM binary and all assets (fonts, song) embedded inline. No companion
`.js`/`.wasm`/`.data` files.

## Files

```
index.html        ← the entire built site (what GitHub Pages serves)
icon_64.png       ← favicon
.nojekyll         ← tells GitHub Pages not to run Jekyll on this repo
main_sdl.cpp      ← C++ source (SDL2 port)
shell.html        ← HTML/CSS template the build wraps around the loader script
build.sh          ← em++ build command
assets/
  Arimo.ttf       ← Arial-metric-compatible font (OFL)
  Gelasio.ttf     ← Georgia-metric-compatible font (OFL)
  love_you.mp3    ← background song
```

## Run locally

Serve over `http://` — some browsers restrict WebAssembly/audio on `file://`:

```sh
npx serve .
```

Then visit the printed localhost URL and tap the heart to start (browsers
require a user gesture before playing audio).

## Deploy to GitHub Pages

### Option A — serve the pre-built `index.html` directly (instant)

1. Push this folder to a GitHub repository.
2. Go to **Settings → Pages → Build and deployment → Source**.
3. Choose **Deploy from a branch → `main` / `/ (root)`** → **Save**.
4. Your site is live at `https://<you>.github.io/<repo>/` within a minute.

### Option B — auto-build on every push with GitHub Actions (recommended)

1. Go to **Settings → Pages → Source** and choose **GitHub Actions**.
2. The workflow at `.github/workflows/deploy.yml` will compile the C++ and
   deploy automatically on every push to `main`.

## Rebuild

After editing `main_sdl.cpp` or `shell.html`, rebuild with the Emscripten SDK:

```sh
source /path/to/emsdk/emsdk_env.sh
bash build.sh
```

This regenerates `index.html` from `main_sdl.cpp`, `shell.html`, and
`assets/` (all embedded inline). Commit and push the new `index.html`.
