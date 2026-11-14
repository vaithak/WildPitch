/* render.c - tilted 2.5D "tabletop" match renderer on SDL_Renderer. */
#include "game/render.h"
#include <math.h>

static float tof(fx v) { return (float)v / 65536.0f; }

WpScreenPt wp_project(const WpCamera *c, float x, float y, float z) {
    float d = y - c->cy;                         /* depth: +y is "away" from viewer (up the screen) */
    float s = 1.0f / (1.0f + d * c->persp);
    if (s < 0.2f) s = 0.2f;
    WpScreenPt p;
    p.s = s;
    p.x = (float)c->w * 0.5f + (x - c->cx) * c->ppm * s;
    p.y = (float)c->h * 0.5f - d * c->tilt * c->ppm * s - z * c->ppm * s;
    return p;
}

void wp_fill_ellipse(SDL_Renderer *r, float cx, float cy, float rx, float ry, SDL_FColor col) {
    enum { N = 20 };
    SDL_Vertex v[N + 1];
    int idx[N * 3];
    v[0].position = (SDL_FPoint){cx, cy};
    v[0].color = col;
    for (int i = 0; i < N; i++) {
        float a = (float)i * 6.2831853f / N;
        v[i + 1].position = (SDL_FPoint){cx + cosf(a) * rx, cy + sinf(a) * ry};
        v[i + 1].color = col;
        idx[i * 3] = 0; idx[i * 3 + 1] = i + 1; idx[i * 3 + 2] = (i + 1) % N + 1;
    }
    for (int i = 0; i <= N; i++) v[i].tex_coord = (SDL_FPoint){0, 0};
    SDL_RenderGeometry(r, NULL, v, N + 1, idx, N * 3);
}

static void quad(SDL_Renderer *r, const WpCamera *c, float x0, float y0, float x1, float y1, SDL_FColor col) {
    WpScreenPt a = wp_project(c, x0, y0, 0), b = wp_project(c, x1, y0, 0);
    WpScreenPt d = wp_project(c, x0, y1, 0), e = wp_project(c, x1, y1, 0);
    SDL_Vertex v[4] = {
        {{a.x, a.y}, col, {0, 0}}, {{b.x, b.y}, col, {0, 0}},
        {{e.x, e.y}, col, {0, 0}}, {{d.x, d.y}, col, {0, 0}},
    };
    int idx[6] = {0, 1, 2, 0, 2, 3};
    SDL_RenderGeometry(r, NULL, v, 4, idx, 6);
}

static void line3(SDL_Renderer *r, const WpCamera *c, float x0, float y0, float z0, float x1, float y1, float z1) {
    WpScreenPt a = wp_project(c, x0, y0, z0), b = wp_project(c, x1, y1, z1);
    SDL_RenderLine(r, a.x, a.y, b.x, b.y);
}

static void draw_pitch(SDL_Renderer *r, const WpCamera *c, const WpPitch *p) {
    float hl = tof(p->length) / 2, hw = tof(p->width) / 2;
    /* surround */
    quad(r, c, -hl - 6, -hw - 6, hl + 6, hw + 6, (SDL_FColor){0.16f, 0.36f, 0.18f, 1});
    /* mown stripes */
    int stripes = 10;
    for (int i = 0; i < stripes; i++) {
        float x0 = -hl + (float)i * (2 * hl / (float)stripes), x1 = x0 + 2 * hl / (float)stripes;
        SDL_FColor g = (i & 1) ? (SDL_FColor){0.24f, 0.55f, 0.26f, 1} : (SDL_FColor){0.27f, 0.60f, 0.29f, 1};
        quad(r, c, x0, -hw, x1, hw, g);
    }
    SDL_SetRenderDrawColor(r, 235, 245, 235, 255);
    line3(r, c, -hl, -hw, 0, hl, -hw, 0); line3(r, c, -hl, hw, 0, hl, hw, 0);
    line3(r, c, -hl, -hw, 0, -hl, hw, 0); line3(r, c, hl, -hw, 0, hl, hw, 0);
    line3(r, c, 0, -hw, 0, 0, hw, 0);
    /* centre circle + penalty areas */
    float cr = 3.0f;
    for (int i = 0; i < 32; i++) {
        float a0 = (float)i * 6.2831853f / 32, a1 = (float)(i + 1) * 6.2831853f / 32;
        line3(r, c, cosf(a0) * cr, sinf(a0) * cr, 0, cosf(a1) * cr, sinf(a1) * cr, 0);
    }
    float bx = 6.0f, bw = 6.0f;
    for (int sgn = -1; sgn <= 1; sgn += 2) {
        float gx = (float)sgn * hl, ix = gx - (float)sgn * bx;
        line3(r, c, gx, -bw, 0, ix, -bw, 0); line3(r, c, ix, -bw, 0, ix, bw, 0); line3(r, c, ix, bw, 0, gx, bw, 0);
    }
}

static void draw_goal(SDL_Renderer *r, const WpCamera *c, const WpPitch *p, int sgn) {
    float gx = (float)sgn * tof(p->length) / 2, gw = tof(p->goal_width) / 2, gh = tof(p->goal_height);
    float back = gx + (float)sgn * tof(p->goal_depth);
    SDL_SetRenderDrawColor(r, 200, 200, 210, 140);
    for (int i = 0; i <= 8; i++) {  /* net */
        float y = -gw + (float)i * (2 * gw / 8);
        line3(r, c, gx, y, gh, back, y, gh * 0.6f); line3(r, c, back, y, gh * 0.6f, back, y, 0);
    }
    line3(r, c, back, -gw, gh * 0.6f, back, gw, gh * 0.6f);
    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
    for (int k = -1; k <= 1; k++) {   /* thick posts */
        float o = (float)k * 0.04f;
        line3(r, c, gx + o, -gw, 0, gx + o, -gw, gh); line3(r, c, gx + o, gw, 0, gx + o, gw, gh);
        line3(r, c, gx + o, -gw, gh, gx + o, gw, gh);
    }
}

static float lerpf(float a, float b, float t) { return a + (b - a) * t; }

typedef struct { float y; int kind, idx; } DrawItem;   /* depth-sorted sprites */

void wp_render_match(SDL_Renderer *r, const WpCamera *cam, const SimState *prev, const SimState *cur, float alpha) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    draw_pitch(r, cam, &cur->pitch);
    draw_goal(r, cam, &cur->pitch, -1);  /* far-to-near order doesn't matter much for side goals */

    int np = 2 * cur->n_per_side;
    DrawItem items[WP_MAX_PLAYERS + 1];
    int n = 0;
    for (int i = 0; i < np; i++)
        items[n++] = (DrawItem){lerpf(tof(prev->players[i].pos.y), tof(cur->players[i].pos.y), alpha), 0, i};
    float bx = lerpf(tof(prev->ball.pos.x), tof(cur->ball.pos.x), alpha);
    float by = lerpf(tof(prev->ball.pos.y), tof(cur->ball.pos.y), alpha);
    float bz = lerpf(tof(prev->ball.z), tof(cur->ball.z), alpha);
    items[n++] = (DrawItem){by, 1, 0};
    /* far (large y) first */
    for (int i = 1; i < n; i++) {
        DrawItem t = items[i]; int j = i - 1;
        while (j >= 0 && items[j].y < t.y) { items[j + 1] = items[j]; j--; }
        items[j + 1] = t;
    }
    /* shadows first (on the ground) */
    for (int i = 0; i < np; i++) {
        float x = lerpf(tof(prev->players[i].pos.x), tof(cur->players[i].pos.x), alpha);
        float y = lerpf(tof(prev->players[i].pos.y), tof(cur->players[i].pos.y), alpha);
        WpScreenPt g = wp_project(cam, x + 0.15f, y - 0.1f, 0);
        wp_fill_ellipse(r, g.x, g.y, 0.45f * cam->ppm * g.s, 0.45f * cam->ppm * g.s * cam->tilt, (SDL_FColor){0, 0, 0, 0.28f});
    }
    {
        WpScreenPt g = wp_project(cam, bx, by, 0);
        float sh = 1.0f / (1.0f + bz * 0.25f);
        wp_fill_ellipse(r, g.x, g.y, 0.16f * cam->ppm * g.s * sh, 0.16f * cam->ppm * g.s * cam->tilt * sh,
                        (SDL_FColor){0, 0, 0, 0.35f * sh});
    }
    for (int k = 0; k < n; k++) {
        if (items[k].kind == 0) {
            int i = items[k].idx;
            const WpPlayer *p = &cur->players[i];
            float x = lerpf(tof(prev->players[i].pos.x), tof(p->pos.x), alpha);
            float y = items[k].y;
            WpScreenPt base = wp_project(cam, x, y, 0), head = wp_project(cam, x, y, 1.7f);
            float rad = 0.38f * cam->ppm * base.s;
            SDL_FColor body = p->team == 0 ? (SDL_FColor){0.92f, 0.36f, 0.20f, 1} : (SDL_FColor){0.22f, 0.45f, 0.92f, 1};
            if (p->role == WP_ROLE_KEEPER) body = p->team == 0 ? (SDL_FColor){0.95f, 0.85f, 0.2f, 1} : (SDL_FColor){0.6f, 0.9f, 0.3f, 1};
            /* body: a capsule from feet to shoulders */
            float top = head.y + rad * 0.9f;
            SDL_FRect torso = {base.x - rad, top, rad * 2, base.y - top};
            SDL_SetRenderDrawColor(r, (Uint8)(body.r * 255), (Uint8)(body.g * 255), (Uint8)(body.b * 255), 255);
            SDL_RenderFillRect(r, &torso);
            wp_fill_ellipse(r, base.x, base.y, rad, rad * cam->tilt, body);
            wp_fill_ellipse(r, head.x, head.y + rad * 0.2f, rad * 0.75f, rad * 0.75f, (SDL_FColor){0.96f, 0.80f, 0.66f, 1});
            /* controlled marker */
            for (int s = 0; s < WP_MAX_SEATS; s++)
                if (cur->seat_player[s] == i) {
                    WpScreenPt m = wp_project(cam, x, y, 2.35f);
                    SDL_Vertex tri[3] = {
                        {{m.x - rad * 0.6f, m.y - rad * 0.7f}, {1, 1, 0.3f, 1}, {0, 0}},
                        {{m.x + rad * 0.6f, m.y - rad * 0.7f}, {1, 1, 0.3f, 1}, {0, 0}},
                        {{m.x, m.y}, {1, 1, 0.3f, 1}, {0, 0}},
                    };
                    SDL_RenderGeometry(r, NULL, tri, 3, NULL, 0);
                }
        } else {
            WpScreenPt b = wp_project(cam, bx, by, bz + 0.11f);
            float br = 0.11f * cam->ppm * b.s * 1.6f;  /* slightly exaggerated for readability */
            wp_fill_ellipse(r, b.x, b.y, br, br, (SDL_FColor){0.98f, 0.98f, 0.98f, 1});
            wp_fill_ellipse(r, b.x - br * 0.25f, b.y - br * 0.25f, br * 0.35f, br * 0.35f, (SDL_FColor){0.7f, 0.7f, 0.75f, 1});
        }
    }
    draw_goal(r, cam, &cur->pitch, 1);
}
