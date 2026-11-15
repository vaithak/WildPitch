/* render.c - tilted 2.5D "tabletop" match renderer on SDL_Renderer. */
#include "game/render.h"
#include "sim/tuning.h"
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
    /* net = the physics box (ball.c): roof at crossbar height, back wall, side walls */
    SDL_SetRenderDrawColor(r, 210, 210, 220, 120);
    for (int i = 0; i <= 8; i++) {
        float y = -gw + (float)i * (2 * gw / 8);
        line3(r, c, gx, y, gh, back, y, gh); line3(r, c, back, y, gh, back, y, 0);
    }
    for (int i = 1; i <= 4; i++) {
        float z = gh * (float)i / 5.0f;
        line3(r, c, back, -gw, z, back, gw, z);
        line3(r, c, gx, -gw, z, back, -gw, z); line3(r, c, gx, gw, z, back, gw, z);
    }
    for (int i = 1; i <= 3; i++) {
        float x = gx + (back - gx) * (float)i / 4.0f;
        line3(r, c, x, -gw, 0, x, -gw, gh); line3(r, c, x, gw, 0, x, gw, gh); line3(r, c, x, -gw, gh, x, gw, gh);
    }
    line3(r, c, back, -gw, 0, back, -gw, gh); line3(r, c, back, gw, 0, back, gw, gh);
    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
    for (int k = -1; k <= 1; k++) {   /* thick posts */
        float o = (float)k * 0.04f;
        line3(r, c, gx + o, -gw, 0, gx + o, -gw, gh); line3(r, c, gx + o, gw, 0, gx + o, gw, gh);
        line3(r, c, gx + o, -gw, gh, gx + o, gw, gh);
    }
}

static float lerpf(float a, float b, float t) { return a + (b - a) * t; }

void wp_render_ctx_init(WpRenderCtx *ctx, SDL_Renderer *r, uint64_t seed) {
    SDL_memset(ctx, 0, sizeof *ctx);
    ctx->soft = wp_soft_disc_create(r);
    ctx->look_seed = seed;
}

void wp_render_reset_anim(WpRenderCtx *ctx) {
    for (int i = 0; i < WP_MAX_PLAYERS; i++) ctx->anim[i].init = false;
}

typedef struct { float y; int kind, idx; } DrawItem;   /* depth-sorted sprites */

void wp_render_match(SDL_Renderer *r, WpRenderCtx *ctx, const WpCamera *cam,
                     const SimState *prev, const SimState *cur, float alpha, float dt) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    ctx->time += dt;
    int np = 2 * cur->n_per_side;
    if (!ctx->looks_ready) {
        for (int i = 0; i < np; i++)
            wp_avatar_make_look(&ctx->look[i], ctx->look_seed, i, cur->players[i].team, cur->players[i].role == WP_ROLE_KEEPER);
        ctx->looks_ready = true;
    }
    const WpAvatarStyle *style = &wp_avatar_styles[ctx->style];

    draw_pitch(r, cam, &cur->pitch);
    draw_goal(r, cam, &cur->pitch, -1);

    DrawItem items[WP_MAX_PLAYERS + 1];
    float px[WP_MAX_PLAYERS], py[WP_MAX_PLAYERS], pz[WP_MAX_PLAYERS];
    int n = 0;
    for (int i = 0; i < np; i++) {
        const WpPlayer *a = &prev->players[i], *b = &cur->players[i];
        px[i] = lerpf(tof(a->pos.x), tof(b->pos.x), alpha);
        py[i] = lerpf(tof(a->pos.y), tof(b->pos.y), alpha);
        pz[i] = lerpf(tof(a->z), tof(b->z), alpha);
        float spd = sqrtf(tof(b->vel.x) * tof(b->vel.x) + tof(b->vel.y) * tof(b->vel.y));
        float face = (float)b->facing * (6.2831853f / 65536.0f);
        wp_avatar_anim_update(&ctx->anim[i], px[i], py[i], face, spd, dt);
        items[n++] = (DrawItem){py[i], 0, i};
    }
    float bx = lerpf(tof(prev->ball.pos.x), tof(cur->ball.pos.x), alpha);
    float by = lerpf(tof(prev->ball.pos.y), tof(cur->ball.pos.y), alpha);
    float bz = lerpf(tof(prev->ball.z), tof(cur->ball.z), alpha);
    items[n++] = (DrawItem){by, 1, 0};
    for (int i = 1; i < n; i++) {   /* far (large y) first */
        DrawItem t = items[i]; int j = i - 1;
        while (j >= 0 && items[j].y < t.y) { items[j + 1] = items[j]; j--; }
        items[j + 1] = t;
    }
    /* shadows on the grass (soft), shrinking with height */
    for (int i = 0; i < np; i++) {
        WpScreenPt g = wp_project(cam, px[i] + 0.12f, py[i] - 0.08f, 0);
        float sh = 1.0f / (1.0f + pz[i] * 0.8f);
        wp_soft_disc(r, ctx->soft, g.x, g.y, 0.42f * cam->ppm * g.s * sh, 0.42f * cam->ppm * g.s * cam->tilt * sh,
                     (SDL_FColor){0, 0, 0, 0.30f * sh});
    }
    {
        WpScreenPt g = wp_project(cam, bx, by, 0);
        float sh = 1.0f / (1.0f + bz * 0.25f);
        wp_soft_disc(r, ctx->soft, g.x, g.y, 0.16f * cam->ppm * g.s * sh, 0.16f * cam->ppm * g.s * cam->tilt * sh,
                     (SDL_FColor){0, 0, 0, 0.35f * sh});
    }
    for (int k = 0; k < n; k++) {
        if (items[k].kind == 0) {
            int i = items[k].idx;
            const WpPlayer *p = &cur->players[i];
            WpAvatarPose pose = {
                .facing = ctx->anim[i].facing,
                .speed = sqrtf(tof(p->vel.x) * tof(p->vel.x) + tof(p->vel.y) * tof(p->vel.y)),
                .phase = ctx->anim[i].phase,
                .z = pz[i],
                .action = p->action,
                .action_t = ((float)p->action_timer + alpha) / (float)WP_TICK_HZ,
                .time = ctx->time + (float)i * 0.37f,
                .keeper = p->role == WP_ROLE_KEEPER,
                .kick_foot = p->kick.foot,
                .kick_kind = p->kick.kind,
                .kick_windup = (float)p->kick.windup / (float)WP_TICK_HZ,
                .kick_power01 = p->kick.kind == WP_KICK_PASS ? 0.3f
                              : (tof(p->kick.power) - 14.0f) / 16.0f,
            };
            if (p->charge_kind != 0xFF && p->kick_charge > 0) {
                float dxb = bx - px[i], dyb = by - py[i];
                if (dxb * dxb + dyb * dyb < 4.0f) pose.charge01 = (float)p->kick_charge / (float)T_CHARGE_MAX_TICKS;
            }
            wp_avatar_draw(r, ctx->soft, cam, px[i], py[i], &pose, &ctx->look[i], style);
            for (int s = 0; s < WP_MAX_SEATS && !ctx->hide_markers; s++)
                if (cur->seat_player[s] == i) {
                    WpScreenPt m = wp_project(cam, px[i], py[i], pz[i] + 2.25f * ctx->look[i].height / 1.8f);
                    float rad = 0.32f * cam->ppm * m.s;
                    SDL_Vertex tri[3] = {
                        {{m.x - rad * 0.6f, m.y - rad * 0.8f}, {1, 0.95f, 0.3f, 1}, {0, 0}},
                        {{m.x + rad * 0.6f, m.y - rad * 0.8f}, {1, 0.95f, 0.3f, 1}, {0, 0}},
                        {{m.x, m.y}, {1, 0.95f, 0.3f, 1}, {0, 0}},
                    };
                    SDL_RenderGeometry(r, NULL, tri, 3, NULL, 0);
                    if (p->charge_kind != 0xFF && p->kick_charge > 0) {   /* power bar while charging */
                        float c = (float)p->kick_charge / (float)T_CHARGE_MAX_TICKS;
                        float bw = 1.1f * cam->ppm * m.s, bh = 0.16f * cam->ppm * m.s + 2.0f;
                        float x0 = m.x - bw * 0.5f, y0 = m.y - rad * 0.8f - bh - 4.0f;
                        SDL_FRect bg = {x0 - 1, y0 - 1, bw + 2, bh + 2}, fg = {x0, y0, bw * c, bh};
                        SDL_SetRenderDrawColor(r, 15, 15, 20, 220);
                        SDL_RenderFillRect(r, &bg);
                        if (p->charge_kind == WP_KICK_CHIP) SDL_SetRenderDrawColor(r, 90, 200, 255, 255);
                        else SDL_SetRenderDrawColor(r, (Uint8)(120 + 135 * c), (Uint8)(230 - 150 * c), 60, 255);
                        SDL_RenderFillRect(r, &fg);
                    }
                }
        } else {
            WpScreenPt b = wp_project(cam, bx, by, bz + 0.11f);
            float br = 0.11f * cam->ppm * b.s * 1.5f;  /* slightly exaggerated for readability */
            wp_soft_disc(r, ctx->soft, b.x, b.y, br + 1.0f, br + 1.0f, (SDL_FColor){0.07f, 0.07f, 0.09f, 1});
            wp_soft_disc(r, ctx->soft, b.x, b.y, br, br, (SDL_FColor){0.98f, 0.98f, 0.98f, 1});
            wp_soft_disc(r, ctx->soft, b.x + br * 0.2f, b.y + br * 0.15f, br * 0.38f, br * 0.38f, (SDL_FColor){0.25f, 0.25f, 0.3f, 1});
        }
    }
    draw_goal(r, cam, &cur->pitch, 1);
}
