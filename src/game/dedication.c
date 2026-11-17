/* dedication.c - see dedication.h. A dark card, the line centred, a small red heart after it. The heart is drawn
 * as a shape (the debug font is ASCII-only, and a font library would be a new dependency). */
#include <math.h>
#include <string.h>
#include "game/dedication.h"

void wp_draw_heart(SDL_Renderer *r, float cx, float cy, float size, float a) {
    enum { N = 48 };
    SDL_Vertex v[N + 1];
    int idx[N * 3];
    SDL_FColor col = {0.86f, 0.12f, 0.20f, a};
    v[0] = (SDL_Vertex){{cx, cy}, col, {0, 0}};
    for (int i = 0; i < N; i++) {            /* classic heart curve, y flipped for screen space */
        float t = (float)i / N * 6.2831853f, s = sinf(t);
        float x = 16.0f * s * s * s;
        float y = 13.0f * cosf(t) - 5.0f * cosf(2 * t) - 2.0f * cosf(3 * t) - cosf(4 * t);
        v[i + 1] = (SDL_Vertex){{cx + x * size / 32.0f, cy - y * size / 32.0f}, col, {0, 0}};
        idx[i * 3] = 0; idx[i * 3 + 1] = i + 1; idx[i * 3 + 2] = (i + 1) % N + 1;
    }
    SDL_RenderGeometry(r, NULL, v, N + 1, idx, N * 3);
}

void wp_dedication_draw(SDL_Renderer *r, int w, int h, float t) {
    float in = fminf(t / 0.8f, 1.0f), out = fminf((WP_DEDICATION_SECONDS - t) / 0.8f, 1.0f);
    float a = fmaxf(fminf(in, out), 0.0f);
    a = a * a * (3.0f - 2.0f * a);           /* smoothstep */
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 10, 12, 16, 255);
    SDL_RenderClear(r);
    const char *msg = WP_DEDICATION_TEXT;
    float len = (float)strlen(msg) * 8.0f;   /* SDL debug font: 8 px per glyph */
    float sc = floorf(fminf(3.0f, (float)w * 0.7f / (len + 24.0f)));
    if (sc < 1.0f) sc = 1.0f;
    float heart = 10.0f;                     /* in scaled units */
    float x0 = ((float)w / sc - (len + 6.0f + heart)) * 0.5f, y0 = (float)h / sc * 0.5f - 4.0f;
    SDL_SetRenderScale(r, sc, sc);
    SDL_SetRenderDrawColor(r, 236, 232, 222, (Uint8)(255.0f * a));
    SDL_RenderDebugText(r, x0, y0, msg);
    SDL_SetRenderScale(r, 1.0f, 1.0f);
    wp_draw_heart(r, (x0 + len + 6.0f + heart * 0.5f) * sc, (y0 + 4.0f) * sc, heart * sc, a);
}

bool wp_dedication_run(SDL_Renderer *r) {
    Uint64 start = SDL_GetTicksNS();
    for (;;) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) return false;
            if (e.type == SDL_EVENT_KEY_DOWN || e.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
                e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) return true;
        }
        float t = (float)(SDL_GetTicksNS() - start) / 1e9f;
        if (t >= WP_DEDICATION_SECONDS) return true;
        int w, h;
        SDL_GetRenderOutputSize(r, &w, &h);
        Uint64 f0 = SDL_GetTicksNS();
        wp_dedication_draw(r, w, h, t);
        SDL_RenderPresent(r);                /* vsync paces this ... */
        Uint64 took = SDL_GetTicksNS() - f0;
        if (took < 12 * SDL_NS_PER_MS) SDL_DelayNS(16 * SDL_NS_PER_MS - took);   /* ... and without vsync: never spin */
    }
}
