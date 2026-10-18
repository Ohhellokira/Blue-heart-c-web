# Blue Heart (Web)

A browser port of the "Blue Heart" C++/SFML particle animation: a heart made
of drifting "love you" text, glowing in and filling out to a pulsing center
title, synced with a song.

This is real C++ compiled to **WebAssembly** with Emscripten. SFML has no
official web backend, so the rendering/audio were rewritten from SFML to
**SDL2 + SDL2_ttf + SDL2_mixer**, which Emscripten does support, keeping the
original's heart-curve math, particle placement/glow logic, and pulsing
center title.

## Run locally

The build needs to be served over `http://` — opening `index.html` directly
as a `file://` URL blocks the `.wasm`/`.data` fetches (CORS):

```sh
npx serve .
```

Then visit the printed localhost URL, and tap the heart to start (browsers
block audio with sound until a user gesture).

## Deploy to GitHub Pages

1. Create a new GitHub repository (or use an existing one) and push this
   folder's contents to it:

   ```sh
   git init
   git add .
   git commit -m "Initial commit"
   git branch -M main
   git remote add origin https://github.com/<you>/<repo>.git
   git push -u origin main
   ```

2. On GitHub, go to **Settings → Pages**.
3. Under **Build and deployment → Source**, choose **Deploy from a branch**.
4. Pick the `main` branch and `/ (root)` folder, then **Save**.
5. GitHub publishes the site at `https://<you>.github.io/<repo>/` within a
   minute or two.

## Rebuilding

Source lives in `wasm/`. After editing `wasm/main_sdl.cpp`, rebuild with the
Emscripten SDK installed and sourced:

```sh
source /path/to/emsdk/emsdk_env.sh
cd wasm
bash build.sh
```

This regenerates `index.html`, `index.js`, `index.wasm`, and `index.data` at
the repo root (the actual published site) from `wasm/main_sdl.cpp`,
`wasm/shell.html` (the HTML template), and `wasm/assets/` (fonts + song,
baked into `index.data` at build time).

## Files

- `index.html`, `index.js`, `index.wasm`, `index.data` — the built site
  (what GitHub Pages actually serves)
- `icon_64.png` — favicon
- `wasm/main_sdl.cpp` — the C++ source (SDL2 port of `main.cpp`)
- `wasm/shell.html` — HTML/CSS template (start overlay, styling) the build
  wraps around the generated loader script
- `wasm/assets/` — fonts (Arimo/Gelasio, OFL-licensed Arial/Georgia
  substitutes — Microsoft's fonts can't be redistributed) and the song
- `wasm/build.sh` — the `em++` build command
