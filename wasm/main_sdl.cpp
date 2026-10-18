// WebAssembly port of ../../main.cpp.
// SFML has no official Emscripten backend, so this rewrites the same
// animation (heartXY math, particle placement, glow layering, pulsing
// center title) on SDL2 + SDL2_ttf + SDL2_mixer, which Emscripten does
// support directly. The blocking SFML event/render loop becomes a
// callback driven by emscripten_set_main_loop (the browser owns the
// loop; we can't block on it).

#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_mixer.h>
#include <emscripten.h>
#include <emscripten/html5.h>

#include <cmath>
#include <cstdint>
#include <random>
#include <string>
#include <vector>
#include <algorithm>
#include <memory>

static int WIDTH = 1280;
static int HEIGHT = 800;
static const SDL_Color BACKGROUND_COLOR{ 0, 0, 0, 255 };
static float SCALE = 20.0f;
static constexpr float PI = 3.14159265358979323846f;

static const std::vector<std::string> WORDS = { "love you", "Love You", "LOVE YOU" };
static const std::string CENTER_TEXT = "Love you ";
static const std::vector<SDL_Color> COLORS = {
    { 255, 181, 193, 255 }, // baby pink
    { 255, 192, 203, 255 }, // pink
    { 250, 160, 176, 255 }, // deeper baby pink
    { 255, 210, 220, 255 }, // pale baby pink
    { 245, 169, 191, 255 }, // rose pink
};

static std::mt19937 rng(std::random_device{}());

static float randUniform(float lo, float hi) {
    std::uniform_real_distribution<float> d(lo, hi);
    return d(rng);
}
static int randInt(int lo, int hi) {
    std::uniform_int_distribution<int> d(lo, hi);
    return d(rng);
}
template <typename T>
static const T& randChoice(const std::vector<T>& v) { return v[randInt(0, (int)v.size() - 1)]; }

enum class Kind { Outline, Fill };

struct Particle {
    float x = 0.f, y = 0.f;
    int order = 0;
    Kind kind = Kind::Outline;
    std::string word;
    SDL_Color color{};
    int alpha = 0;
    float flicker = 0.f;
    int delay = 0;
    float sizeMult = 1.f;
    SDL_Texture* tex = nullptr;
    int texW = 0, texH = 0;
};

static std::pair<float, float> heartXY(float t) {
    float x = 16.0f * std::pow(std::sin(t), 3.0f);
    float y = 13.0f * std::cos(t) - 5.0f * std::cos(2 * t) - 2.0f * std::cos(3 * t) - std::cos(4 * t);
    return { x, -y };
}

static std::pair<float, float> toScreen(float x, float y) {
    return { x * SCALE + WIDTH / 2.0f, y * SCALE + HEIGHT / 2.0f };
}

static Particle makeParticle(float x, float y, int order, Kind kind) {
    Particle p;
    p.x = x; p.y = y; p.order = order; p.kind = kind;
    p.word = randChoice(WORDS);
    p.color = randChoice(COLORS);
    p.flicker = randUniform(0.0f, 2.0f * PI);
    p.sizeMult = randUniform(0.85f, 1.15f);
    return p;
}

static std::vector<Particle> buildOutlineParticles(int n, float minGap) {
    std::vector<Particle> particles;
    std::vector<std::pair<float, float>> placed;
    for (int i = 0; i < n; ++i) {
        float t = (static_cast<float>(i) / n) * 2.0f * PI;
        auto [bx, by] = heartXY(t);
        auto [sx, sy] = toScreen(bx, by);
        bool tooClose = std::any_of(placed.begin(), placed.end(), [&](const auto& pt) {
            return std::hypot(sx - pt.first, sy - pt.second) < minGap;
            });
        if (tooClose) continue;
        placed.push_back({ sx, sy });
        particles.push_back(makeParticle(sx, sy, i, Kind::Outline));
    }
    return particles;
}

static std::vector<Particle> buildFillParticles(int n, float minGap) {
    std::vector<Particle> particles;
    std::vector<std::pair<float, float>> placed;
    int attempts = 0;
    int maxAttempts = n * 80;
    while (static_cast<int>(particles.size()) < n && attempts < maxAttempts) {
        attempts++;
        float t = randUniform(0.0f, 2.0f * PI);
        float r = randUniform(0.0f, 0.86f);
        auto [bx, by] = heartXY(t);
        float px = bx * r, py = by * r;
        auto [sx, sy] = toScreen(px, py);
        bool tooClose = std::any_of(placed.begin(), placed.end(), [&](const auto& pt) {
            return std::hypot(sx - pt.first, sy - pt.second) < minGap;
            });
        if (tooClose) continue;
        placed.push_back({ sx, sy });
        particles.push_back(makeParticle(sx, sy, randInt(0, 320), Kind::Fill));
    }
    return particles;
}

// --- globals: SDL state ---
static SDL_Window* g_window = nullptr;
static SDL_Renderer* g_renderer = nullptr;
static TTF_Font* g_fontSans = nullptr;   // word particles (Arimo, Arial-metric-compatible)
static TTF_Font* g_fontSerif = nullptr;  // center title (Gelasio, Georgia-metric-compatible)
static Mix_Music* g_music = nullptr;
static bool g_musicStarted = false;

static SDL_Texture* g_glowLayer = nullptr;
static SDL_Texture* g_textLayer = nullptr;

static std::vector<Particle> g_particles;
static std::vector<int> g_frameAlpha;
static SDL_Texture* g_centerTexture = nullptr;
static int g_centerW = 0, g_centerH = 0;
static int g_fillStartFrame = 0;
static long g_frame = 0;
static double g_lastTimeMs = -1.0;
static double g_accumulatorMs = 0.0;
static const double STEP_MS = 1000.0 / 60.0;

static void destroyParticleTextures() {
    for (auto& p : g_particles) {
        if (p.tex) { SDL_DestroyTexture(p.tex); p.tex = nullptr; }
    }
}

static SDL_Texture* renderWordTexture(TTF_Font* font, const std::string& word, int fontPx, SDL_Color color, int* outW, int* outH) {
    TTF_SetFontSize(font, fontPx);
    TTF_SetFontStyle(font, TTF_STYLE_BOLD);
    SDL_Surface* surf = TTF_RenderUTF8_Blended(font, word.c_str(), color);
    if (!surf) { *outW = *outH = 0; return nullptr; }
    SDL_Texture* tex = SDL_CreateTextureFromSurface(g_renderer, surf);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    *outW = surf->w; *outH = surf->h;
    SDL_FreeSurface(surf);
    return tex;
}

// Builds the "Love you " title + heart-ring icon into one render-target
// texture, mirroring renderCenterTextureWithHeart() in the SFML version:
// an outlined/filled text block to the left, a two-tone heart-shaped
// ring (dark outline ring + pink ring on top) to the right.
static SDL_Texture* buildCenterTexture(const std::string& text, int charSize,
    SDL_Color fillColor, SDL_Color outlineColor, float outlineThickness,
    float heartPixelHeight, int* outW, int* outH) {

    TTF_SetFontSize(g_fontSerif, charSize);
    TTF_SetFontStyle(g_fontSerif, TTF_STYLE_BOLD);

    TTF_SetFontOutline(g_fontSerif, 0);
    SDL_Surface* fillSurf = TTF_RenderUTF8_Blended(g_fontSerif, text.c_str(), fillColor);
    TTF_SetFontOutline(g_fontSerif, std::max(1, (int)std::round(outlineThickness)));
    SDL_Surface* outlineSurf = TTF_RenderUTF8_Blended(g_fontSerif, text.c_str(), outlineColor);
    TTF_SetFontOutline(g_fontSerif, 0);

    int textW = outlineSurf->w;
    int textH = outlineSurf->h;
    int ot = std::max(1, (int)std::round(outlineThickness));
    SDL_Rect fillDst{ ot, ot, fillSurf->w, fillSurf->h };
    SDL_BlitSurface(fillSurf, nullptr, outlineSurf, &fillDst);
    SDL_Texture* textTex = SDL_CreateTextureFromSurface(g_renderer, outlineSurf);
    SDL_SetTextureBlendMode(textTex, SDL_BLENDMODE_BLEND);
    SDL_FreeSurface(fillSurf);
    SDL_FreeSurface(outlineSurf);

    constexpr int N = 200;
    std::vector<SDL_FPoint> curvePts(N);
    std::vector<SDL_FPoint> normals(N);
    float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;
    for (int i = 0; i < N; ++i) {
        float t = (static_cast<float>(i) / N) * 2.0f * PI;
        auto [hx, hy] = heartXY(t);
        curvePts[i] = { hx, hy };
        minX = std::min(minX, hx); maxX = std::max(maxX, hx);
        minY = std::min(minY, hy); maxY = std::max(maxY, hy);
    }
    float naturalH = maxY - minY;
    float naturalW = maxX - minX;
    float scale = heartPixelHeight / naturalH;
    float heartW = naturalW * scale;
    float curveMidX = (minX + maxX) / 2.0f;
    float curveMidY = (minY + maxY) / 2.0f;

    for (int i = 0; i < N; ++i) {
        int prev = (i - 1 + N) % N;
        int next = (i + 1) % N;
        SDL_FPoint tangent{ curvePts[next].x - curvePts[prev].x, curvePts[next].y - curvePts[prev].y };
        SDL_FPoint normal{ -tangent.y, tangent.x };
        float len = std::sqrt(normal.x * normal.x + normal.y * normal.y);
        if (len > 1e-6f) { normal.x /= len; normal.y /= len; }
        SDL_FPoint toCenter{ curveMidX - curvePts[i].x, curveMidY - curvePts[i].y };
        if (normal.x * toCenter.x + normal.y * toCenter.y < 0.0f) { normal.x = -normal.x; normal.y = -normal.y; }
        normals[i] = normal;
    }

    const float pad = outlineThickness + 4.0f;
    const float gap = heartPixelHeight * 0.26f;
    int totalW = (int)std::ceil(pad * 2 + textW + gap + heartW);
    int totalH = (int)std::ceil(std::max((float)textH, heartPixelHeight) + pad * 2);
    totalW = std::max(1, totalW);
    totalH = std::max(1, totalH);

    float heartCenterX = pad + textW + gap + heartW / 2.0f;
    float heartCenterY = totalH / 2.0f;
    auto toPixel = [&](SDL_FPoint p) {
        return SDL_FPoint{ (p.x - curveMidX) * scale + heartCenterX, (p.y - curveMidY) * scale + heartCenterY };
        };

    auto buildRingVerts = [&](float halfWidthCurveUnits, SDL_Color col) {
        std::vector<SDL_Vertex> verts;
        verts.reserve(N * 6);
        for (int i = 0; i < N; ++i) {
            int nxt = (i + 1) % N;
            SDL_FPoint outerCur = toPixel({ curvePts[i].x - normals[i].x * halfWidthCurveUnits, curvePts[i].y - normals[i].y * halfWidthCurveUnits });
            SDL_FPoint innerCur = toPixel({ curvePts[i].x + normals[i].x * halfWidthCurveUnits, curvePts[i].y + normals[i].y * halfWidthCurveUnits });
            SDL_FPoint outerNxt = toPixel({ curvePts[nxt].x - normals[nxt].x * halfWidthCurveUnits, curvePts[nxt].y - normals[nxt].y * halfWidthCurveUnits });
            SDL_FPoint innerNxt = toPixel({ curvePts[nxt].x + normals[nxt].x * halfWidthCurveUnits, curvePts[nxt].y + normals[nxt].y * halfWidthCurveUnits });
            SDL_FPoint tex0{ 0, 0 };
            verts.push_back({ outerCur, col, tex0 });
            verts.push_back({ innerCur, col, tex0 });
            verts.push_back({ outerNxt, col, tex0 });
            verts.push_back({ innerCur, col, tex0 });
            verts.push_back({ innerNxt, col, tex0 });
            verts.push_back({ outerNxt, col, tex0 });
        }
        return verts;
        };

    float strokeWidthPx = std::max(3.0f, heartPixelHeight * 0.12f);
    float halfStrokeCurveUnits = (strokeWidthPx / scale) / 2.0f;
    float outlineCurveUnits = outlineThickness / scale;
    auto outerRing = buildRingVerts(halfStrokeCurveUnits + outlineCurveUnits, outlineColor);
    auto innerRing = buildRingVerts(halfStrokeCurveUnits, fillColor);

    SDL_Texture* target = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET, totalW, totalH);
    SDL_SetTextureBlendMode(target, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(g_renderer, target);
    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 0);
    SDL_RenderClear(g_renderer);
    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);

    SDL_Rect textDst{ (int)pad, (int)((totalH - textH) / 2.0f), textW, textH };
    SDL_RenderCopy(g_renderer, textTex, nullptr, &textDst);
    SDL_DestroyTexture(textTex);

    SDL_RenderGeometry(g_renderer, nullptr, outerRing.data(), (int)outerRing.size(), nullptr, 0);
    SDL_RenderGeometry(g_renderer, nullptr, innerRing.data(), (int)innerRing.size(), nullptr, 0);

    SDL_SetRenderTarget(g_renderer, nullptr);

    *outW = totalW; *outH = totalH;
    return target;
}

// Assumes the render target is already set by the caller (batched across
// all particles — see stepFrame) so this never touches SDL_SetRenderTarget
// itself: switching render targets forces the SDL2/WebGL batch to flush,
// and doing that per-particle (as drawGlowText used to) tanked framerate.
static void drawGlow(SDL_Texture* src, int texW, int texH, float x, float y, int alpha) {
    if (alpha <= 10 || !src) return;
    SDL_SetTextureAlphaMod(src, (Uint8)std::max(0, alpha / 7));
    SDL_FRect bigDst{ x - texW * 1.2f, y - texH * 1.2f, texW * 2.4f, texH * 2.4f };
    SDL_RenderCopyF(g_renderer, src, nullptr, &bigDst);

    SDL_SetTextureAlphaMod(src, (Uint8)std::max(0, alpha / 3));
    SDL_FRect smallDst{ x - texW * 0.8f, y - texH * 0.8f, texW * 1.6f, texH * 1.6f };
    SDL_RenderCopyF(g_renderer, src, nullptr, &smallDst);
}

static void drawText(SDL_Texture* src, int texW, int texH, float x, float y, int alpha) {
    if (alpha <= 0 || !src) return;
    SDL_SetTextureAlphaMod(src, (Uint8)alpha);
    SDL_FRect dst{ x - texW / 2.0f, y - texH / 2.0f, (float)texW, (float)texH };
    SDL_RenderCopyF(g_renderer, src, nullptr, &dst);
}

static void buildScene() {
    destroyParticleTextures();
    if (g_centerTexture) { SDL_DestroyTexture(g_centerTexture); g_centerTexture = nullptr; }

    SCALE = HEIGHT / 38.0f;
    float uiScale = SCALE / 20.0f;

    std::vector<Particle> outline = buildOutlineParticles(160, 30.0f * uiScale);
    std::vector<Particle> fill = buildFillParticles(130, 46.0f * uiScale);

    int outlineSpan = 0;
    for (auto& p : outline) outlineSpan = std::max(outlineSpan, p.order);
    const float framesPerStep = 1.6f;
    g_fillStartFrame = static_cast<int>(outlineSpan * framesPerStep) + 30;

    for (auto& p : fill) p.delay = g_fillStartFrame + p.order;
    for (auto& p : outline) p.delay = static_cast<int>(p.order * framesPerStep);

    g_particles.clear();
    g_particles.reserve(outline.size() + fill.size());
    for (auto& p : outline) g_particles.push_back(std::move(p));
    for (auto& p : fill) g_particles.push_back(std::move(p));

    for (auto& p : g_particles) {
        int baseSize = (p.kind == Kind::Outline) ? 20 : 17;
        int fontPx = std::max(1, (int)std::round(baseSize * uiScale * p.sizeMult));
        p.tex = renderWordTexture(g_fontSans, p.word, fontPx, p.color, &p.texW, &p.texH);
    }

    int centerCharSize = std::max(1, (int)std::round(54 * uiScale));
    g_centerTexture = buildCenterTexture(CENTER_TEXT, centerCharSize,
        SDL_Color{ 255, 245, 248, 255 }, SDL_Color{ 90, 15, 40, 255 },
        3.0f * uiScale, 38.0f * uiScale, &g_centerW, &g_centerH);

    if (g_glowLayer) SDL_DestroyTexture(g_glowLayer);
    if (g_textLayer) SDL_DestroyTexture(g_textLayer);
    g_glowLayer = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, WIDTH, HEIGHT);
    g_textLayer = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, WIDTH, HEIGHT);
    SDL_SetTextureBlendMode(g_glowLayer, SDL_BLENDMODE_BLEND);
    SDL_SetTextureBlendMode(g_textLayer, SDL_BLENDMODE_BLEND);

    g_frame = 0;
    g_lastTimeMs = -1.0;
    g_accumulatorMs = 0.0;
}

static void clearLayer(SDL_Texture* layer) {
    SDL_SetRenderTarget(g_renderer, layer);
    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 0);
    SDL_RenderClear(g_renderer);
    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
}

static void stepFrame() {
    g_frame++;

    g_frameAlpha.resize(g_particles.size());
    for (size_t i = 0; i < g_particles.size(); ++i) {
        Particle& p = g_particles[i];
        if (g_frame > p.delay && p.alpha < 255) {
            p.alpha = std::min(255, p.alpha + 14 + randInt(0, 4));
        }
        float flick;
        if (p.alpha >= 255) {
            flick = 0.75f + 0.25f * std::sin(g_frame * 0.04f + p.flicker);
        } else {
            flick = 1.0f;
        }
        g_frameAlpha[i] = static_cast<int>(p.alpha * flick);
    }

    clearLayer(g_glowLayer); // leaves render target set to g_glowLayer
    for (size_t i = 0; i < g_particles.size(); ++i) {
        Particle& p = g_particles[i];
        drawGlow(p.tex, p.texW, p.texH, p.x, p.y, g_frameAlpha[i]);
    }

    clearLayer(g_textLayer); // leaves render target set to g_textLayer
    for (size_t i = 0; i < g_particles.size(); ++i) {
        Particle& p = g_particles[i];
        drawText(p.tex, p.texW, p.texH, p.x, p.y, g_frameAlpha[i]);
    }
}

static void renderFrame() {
    SDL_SetRenderTarget(g_renderer, nullptr);
    SDL_SetRenderDrawColor(g_renderer, BACKGROUND_COLOR.r, BACKGROUND_COLOR.g, BACKGROUND_COLOR.b, 255);
    SDL_RenderClear(g_renderer);

    SDL_RenderCopy(g_renderer, g_glowLayer, nullptr, nullptr);
    SDL_RenderCopy(g_renderer, g_textLayer, nullptr, nullptr);

    int centerStart = g_fillStartFrame + 200;
    if (g_frame > centerStart) {
        float progress = std::min(1.0f, (g_frame - centerStart) / 60.0f);
        float centerAlphaF = 255.0f * (1.0f - std::exp(-progress * 8));
        int centerAlpha = (int)std::round(std::clamp(centerAlphaF, 0.0f, 255.0f));
        float pulse = 1.0f + 0.025f * std::sin(g_frame * 0.05f);

        if (centerAlpha > 10) {
            SDL_SetTextureAlphaMod(g_centerTexture, (Uint8)std::max(0, centerAlpha / 5));
            float gw = g_centerW * pulse * 1.4f, gh = g_centerH * pulse * 1.4f;
            SDL_FRect glowDst{ WIDTH / 2.0f - gw / 2.0f, HEIGHT / 2.0f - gh / 2.0f, gw, gh };
            SDL_RenderCopyF(g_renderer, g_centerTexture, nullptr, &glowDst);
        }

        SDL_SetTextureAlphaMod(g_centerTexture, (Uint8)centerAlpha);
        float cw = g_centerW * pulse, ch = g_centerH * pulse;
        SDL_FRect dst{ WIDTH / 2.0f - cw / 2.0f, HEIGHT / 2.0f - ch / 2.0f, cw, ch };
        SDL_RenderCopyF(g_renderer, g_centerTexture, nullptr, &dst);
    }

    SDL_RenderPresent(g_renderer);
}

static void mainLoop() {
    double now = emscripten_get_now();
    if (g_lastTimeMs < 0) g_lastTimeMs = now;
    double delta = now - g_lastTimeMs;
    g_lastTimeMs = now;
    if (delta > 250.0) delta = 250.0;
    g_accumulatorMs += delta;

    bool stepped = false;
    while (g_accumulatorMs >= STEP_MS) {
        g_accumulatorMs -= STEP_MS;
        stepFrame();
        stepped = true;
    }
    if (stepped) renderFrame();
}

static EM_BOOL onResize(int /*eventType*/, const EmscriptenUiEvent* /*e*/, void* /*userData*/) {
    double cssW = 0, cssH = 0;
    emscripten_get_element_css_size("#canvas", &cssW, &cssH);
    WIDTH = std::max(1, (int)std::round(cssW));
    HEIGHT = std::max(1, (int)std::round(cssH));
    emscripten_set_canvas_element_size("#canvas", WIDTH, HEIGHT);
    SDL_SetWindowSize(g_window, WIDTH, HEIGHT);
    buildScene();
    return EM_TRUE;
}

extern "C" void EMSCRIPTEN_KEEPALIVE start_music() {
    if (g_musicStarted || !g_music) return;
    g_musicStarted = true;
    Mix_PlayMusic(g_music, 1);
}

int main() {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    TTF_Init();
    Mix_Init(MIX_INIT_MP3);

    double cssW = 0, cssH = 0;
    emscripten_get_element_css_size("#canvas", &cssW, &cssH);
    if (cssW > 0 && cssH > 0) {
        WIDTH = (int)std::round(cssW);
        HEIGHT = (int)std::round(cssH);
    }
    emscripten_set_canvas_element_size("#canvas", WIDTH, HEIGHT);

    g_window = SDL_CreateWindow("I love you <3", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        WIDTH, HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED);
    if (!g_renderer) g_renderer = SDL_CreateRenderer(g_window, -1, 0);

    g_fontSans = TTF_OpenFont("/assets/Arimo.ttf", 24);
    g_fontSerif = TTF_OpenFont("/assets/Gelasio.ttf", 54);

    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) == 0) {
        g_music = Mix_LoadMUS("/assets/love_you.mp3");
    }

    buildScene();

    emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_TRUE, onResize);
    emscripten_set_main_loop(mainLoop, 0, 1);
    return 0;
}
