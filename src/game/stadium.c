/* stadium.c - see stadium.h. The board faces are two small textures built once (render-target drawing with the SDL
 * debug font, so no font dependency) and mapped onto the projected boards in panels. */
#include <math.h>
#include "game/stadium.h"
#include "game/dedication.h"
#include "game/render.h"
#include "sim/tuning.h"

enum { BANNER_W = 512, DEDIC_W = 768, PANEL_H = 64, SUBDIV = 4 };

static float tof(fx v) { return (float)v / 65536.0f; }

static void board_extent(const WpPitch *p, float *xb, float *yb, float *h) {
    *xb = tof(p->length) / 2 + tof(T_BOARD_END);
    *yb = tof(p->width) / 2 + tof(T_BOARD_SIDE);
    *h = tof(T_BOARD_H);
}

static void fill_poly(SDL_Renderer *r, const float *xs, const float *ys, int n, SDL_FColor col) {
    SDL_Vertex v[8];
    int idx[18], k = 0;
    for (int i = 0; i < n; i++) v[i] = (SDL_Vertex){{xs[i], ys[i]}, col, {0, 0}};
    for (int i = 1; i + 1 < n; i++) { idx[k++] = 0; idx[k++] = i; idx[k++] = i + 1; }
    SDL_RenderGeometry(r, NULL, v, n, idx, k);
}

/* a classic black-and-white football, centred at (cx, cy) */
static void draw_football(SDL_Renderer *r, float cx, float cy, float rad) {
    wp_fill_ellipse(r, cx, cy, rad + 1.5f, rad + 1.5f, (SDL_FColor){0.05f, 0.05f, 0.08f, 1});
    wp_fill_ellipse(r, cx, cy, rad, rad, (SDL_FColor){0.97f, 0.97f, 0.97f, 1});
    float px[5], py[5];
    for (int i = 0; i < 5; i++) {
        float a = -1.5707963f + (float)i * 1.2566371f;
        px[i] = cx + cosf(a) * rad * 0.36f; py[i] = cy + sinf(a) * rad * 0.36f;
    }
    fill_poly(r, px, py, 5, (SDL_FColor){0.08f, 0.08f, 0.10f, 1});
    SDL_SetRenderDrawColor(r, 20, 20, 26, 255);
    for (int i = 0; i < 5; i++) {                         /* seams to the rim, and a patch on the rim */
        float a = -1.5707963f + (float)i * 1.2566371f;
        float ex = cx + cosf(a) * rad * 0.92f, ey = cy + sinf(a) * rad * 0.92f;
        SDL_RenderLine(r, px[i], py[i], ex, ey);
        float qx[4], qy[4];
        for (int k = 0; k < 4; k++) {
            float b = a + ((float)k - 1.5f) * 0.16f, rr = k == 0 || k == 3 ? rad * 0.99f : rad * 0.82f;
            qx[k] = cx + cosf(b) * rr; qy[k] = cy + sinf(b) * rr;
        }
        fill_poly(r, qx, qy, 4, (SDL_FColor){0.08f, 0.08f, 0.10f, 1});
    }
}

static void panel_background(SDL_Renderer *r, int w) {
    SDL_SetRenderDrawColor(r, 14, 30, 74, 255);
    SDL_RenderClear(r);
    SDL_SetRenderDrawColor(r, 250, 196, 40, 255);         /* gold trim top and bottom */
    SDL_FRect top = {0, 0, (float)w, 4}, bot = {0, PANEL_H - 3, (float)w, 3};
    SDL_RenderFillRect(r, &top);
    SDL_RenderFillRect(r, &bot);
}

static SDL_Texture *make_panel(SDL_Renderer *r, int w, int dedication) {
    SDL_Texture *t = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, w, PANEL_H);
    if (!t) return NULL;
    SDL_SetTextureScaleMode(t, SDL_SCALEMODE_LINEAR);
    SDL_SetRenderTarget(r, t);
    panel_background(r, w);
    if (!dedication) {                                    /* WILDPITCH, then the football */
        const char *name = "WILDPITCH";
        float sc = 4.0f, tw = 9.0f * 8.0f * sc, logo = 44.0f, gap = 16.0f;
        float x0 = ((float)w - (tw + gap + logo)) * 0.5f;
        SDL_SetRenderScale(r, sc, sc);
        SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
        SDL_RenderDebugText(r, x0 / sc, (PANEL_H * 0.5f - 4.0f * sc) / sc + 0.25f, name);
        SDL_SetRenderScale(r, 1, 1);
        draw_football(r, x0 + tw + gap + logo * 0.5f, PANEL_H * 0.5f, logo * 0.5f);
    } else {   /* the owner's words (D-018), smaller than the name, right-aligned on two lines, the heart last */
        const char *msg = WP_DEDICATION_TEXT;
        const char *brk = SDL_strstr(msg, " Shubham");     /* "In the memory of my brother" / "Shubham Thakkar" */
        int n1 = brk ? (int)(brk - msg) : (int)SDL_strlen(msg);
        char line1[64];
        SDL_strlcpy(line1, msg, (size_t)(n1 + 1) < sizeof line1 ? (size_t)(n1 + 1) : sizeof line1);
        const char *line2 = brk ? brk + 1 : "";
        float sc = 3.0f, g = 8.0f * sc, heart = 22.0f, gap = 8.0f, margin = 16.0f;
        float w1 = (float)SDL_strlen(line1) * g, w2 = (float)SDL_strlen(line2) * g;
        float right = (float)w - margin;
        SDL_SetRenderScale(r, sc, sc);
        SDL_SetRenderDrawColor(r, 236, 232, 222, 255);
        SDL_RenderDebugText(r, (right - w1) / sc, 5.0f / sc, line1);
        SDL_RenderDebugText(r, (right - heart - gap - w2) / sc, (PANEL_H - 5.0f - g) / sc, line2);
        SDL_SetRenderScale(r, 1, 1);
        wp_draw_heart(r, right - heart * 0.5f, PANEL_H - 5.0f - g * 0.5f, heart, 1.0f);
    }
    return t;
}

static void ensure_ready(SDL_Renderer *r, WpStadium *st) {
    if (st->ready) return;
    SDL_Texture *prev = SDL_GetRenderTarget(r);
    float sx = 1, sy = 1;
    SDL_GetRenderScale(r, &sx, &sy);
    SDL_SetRenderScale(r, 1, 1);
    st->banner = make_panel(r, BANNER_W, 0);
    st->dedication = make_panel(r, DEDIC_W, 1);
    SDL_SetRenderTarget(r, prev);
    SDL_SetRenderScale(r, sx, sy);
    st->ready = true;
}

/* one textured panel standing on the ground from (ax, ay) to (bx, by), h tall; texture u runs a -> b */
static void panel(SDL_Renderer *r, SDL_Texture *tex, const WpCamera *c, float ax, float ay, float bx, float by, float h) {
    SDL_FColor white = {1, 1, 1, 1};
    for (int k = 0; k < SUBDIV; k++) {
        float u0 = (float)k / SUBDIV, u1 = (float)(k + 1) / SUBDIV;
        float x0 = ax + (bx - ax) * u0, y0 = ay + (by - ay) * u0, x1 = ax + (bx - ax) * u1, y1 = ay + (by - ay) * u1;
        WpScreenPt g0 = wp_project(c, x0, y0, 0), g1 = wp_project(c, x1, y1, 0);
        WpScreenPt t0 = wp_project(c, x0, y0, h), t1 = wp_project(c, x1, y1, h);
        SDL_Vertex v[4] = {
            {{t0.x, t0.y}, white, {u0, 0}}, {{t1.x, t1.y}, white, {u1, 0}},
            {{g1.x, g1.y}, white, {u1, 1}}, {{g0.x, g0.y}, white, {u0, 1}},
        };
        int idx[6] = {0, 1, 2, 0, 2, 3};
        SDL_RenderGeometry(r, tex, v, 4, idx, 6);
    }
}

/* a whole board from a to b (reading direction): banner panels, the dedication panel at the b (right-hand) end */
static void board(SDL_Renderer *r, WpStadium *st, const WpCamera *c, float ax, float ay, float bx, float by, float h) {
    float len = hypotf(bx - ax, by - ay);
    if (len < 1.0f) return;
    float ux = (bx - ax) / len, uy = (by - ay) / len;
    float ld = h * (float)DEDIC_W / PANEL_H, ln = h * (float)BANNER_W / PANEL_H;
    if (ld > len) ld = len;
    int n = (int)lroundf((len - ld) / ln);
    float step = n > 0 ? (len - ld) / (float)n : 0;
    float at = 0;
    for (int i = 0; i < n; i++, at += step)
        panel(r, st->banner, c, ax + ux * at, ay + uy * at, ax + ux * (at + step), ay + uy * (at + step), h);
    panel(r, st->dedication, c, ax + ux * at, ay + uy * at, bx, by, h);
    SDL_SetRenderDrawColor(r, 8, 14, 34, 255);            /* top rail */
    WpScreenPt p0 = wp_project(c, ax, ay, h), p1 = wp_project(c, bx, by, h);
    SDL_RenderLine(r, p0.x, p0.y, p1.x, p1.y);
}

static void corner_flag(SDL_Renderer *r, const WpCamera *c, float x, float y, float time) {
    const float pole = 1.5f;
    WpScreenPt g = wp_project(c, x, y, 0), t = wp_project(c, x, y, pole);
    float w = fmaxf(1.5f, 0.05f * c->ppm * g.s);
    SDL_SetRenderDrawColor(r, 245, 245, 245, 255);
    for (float o = -w * 0.5f; o <= w * 0.5f; o += 1.0f) SDL_RenderLine(r, g.x + o, g.y, t.x + o, t.y);
    float wave = sinf(time * 5.0f + x * 0.3f + y * 0.2f) * 0.08f;
    float dir = x > 0 ? 1.0f : -1.0f;                     /* the flag blows outward */
    WpScreenPt a = wp_project(c, x, y, pole), b = wp_project(c, x, y, pole - 0.4f);
    WpScreenPt tip = wp_project(c, x + dir * 0.55f, y, pole - 0.18f + wave);
    SDL_Vertex v[3] = {
        {{a.x, a.y}, {0.98f, 0.82f, 0.10f, 1}, {0, 0}},
        {{b.x, b.y}, {0.98f, 0.82f, 0.10f, 1}, {0, 0}},
        {{tip.x, tip.y}, {0.90f, 0.20f, 0.12f, 1}, {0, 0}},
    };
    SDL_RenderGeometry(r, NULL, v, 3, NULL, 0);
}

void wp_stadium_draw_ground(SDL_Renderer *r, const WpCamera *c, const WpPitch *p) {
    float xb, yb, h;
    board_extent(p, &xb, &yb, &h);
    const float e = 16.0f;                                /* the stands' concourse beyond the boards */
    WpScreenPt a = wp_project(c, -xb - e, -yb - e, 0), b = wp_project(c, xb + e, -yb - e, 0);
    WpScreenPt d = wp_project(c, -xb - e, yb + e, 0), f = wp_project(c, xb + e, yb + e, 0);
    SDL_FColor col = {0.17f, 0.19f, 0.23f, 1};
    SDL_Vertex v[4] = {{{a.x, a.y}, col, {0, 0}}, {{b.x, b.y}, col, {0, 0}}, {{f.x, f.y}, col, {0, 0}}, {{d.x, d.y}, col, {0, 0}}};
    int idx[6] = {0, 1, 2, 0, 2, 3};
    SDL_RenderGeometry(r, NULL, v, 4, idx, 6);
}

void wp_stadium_draw_back(SDL_Renderer *r, WpStadium *st, const WpCamera *c, const WpPitch *p, float time) {
    ensure_ready(r, st);
    float xb, yb, h, hl = tof(p->length) / 2, hw = tof(p->width) / 2;
    board_extent(p, &xb, &yb, &h);
    board(r, st, c, -xb, yb, xb, yb, h);                  /* far side: reads left to right */
    board(r, st, c, -xb, -yb, -xb, yb, h);                /* left end: faces the pitch, its right is the far end */
    board(r, st, c, xb, yb, xb, -yb, h);                  /* right end: its right is the near end */
    corner_flag(r, c, -hl, hw, time);
    corner_flag(r, c, hl, hw, time);
}

void wp_stadium_draw_front(SDL_Renderer *r, WpStadium *st, const WpCamera *c, const WpPitch *p, float time) {
    ensure_ready(r, st);
    float xb, yb, h, hl = tof(p->length) / 2, hw = tof(p->width) / 2;
    board_extent(p, &xb, &yb, &h);
    corner_flag(r, c, -hl, -hw, time);
    corner_flag(r, c, hl, -hw, time);
    board(r, st, c, -xb, -yb, xb, -yb, h);                /* near side (faces the camera here) */
}

void wp_stadium_draw_sheet(SDL_Renderer *r, WpStadium *st, int w, int h) {
    ensure_ready(r, st);
    SDL_SetRenderDrawColor(r, 40, 44, 52, 255);
    SDL_RenderClear(r);
    float s = fminf(2.0f, (float)w * 0.95f / (float)DEDIC_W);
    SDL_FRect a = {20, 20, BANNER_W * s, PANEL_H * s}, b = {20, 40 + PANEL_H * s, DEDIC_W * s, PANEL_H * s};
    SDL_RenderTexture(r, st->banner, NULL, &a);
    SDL_RenderTexture(r, st->dedication, NULL, &b);
    (void)h;
}

void wp_stadium_free(WpStadium *st) {
    if (st->banner) SDL_DestroyTexture(st->banner);
    if (st->dedication) SDL_DestroyTexture(st->dedication);
    st->banner = st->dedication = NULL;
    st->ready = false;
}
