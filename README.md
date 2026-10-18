# Blue Heart (Web)

A browser port of the "Blue Heart" C++/SFML particle animation: a heart made
of drifting "love you" text, glowing in and filling out to a pulsing center
title, synced with a song.

This is a static HTML5 Canvas + JavaScript reimplementation (no build step,
no dependencies) so it can be hosted directly on GitHub Pages — the original
uses SFML, which needs a native Windows executable and can't run in a
browser.

## Run locally

Open `index.html` directly, or serve the folder so audio/canvas behave
exactly like they will on Pages:

```sh
npx serve .
# or
python -m http.server 8080
```

Then visit the printed localhost URL and click/tap the heart to start
(browsers block audio with sound until a user gesture).

## Deploy to GitHub Pages

1. Create a new GitHub repository (or use an existing one) and push this
   folder's contents to it:

   ```sh
   git init
   git add .
   git commit -m "Initial web port"
   git branch -M main
   git remote add origin https://github.com/<you>/<repo>.git
   git push -u origin main
   ```

2. On GitHub, go to **Settings → Pages**.
3. Under **Build and deployment → Source**, choose **Deploy from a branch**.
4. Pick the `main` branch and `/ (root)` folder, then **Save**.
5. GitHub publishes the site at `https://<you>.github.io/<repo>/` within a
   minute or two.

## Files

- `index.html` — page shell + start overlay
- `style.css` — fullscreen black canvas styling
- `script.js` — particle heart animation (ports `heartXY`, particle
  placement, glow layering, and the pulsing center title from `main.cpp`)
- `love_you.mp3` — the song that plays once you tap to start
- `icon_64.png` — favicon
