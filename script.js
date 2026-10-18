// Web port of the SFML "Blue Heart" particle animation.
// A heart made of drifting "love you" particles, with a glowing
// center title and a heart-ring icon, built from the same math
// as the original C++ (heartXY / toScreen / glow layering).

(() => {
  const WORDS = ['love you', 'Love You', 'LOVE YOU'];
  const CENTER_TEXT = 'Love you ';
  const COLORS = [
    'rgb(255,181,193)', // baby pink
    'rgb(255,192,203)', // pink
    'rgb(250,160,176)', // deeper baby pink
    'rgb(255,210,220)', // pale baby pink
    'rgb(245,169,191)', // rose pink
  ];
  const PI = Math.PI;
  const FPS = 60;

  let WIDTH = window.innerWidth;
  let HEIGHT = window.innerHeight;
  let SCALE = HEIGHT / 38;

  function randUniform(lo, hi) { return lo + Math.random() * (hi - lo); }
  function randInt(lo, hi) { return Math.floor(randUniform(lo, hi + 1)); }
  function randChoice(arr) { return arr[randInt(0, arr.length - 1)]; }

  function heartXY(t) {
    const x = 16 * Math.pow(Math.sin(t), 3);
    const y = 13 * Math.cos(t) - 5 * Math.cos(2 * t) - 2 * Math.cos(3 * t) - Math.cos(4 * t);
    return [x, -y];
  }

  function toScreen(x, y) {
    return [x * SCALE + WIDTH / 2, y * SCALE + HEIGHT / 2];
  }

  function makeParticle(x, y, order, kind) {
    return {
      x, y, order, kind,
      word: randChoice(WORDS),
      color: randChoice(COLORS),
      alpha: 0,
      flicker: randUniform(0, 2 * PI),
      delay: 0,
      sizeMult: randUniform(0.85, 1.15),
      canvas: null,
    };
  }

  function buildOutlineParticles(n, minGap) {
    const particles = [];
    const placed = [];
    for (let i = 0; i < n; i++) {
      const t = (i / n) * 2 * PI;
      const [bx, by] = heartXY(t);
      const [sx, sy] = toScreen(bx, by);
      const tooClose = placed.some(p => Math.hypot(sx - p[0], sy - p[1]) < minGap);
      if (tooClose) continue;
      placed.push([sx, sy]);
      particles.push(makeParticle(sx, sy, i, 'outline'));
    }
    return particles;
  }

  function buildFillParticles(n, minGap) {
    const particles = [];
    const placed = [];
    let attempts = 0;
    const maxAttempts = n * 80;
    while (particles.length < n && attempts < maxAttempts) {
      attempts++;
      const t = randUniform(0, 2 * PI);
      const r = randUniform(0, 0.86);
      const [bx, by] = heartXY(t);
      const px = bx * r, py = by * r;
      const [sx, sy] = toScreen(px, py);
      const tooClose = placed.some(p => Math.hypot(sx - p[0], sy - p[1]) < minGap);
      if (tooClose) continue;
      placed.push([sx, sy]);
      particles.push(makeParticle(sx, sy, randInt(0, 320), 'fill'));
    }
    return particles;
  }

  function renderWordCanvas(word, fontPx, color) {
    const font = `bold ${fontPx}px Arial, Helvetica, sans-serif`;
    const probe = document.createElement('canvas').getContext('2d');
    probe.font = font;
    const m = probe.measureText(word);
    const ascent = m.actualBoundingBoxAscent || fontPx * 0.8;
    const descent = m.actualBoundingBoxDescent || fontPx * 0.25;
    const w = Math.max(1, Math.ceil(m.width + 4));
    const h = Math.max(1, Math.ceil(ascent + descent + 4));

    const canvas = document.createElement('canvas');
    canvas.width = w;
    canvas.height = h;
    const ctx = canvas.getContext('2d');
    ctx.font = font;
    ctx.textBaseline = 'alphabetic';
    ctx.fillStyle = color;
    ctx.fillText(word, 2, 2 + ascent);
    return canvas;
  }

  function buildCenterCanvas(text, charSize, fillColor, outlineColor, outlineThickness, heartPixelHeight) {
    const font = `bold ${charSize}px Georgia, 'Times New Roman', serif`;
    const probe = document.createElement('canvas').getContext('2d');
    probe.font = font;
    const m = probe.measureText(text);
    const ascent = m.actualBoundingBoxAscent || charSize * 0.8;
    const descent = m.actualBoundingBoxDescent || charSize * 0.25;
    const textW = Math.ceil(m.width + outlineThickness * 2 + 4);
    const textH = Math.ceil(ascent + descent + outlineThickness * 2 + 4);

    const N = 200;
    const curvePts = new Array(N);
    let minX = Infinity, maxX = -Infinity, minY = Infinity, maxY = -Infinity;
    for (let i = 0; i < N; i++) {
      const t = (i / N) * 2 * PI;
      const [hx, hy] = heartXY(t);
      curvePts[i] = [hx, hy];
      minX = Math.min(minX, hx); maxX = Math.max(maxX, hx);
      minY = Math.min(minY, hy); maxY = Math.max(maxY, hy);
    }
    const naturalH = maxY - minY;
    const naturalW = maxX - minX;
    const scale = heartPixelHeight / naturalH;
    const heartW = naturalW * scale;
    const curveMidX = (minX + maxX) / 2;
    const curveMidY = (minY + maxY) / 2;

    const pad = outlineThickness + 4;
    const gap = heartPixelHeight * 0.26;
    const totalW = Math.ceil(pad * 2 + textW + gap + heartW);
    const totalH = Math.ceil(Math.max(textH, heartPixelHeight) + pad * 2);

    const canvas = document.createElement('canvas');
    canvas.width = Math.max(1, totalW);
    canvas.height = Math.max(1, totalH);
    const ctx = canvas.getContext('2d');
    ctx.font = font;
    ctx.textBaseline = 'alphabetic';
    ctx.lineJoin = 'round';

    const textY = pad + (totalH - pad * 2 - textH) / 2 + ascent + outlineThickness;
    const textX = pad + outlineThickness;
    ctx.strokeStyle = outlineColor;
    ctx.lineWidth = outlineThickness * 2;
    ctx.strokeText(text, textX, textY);
    ctx.fillStyle = fillColor;
    ctx.fillText(text, textX, textY);

    const heartCenterX = pad + textW + gap + heartW / 2;
    const heartCenterY = totalH / 2;
    const toPixel = ([x, y]) => [
      (x - curveMidX) * scale + heartCenterX,
      (y - curveMidY) * scale + heartCenterY,
    ];

    const tracePath = () => {
      ctx.beginPath();
      curvePts.forEach((p, i) => {
        const [px, py] = toPixel(p);
        if (i === 0) ctx.moveTo(px, py); else ctx.lineTo(px, py);
      });
      ctx.closePath();
    };

    const strokeWidthPx = Math.max(3, heartPixelHeight * 0.12);
    ctx.lineCap = 'round';
    tracePath();
    ctx.lineWidth = strokeWidthPx + outlineThickness * 2;
    ctx.strokeStyle = outlineColor;
    ctx.stroke();
    tracePath();
    ctx.lineWidth = strokeWidthPx;
    ctx.strokeStyle = fillColor;
    ctx.stroke();

    return canvas;
  }

  function drawGlowText(glowCtx, textCtx, src, x, y, alpha) {
    if (alpha <= 0) return;
    const w = src.width, h = src.height;

    if (alpha > 10) {
      glowCtx.save();
      glowCtx.globalAlpha = Math.max(0, Math.floor(alpha / 7)) / 255;
      glowCtx.translate(x, y);
      glowCtx.scale(2.4, 2.4);
      glowCtx.drawImage(src, -w / 2, -h / 2);
      glowCtx.restore();

      glowCtx.save();
      glowCtx.globalAlpha = Math.max(0, Math.floor(alpha / 3)) / 255;
      glowCtx.translate(x, y);
      glowCtx.scale(1.6, 1.6);
      glowCtx.drawImage(src, -w / 2, -h / 2);
      glowCtx.restore();
    }

    textCtx.save();
    textCtx.globalAlpha = alpha / 255;
    textCtx.translate(x, y);
    textCtx.drawImage(src, -w / 2, -h / 2);
    textCtx.restore();
  }

  // --- scene state ---
  let canvas, ctx, glowLayer, glowCtx, textLayer, textCtx, dpr;
  let particles = [], centerCanvas = null, fillStartFrame = 0;
  let frame = 0;
  let rafId = null;
  let started = false;

  function setupCanvases() {
    dpr = window.devicePixelRatio || 1;
    WIDTH = window.innerWidth;
    HEIGHT = window.innerHeight;
    SCALE = HEIGHT / 38;

    canvas.width = WIDTH * dpr;
    canvas.height = HEIGHT * dpr;
    canvas.style.width = WIDTH + 'px';
    canvas.style.height = HEIGHT + 'px';
    ctx = canvas.getContext('2d');
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);

    glowLayer = document.createElement('canvas');
    glowLayer.width = WIDTH; glowLayer.height = HEIGHT;
    glowCtx = glowLayer.getContext('2d');

    textLayer = document.createElement('canvas');
    textLayer.width = WIDTH; textLayer.height = HEIGHT;
    textCtx = textLayer.getContext('2d');
  }

  function buildScene() {
    const uiScale = SCALE / 20;

    const outline = buildOutlineParticles(160, 30 * uiScale);
    const fill = buildFillParticles(130, 46 * uiScale);

    let outlineSpan = 0;
    outline.forEach(p => { outlineSpan = Math.max(outlineSpan, p.order); });
    const framesPerStep = 1.6;
    fillStartFrame = Math.floor(outlineSpan * framesPerStep) + 30;

    fill.forEach(p => { p.delay = fillStartFrame + p.order; });
    outline.forEach(p => { p.delay = Math.floor(p.order * framesPerStep); });

    particles = outline.concat(fill);

    particles.forEach(p => {
      const baseSize = p.kind === 'outline' ? 20 : 17;
      const fontPx = Math.round(baseSize * uiScale * p.sizeMult);
      p.canvas = renderWordCanvas(p.word, fontPx, p.color);
    });

    const centerCharSize = Math.round(54 * uiScale);
    centerCanvas = buildCenterCanvas(
      CENTER_TEXT, centerCharSize,
      'rgb(255,245,248)', 'rgb(90,15,40)',
      3 * uiScale, 38 * uiScale
    );

    frame = 0;
  }

  const STEP_MS = 1000 / FPS;
  let lastTimestamp = null;
  let accumulator = 0;

  function tick(timestamp) {
    rafId = requestAnimationFrame(tick);

    if (lastTimestamp === null) lastTimestamp = timestamp;
    let delta = timestamp - lastTimestamp;
    lastTimestamp = timestamp;
    if (delta > 250) delta = 250; // clamp big gaps (e.g. tab was backgrounded)
    accumulator += delta;

    let stepped = false;
    while (accumulator >= STEP_MS) {
      accumulator -= STEP_MS;
      frame++;
      stepped = true;
    }
    if (stepped) render();
  }

  function render() {
    glowCtx.clearRect(0, 0, WIDTH, HEIGHT);
    textCtx.clearRect(0, 0, WIDTH, HEIGHT);

    for (const p of particles) {
      if (frame > p.delay && p.alpha < 255) {
        p.alpha = Math.min(255, p.alpha + 14 + randInt(0, 4));
      }
      let flick;
      if (p.alpha >= 255) {
        flick = 0.75 + 0.25 * Math.sin(frame * 0.04 + p.flicker);
      } else {
        flick = 1.0;
      }
      const alpha = Math.floor(p.alpha * flick);
      if (alpha <= 0) continue;
      drawGlowText(glowCtx, textCtx, p.canvas, p.x, p.y, alpha);
    }

    ctx.clearRect(0, 0, WIDTH, HEIGHT);
    ctx.fillStyle = '#000';
    ctx.fillRect(0, 0, WIDTH, HEIGHT);
    ctx.drawImage(glowLayer, 0, 0);
    ctx.drawImage(textLayer, 0, 0);

    const centerStart = fillStartFrame + 200;
    if (frame > centerStart) {
      const progress = Math.min(1, (frame - centerStart) / 60);
      const centerAlpha = 255 * (1 - Math.exp(-progress * 8));
      const pulse = 1 + 0.025 * Math.sin(frame * 0.05);
      const cw = centerCanvas.width, ch = centerCanvas.height;

      if (centerAlpha > 10) {
        ctx.save();
        ctx.globalAlpha = Math.max(0, centerAlpha / 5) / 255;
        ctx.translate(WIDTH / 2, HEIGHT / 2);
        ctx.scale(pulse * 1.4, pulse * 1.4);
        ctx.drawImage(centerCanvas, -cw / 2, -ch / 2);
        ctx.restore();
      }

      ctx.save();
      ctx.globalAlpha = Math.min(255, Math.max(0, centerAlpha)) / 255;
      ctx.translate(WIDTH / 2, HEIGHT / 2);
      ctx.scale(pulse, pulse);
      ctx.drawImage(centerCanvas, -cw / 2, -ch / 2);
      ctx.restore();
    }
  }

  function start() {
    if (started) return;
    started = true;
    setupCanvases();
    buildScene();
    if (rafId) cancelAnimationFrame(rafId);
    lastTimestamp = null;
    accumulator = 0;
    rafId = requestAnimationFrame(tick);

    const bgm = document.getElementById('bgm');
    bgm.volume = 1.0;
    bgm.play().catch(() => {
      // Autoplay without a prior gesture can be blocked; ignored, same as
      // the original app continuing silently when audio fails to load.
    });
  }

  function onResize() {
    if (!started) return;
    setupCanvases();
    buildScene();
  }

  window.addEventListener('DOMContentLoaded', () => {
    canvas = document.getElementById('scene');
    ctx = canvas.getContext('2d');
    setupCanvases();

    const overlay = document.getElementById('startOverlay');
    const beginExperience = () => {
      overlay.classList.add('hidden');
      start();
      setTimeout(() => { overlay.style.display = 'none'; }, 650);
    };
    overlay.addEventListener('click', beginExperience, { once: true });
    window.addEventListener('keydown', (e) => {
      if (!started && (e.key === 'Enter' || e.key === ' ')) beginExperience();
    });

    let resizeTimer = null;
    window.addEventListener('resize', () => {
      clearTimeout(resizeTimer);
      resizeTimer = setTimeout(onResize, 200);
    });
  });
})();
