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
    /* grass out to the advertising boards (D-033) */
    quad(r, c, -hl - tof(T_BOARD_END), -hw - tof(T_BOARD_SIDE), hl + tof(T_BOARD_END), hw + tof(T_BOARD_SIDE),
         (SDL_FColor){0.16f, 0.36f, 0.18f, 1});
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
    float cr = tof(p->length) * tof(T_KICKOFF_R_FRAC);    /* the sim's kick-off circle (9.15/105 of the length) */
    for (int i = 0; i < 32; i++) {
        float a0 = (float)i * 6.2831853f / 32, a1 = (float)(i + 1) * 6.2831853f / 32;
        line3(r, c, cosf(a0) * cr, sinf(a0) * cr, 0, cosf(a1) * cr, sinf(a1) * cr, 0);
    }
    float bx = hl * 0.30f, bw = hw * 0.48f;             /* penalty areas: 6 x 12 m on the 40 x 25 m pitch */
    for (int sgn = -1; sgn <= 1; sgn += 2) {
        float gx = (float)sgn * hl, ix = gx - (float)sgn * bx;
        line3(r, c, gx, -bw, 0, ix, -bw, 0); line3(r, c, ix, -bw, 0, ix, bw, 0); line3(r, c, ix, bw, 0, gx, bw, 0);
        /* goal area (5.5 m of 105 deep, the posts + 5.5 m of 68 wide): goal kicks are taken from its line (D-038) */
        float gd = tof(p->length) * tof(T_GOALAREA_FRAC), gwh = tof(p->goal_width) * 0.5f + tof(p->width) * (5.5f / 68.0f);
        float jx = gx - (float)sgn * gd;
        line3(r, c, gx, -gwh, 0, jx, -gwh, 0); line3(r, c, jx, -gwh, 0, jx, gwh, 0); line3(r, c, jx, gwh, 0, gx, gwh, 0);
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
    for (int i = 0; i < WP_MAX_PLAYERS; i++) { ctx->anim[i].init = false; ctx->stance[i].valid = false; }
    wp_ball_vis_reset(&ctx->ball);
}

/* next-man indicator (D-050): a "V" chevron of two thick strokes (hollow, dim) or a filled triangle (solid: an
 * automatic switch is about to land on him) */
static void draw_chevron(SDL_Renderer *r, float x, float y, float rad, SDL_FColor c, int solid) {
    if (solid) {
        SDL_Vertex tri[3] = {{{x - rad * 0.6f, y - rad * 0.8f}, c, {0, 0}}, {{x + rad * 0.6f, y - rad * 0.8f}, c, {0, 0}},
                             {{x, y}, c, {0, 0}}};
        SDL_RenderGeometry(r, NULL, tri, 3, NULL, 0);
        return;
    }
    float w = fmaxf(1.5f, rad * 0.22f);
    for (int side = -1; side <= 1; side += 2) {
        float x0 = x + (float)side * rad * 0.6f, y0 = y - rad * 0.8f;
        float dx = x - x0, dy = y - y0, l = sqrtf(dx * dx + dy * dy);
        float nx = -dy / l * w * 0.5f, ny = dx / l * w * 0.5f;
        SDL_Vertex q[6] = {{{x0 + nx, y0 + ny}, c, {0, 0}}, {{x0 - nx, y0 - ny}, c, {0, 0}}, {{x + nx, y + ny}, c, {0, 0}},
                           {{x0 - nx, y0 - ny}, c, {0, 0}}, {{x - nx, y - ny}, c, {0, 0}}, {{x + nx, y + ny}, c, {0, 0}}};
        SDL_RenderGeometry(r, NULL, q, 6, NULL, 0);
    }
}

/* thin elliptical ring on the grass (annulus as a triangle strip) */
static void draw_ring(SDL_Renderer *r, float cx, float cy, float rx, float ry, float th, SDL_FColor col) {
    enum { N = 28 };
    SDL_Vertex v[2 * N + 2];
    int idx[6 * N];
    for (int k = 0; k <= N; k++) {
        float a = (float)k / N * 6.2831853f, c = cosf(a), s = sinf(a);
        v[2 * k] = (SDL_Vertex){{cx + c * rx, cy + s * ry}, col, {0, 0}};
        v[2 * k + 1] = (SDL_Vertex){{cx + c * (rx - th), cy + s * (ry - th * ry / rx)}, col, {0, 0}};
    }
    for (int k = 0; k < N; k++) {
        int a = 2 * k;
        idx[6 * k] = a; idx[6 * k + 1] = a + 1; idx[6 * k + 2] = a + 2;
        idx[6 * k + 3] = a + 1; idx[6 * k + 4] = a + 3; idx[6 * k + 5] = a + 2;
    }
    SDL_RenderGeometry(r, NULL, v, 2 * N + 2, idx, 6 * N);
}

/* Pose clock for actions. Header attempts start by waiting for the ball, so their pose is timed from take-off
 * (jump) or contact (stand); while waiting hold the "ready" part of the pose (arms up, before the nod). */
static float header_pose_t(const WpPlayer *p, float alpha) {
    float t = ((float)p->action_timer + alpha) / (float)WP_TICK_HZ;
    if ((p->action != WP_ACT_JUMP_HEADER && p->action != WP_ACT_STAND_HEADER) || p->kick.kind != WP_KICK_HEADER) return t;
    if (!p->kick.windup) return 0.15f;
    float since = ((float)p->action_timer - (float)(p->kick.windup - 1) + alpha) / (float)WP_TICK_HZ;
    return p->action == WP_ACT_JUMP_HEADER ? since : 0.28f + since;
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

    wp_stadium_draw_ground(r, cam, &cur->pitch);
    draw_pitch(r, cam, &cur->pitch);
    wp_stadium_draw_back(r, &ctx->stadium, cam, &cur->pitch, ctx->time);
    draw_goal(r, cam, &cur->pitch, -1);
    if (cur->restart_kind == WP_RS_FREEKICK && cur->restart_indirect && cur->phase != WP_PHASE_PLAY) {
        /* offside given (D-041): the line where it was when the pass was played */
        float ox = tof(cur->offside_x), hw = tof(cur->pitch.width) / 2;
        SDL_SetRenderDrawColor(r, 255, 220, 40, 255);
        for (int k = -1; k <= 1; k++) line3(r, cam, ox + (float)k * 0.04f, -hw, 0.01f, ox + (float)k * 0.04f, hw, 0.01f);
    }

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
        if ((ctx->frame_events & WP_EV_TOUCH) && cur->ball.last_touch == i) ctx->anim[i].since_touch = 0;
        items[n++] = (DrawItem){py[i], 0, i};
    }
    float bx = lerpf(tof(prev->ball.pos.x), tof(cur->ball.pos.x), alpha);
    float by = lerpf(tof(prev->ball.pos.y), tof(cur->ball.pos.y), alpha);
    float bz = lerpf(tof(prev->ball.z), tof(cur->ball.z), alpha);
    wp_ball_vis_update(&ctx->ball, bx, by, bz);
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
    wp_ball_draw_shadow(r, ctx->soft, cam, bx, by, bz);
    /* pass receiver preview (D-021): the sim's pick, so the ring is exactly who a pass goes to. White, tinting to
     * amber when an opponent is near that lane. */
    for (int k = 0; k < WP_MAX_SEATS && !ctx->hide_markers; k++) {
        int pr = cur->pick_recv[k];
        if (pr < 0 || cur->seat_player[k] < 0) continue;
        float dg = (float)cur->pick_danger[k] / 255.0f;
        WpScreenPt g = wp_project(cam, px[pr], py[pr], 0);
        float rad = 0.55f * cam->ppm * g.s;
        draw_ring(r, g.x, g.y, rad, rad * cam->tilt, fmaxf(1.5f, 0.07f * cam->ppm * g.s),
                  (SDL_FColor){1.0f, 1.0f - 0.25f * dg, 1.0f - 0.75f * dg, 0.85f});
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
                .action_t = header_pose_t(p, alpha),
                .time = ctx->time + (float)i * 0.37f,
                .keeper = p->role == WP_ROLE_KEEPER,
                .touch01 = 1.0f - ctx->anim[i].since_touch / 0.16f,
                .kick_foot = p->kick.foot,
                .holding = cur->ball.held && cur->ball.owner == i,
                .throwin_hold = cur->ball.held && cur->ball.owner == i && cur->restart_kind == WP_RS_THROW &&
                                cur->restart_taker == i && p->role != WP_ROLE_KEEPER,
                .kick_kind = p->kick.kind == WP_KICK_THROUGH ? WP_KICK_PASS     /* same strike, different weight */
                           : p->kick.kind == WP_KICK_LOB ? WP_KICK_CHIP : p->kick.kind,
                .kick_windup = (float)p->kick.windup / (float)WP_TICK_HZ,
                .kick_power01 = (p->kick.kind == WP_KICK_PASS || p->kick.kind == WP_KICK_THROUGH) ? 0.3f
                              : p->kick.kind == WP_KICK_LOB ? (p->kick.power > 0 ? fminf(tof(p->kick.power), 1.0f) : 0.5f)   /* h */
                              : (tof(p->kick.power) - 14.0f) / 16.0f,
            };
            /* a pass tap shows nothing; past the tap window it's a through ball and gets the bar (+ backswing) */
            int charging = p->charge_kind != 0xFF &&
                           p->kick_charge > (p->charge_kind == WP_KICK_PASS ? T_PASS_TAP_TICKS
                                             : p->charge_kind == WP_KICK_LOB ? T_LOB_TAP_TICKS : 0);   /* taps: no bar */
            if (charging) {
                float dxb = bx - px[i], dyb = by - py[i];
                if (dxb * dxb + dyb * dyb < 4.0f) pose.charge01 = (float)p->kick_charge / (float)T_CHARGE_MAX_TICKS;
            }
            wp_stance_fill(prev, cur, i, alpha, px[i], py[i], &ctx->stance[i], &pose);   /* chest / thigh / volley (D-052) */
            wp_avatar_draw(r, ctx->soft, cam, px[i], py[i], &pose, &ctx->look[i], style);
            /* who SWITCH goes to (D-050): only while we haven't got the ball and no pass of ours is travelling */
            for (int s = 0; s < WP_MAX_SEATS && !ctx->hide_markers; s++) {
                int me = cur->seat_player[s];
                if (me < 0 || cur->switch_mode[s] == WP_SWITCH_LEGACY || cur->phase != WP_PHASE_PLAY) continue;
                int team = cur->players[me].team, o = cur->ball.owner;
                if ((o >= 0 && cur->players[o].team == team) ||
                    (cur->pass_target >= 0 && cur->players[cur->pass_from].team == team)) continue;
                int hot = cur->sw_hot[s] && cur->sw_next[s] >= 0;
                if ((hot ? cur->sw_next[s] : cur->sw_q[s]) != i) continue;
                WpScreenPt m = wp_project(cam, px[i], py[i], pz[i] + 2.25f * ctx->look[i].height / 1.8f);
                float rad = 0.28f * cam->ppm * m.s;
                draw_chevron(r, m.x, m.y, rad, hot ? (SDL_FColor){1, 0.95f, 0.3f, 0.85f} : (SDL_FColor){1, 0.95f, 0.3f, 0.55f}, hot);
            }
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
                    if (charging) {   /* power bar while charging (through ball: lead distance) */
                        float c = p->charge_kind == WP_KICK_PASS
                                ? (float)(p->kick_charge - T_PASS_TAP_TICKS) / (float)(T_CHARGE_MAX_TICKS - T_PASS_TAP_TICKS)
                                : p->charge_kind == WP_KICK_LOB      /* lob meter: how far (D-045) */
                                ? (float)(p->kick_charge - T_LOB_TAP_TICKS) /
                                  (float)(cur->restart_kind && cur->restart_kind != WP_RS_KICKOFF && cur->restart_taker == i &&
                                          cur->ball.owner == i ? T_LOB_SET_RAMP : T_CHARGE_MAX_TICKS - T_LOB_TAP_TICKS)   /* lob_ramp_ticks */
                                : (float)p->kick_charge / (float)T_CHARGE_MAX_TICKS;
                        if (c > 1.0f) c = 1.0f;
                        float bw = 1.1f * cam->ppm * m.s, bh = 0.16f * cam->ppm * m.s + 2.0f;
                        float x0 = m.x - bw * 0.5f, y0 = m.y - rad * 0.8f - bh - 4.0f;
                        SDL_FRect bg = {x0 - 1, y0 - 1, bw + 2, bh + 2}, fg = {x0, y0, bw * c, bh};
                        SDL_SetRenderDrawColor(r, 15, 15, 20, 220);
                        SDL_RenderFillRect(r, &bg);
                        if (p->charge_kind == WP_KICK_CHIP) SDL_SetRenderDrawColor(r, 90, 200, 255, 255);
                        else if (p->charge_kind == WP_KICK_PASS) SDL_SetRenderDrawColor(r, 240, 240, 245, 255);
                        else if (p->charge_kind == WP_KICK_LOB) SDL_SetRenderDrawColor(r, 200, 130, 255, 255);   /* lob: violet */
                        else SDL_SetRenderDrawColor(r, (Uint8)(120 + 135 * c), (Uint8)(230 - 150 * c), 60, 255);
                        SDL_RenderFillRect(r, &fg);
                    }
                }
        } else {
            wp_ball_draw(r, ctx->soft, cam, &ctx->ball, bx, by, bz, tof(cur->ball.vel.x), tof(cur->ball.vel.y));
        }
    }
    draw_goal(r, cam, &cur->pitch, 1);
    wp_stadium_draw_front(r, &ctx->stadium, cam, &cur->pitch, ctx->time);
}
