/* juice.c - see juice.h. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "game/juice.h"
#include "game/avatar.h"   /* wp_soft_disc */

#define TRAIL_LIFE 0.24f   /* seconds a trail point lives */
#define NET_SETTLE 0.9f    /* seconds the net ripples */

static float tof(fx v) { return (float)v / 65536.0f; }
static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

void wp_juice_reset(WpJuice *j) {
    j->trail_n = 0;
    j->trail_head = 0;
    for (int i = 0; i < WP_PUFF_N; i++) j->puff[i].life = 0;
    j->have_last = false;
}

static void puff(WpJuice *j, float x, float y, float size, float vx, float vy, float life) {
    WpPuff *p = &j->puff[j->puff_next];
    j->puff_next = (j->puff_next + 1) % WP_PUFF_N;
    *p = (WpPuff){x, y, 0, vx, vy, 0, life, size};
}

/* a small cloud: a few puffs spread around (x, y), drifting along (dx, dy) */
static void dust(WpJuice *j, float x, float y, float dx, float dy, float size, int n) {
    for (int k = 0; k < n; k++) {
        float a = j->time * 7.3f + (float)k * 2.39996f;           /* golden-angle spread, no RNG */
        float ox = cosf(a) * 0.18f, oy = sinf(a) * 0.18f;
        puff(j, x + ox, y + oy, size * (0.8f + 0.1f * (float)(k % 3)), dx + ox * 1.2f, dy + oy * 1.2f, 0.55f + 0.05f * (float)k);
    }
}

void wp_juice_update(WpJuice *j, const SimState *cur, uint16_t frame_ev, float bx, float by, float bz,
                     const float *px, const float *py, const float *pz, float dt) {
    j->time += dt;
    int np = 2 * cur->n_per_side;
    if (!j->have_last) {
        for (int i = 0; i < np; i++) { j->last_z[i] = pz[i]; j->last_action[i] = cur->players[i].action; j->slide_emit[i] = 0; }
        j->score_seen[0] = cur->score[0]; j->score_seen[1] = cur->score[1];
        j->have_last = true;
    }
    /* trail: sample the drawn ball while it's fast; old points age out on their own */
    float vx = tof(cur->ball.vel.x), vy = tof(cur->ball.vel.y), vz = tof(cur->ball.vz);
    float hs = sqrtf(vx * vx + vy * vy), s3 = sqrtf(hs * hs + vz * vz);
    bool fast = cur->ball.owner < 0 && (s3 > 15.0f || (bz > 1.2f && hs > 9.0f));
    while (j->trail_n > 0) {                                       /* drop expired points (oldest first) */
        int oldest = (j->trail_head - j->trail_n + WP_TRAIL_N) % WP_TRAIL_N;
        if (j->time - j->trail[oldest].t <= TRAIL_LIFE) break;
        j->trail_n--;
    }
    if (fast) {
        if (j->trail_n > 0) {                                      /* a jump (teleport) restarts the trail */
            const WpTrailPt *l = &j->trail[(j->trail_head - 1 + WP_TRAIL_N) % WP_TRAIL_N];
            float dx = bx - l->x, dy = by - l->y;
            if (dx * dx + dy * dy > 9.0f) j->trail_n = 0;
        }
        j->trail[j->trail_head] = (WpTrailPt){bx, by, bz, j->time};
        j->trail_head = (j->trail_head + 1) % WP_TRAIL_N;
        if (j->trail_n < WP_TRAIL_N) j->trail_n++;
    }
    /* the net: where it was hit, how hard (a ball already resting in it doesn't keep re-triggering) */
    if (frame_ev & WP_EV_NET) {
        int side = bx > 0 ? 1 : 0;
        float amp = clampf(s3 / 18.0f, 0.15f, 1.0f);
        float left = j->net_t[side] >= 0 && j->net_t[side] < NET_SETTLE ? j->net_amp[side] * expf(-j->net_t[side] / 0.35f) : 0;
        if (amp > left * 1.3f + 0.05f) {
            float hw = tof(cur->pitch.goal_width) * 0.5f;
            j->net_t[side] = 0; j->net_amp[side] = amp;
            j->net_y[side] = clampf(by, -hw, hw); j->net_z[side] = clampf(bz, 0.1f, tof(cur->pitch.goal_height));
        }
    }
    for (int s = 0; s < 2; s++) {
        if (j->net_t[s] >= 0) j->net_t[s] += dt;
        if (j->net_t[s] > NET_SETTLE) j->net_t[s] = -1;
    }
    /* dust: slides (a trail of puffs while he skids), hard landings, hard bounces */
    for (int i = 0; i < np; i++) {
        const WpPlayer *p = &cur->players[i];
        float pvx = tof(p->vel.x), pvy = tof(p->vel.y);
        if (p->action == WP_ACT_SLIDE) {
            if (j->last_action[i] != WP_ACT_SLIDE) { dust(j, px[i], py[i], pvx * 0.3f, pvy * 0.3f, 0.30f, 3); j->slide_emit[i] = 0; }
            j->slide_emit[i] += dt;
            if (j->slide_emit[i] > 0.06f && pvx * pvx + pvy * pvy > 2.0f) {
                j->slide_emit[i] = 0;
                puff(j, px[i] - pvx * 0.05f, py[i] - pvy * 0.05f, 0.26f, pvx * 0.15f, pvy * 0.15f, 0.5f);
            }
        }
        if (j->last_z[i] > 0.12f && pz[i] <= 0.01f)                 /* landed from a jump / header */
            dust(j, px[i], py[i], pvx * 0.2f, pvy * 0.2f, 0.22f, 2);
        /* keeper dive: the sim keeps him at z 0 (the dive is a pose); his body meets the grass ~0.3 s in */
        if (p->action == WP_ACT_KEEPER_DIVE && p->action_timer >= 18 &&
            (j->last_action[i] != WP_ACT_KEEPER_DIVE || j->dive_t[i] < 18))
            dust(j, px[i], py[i], pvx * 0.25f, pvy * 0.25f, 0.34f, 4);
        j->dive_t[i] = p->action == WP_ACT_KEEPER_DIVE ? p->action_timer : 0;
        j->last_z[i] = pz[i];
        j->last_action[i] = p->action;
    }
    if ((frame_ev & WP_EV_BOUNCE) && vz > 4.0f && bz < 0.3f) dust(j, bx, by, vx * 0.1f, vy * 0.1f, 0.16f, 2);
    for (int k = 0; k < WP_PUFF_N; k++) {
        WpPuff *p = &j->puff[k];
        if (p->age >= p->life) continue;
        p->age += dt;
        p->x += p->vx * dt; p->y += p->vy * dt;
        p->vx *= expf(-dt * 3.0f); p->vy *= expf(-dt * 3.0f);
        p->z += 0.35f * dt;
    }
    /* goal: flash + banner clock; the scorer is whose score went up */
    if (frame_ev & WP_EV_GOAL) {
        j->goal_team = cur->score[1] > j->score_seen[1] ? 1 : 0;
        j->flash = 1.0f;
        j->banner_t = 0;
    }
    j->score_seen[0] = cur->score[0]; j->score_seen[1] = cur->score[1];
    j->flash *= expf(-dt / 0.18f);
    j->banner_t += dt;
}

float wp_juice_net_bulge(const WpJuice *j, int sgn, float y, float z, float depth01) {
    int s = sgn > 0 ? 1 : 0;
    float t = j->net_t[s];
    if (t < 0) return 0;
    float dy = (y - j->net_y[s]) / 0.85f, dz = (z - j->net_z[s]) / 0.65f;
    float osc = expf(-t / 0.35f) * cosf(6.2831853f * 2.4f * t);   /* pushed out, then rippling back */
    return 0.5f * j->net_amp[s] * expf(-(dy * dy + dz * dz)) * depth01 * osc;
}

void wp_juice_draw_trail(const WpJuice *j, SDL_Renderer *r, const WpCamera *cam) {
    if (j->trail_n < 2) return;
    enum { MAXV = 4 * WP_TRAIL_N };
    SDL_Vertex v[MAXV];
    int idx[6 * WP_TRAIL_N], nv = 0, ni = 0;
    for (int k = 0; k < j->trail_n - 1; k++) {                     /* oldest -> newest segments */
        const WpTrailPt *a = &j->trail[(j->trail_head - j->trail_n + k + WP_TRAIL_N) % WP_TRAIL_N];
        const WpTrailPt *b = &j->trail[(j->trail_head - j->trail_n + k + 1 + WP_TRAIL_N) % WP_TRAIL_N];
        WpScreenPt pa = wp_project(cam, a->x, a->y, a->z + 0.11f), pb = wp_project(cam, b->x, b->y, b->z + 0.11f);
        float dx = pb.x - pa.x, dy = pb.y - pa.y, l = sqrtf(dx * dx + dy * dy);
        if (l < 0.5f) continue;
        float fa = 1.0f - (j->time - a->t) / TRAIL_LIFE, fb = 1.0f - (j->time - b->t) / TRAIL_LIFE;
        fa = clampf(fa, 0, 1); fb = clampf(fb, 0, 1);
        float wa = 0.11f * 1.4f * cam->ppm * pa.s * fa, wb = 0.11f * 1.4f * cam->ppm * pb.s * fb;
        float nx = -dy / l, ny = dx / l;
        SDL_FColor ca = {1.0f, 0.97f, 0.88f, 0.42f * fa * fa}, cb = {1.0f, 0.97f, 0.88f, 0.42f * fb * fb};
        v[nv] = (SDL_Vertex){{pa.x + nx * wa, pa.y + ny * wa}, ca, {0, 0}};
        v[nv + 1] = (SDL_Vertex){{pa.x - nx * wa, pa.y - ny * wa}, ca, {0, 0}};
        v[nv + 2] = (SDL_Vertex){{pb.x + nx * wb, pb.y + ny * wb}, cb, {0, 0}};
        v[nv + 3] = (SDL_Vertex){{pb.x - nx * wb, pb.y - ny * wb}, cb, {0, 0}};
        int q[6] = {nv, nv + 1, nv + 2, nv + 1, nv + 3, nv + 2};
        for (int i = 0; i < 6; i++) idx[ni++] = q[i];
        nv += 4;
    }
    if (ni) SDL_RenderGeometry(r, NULL, v, nv, idx, ni);
}

void wp_juice_draw_puffs(const WpJuice *j, SDL_Renderer *r, SDL_Texture *soft, const WpCamera *cam) {
    for (int k = 0; k < WP_PUFF_N; k++) {
        const WpPuff *p = &j->puff[k];
        if (p->age >= p->life) continue;
        float u = p->age / p->life, rad = p->size * (0.6f + 1.2f * u);
        WpScreenPt g = wp_project(cam, p->x, p->y, p->z);
        float a = 0.5f * (1.0f - u) * (1.0f - u);
        wp_soft_disc(r, soft, g.x, g.y, rad * cam->ppm * g.s, rad * cam->ppm * g.s * 0.75f, (SDL_FColor){0.78f, 0.73f, 0.60f, a});
    }
}

static void text(SDL_Renderer *r, float x, float y, float sc, const char *s, SDL_FColor c) {
    SDL_SetRenderScale(r, sc, sc);
    SDL_SetRenderDrawColorFloat(r, c.r, c.g, c.b, c.a);
    SDL_RenderDebugText(r, x / sc, y / sc, s);
    SDL_SetRenderScale(r, 1, 1);
}

void wp_juice_draw_goal(const WpJuice *j, SDL_Renderer *r, int w, int h, const SimState *cur,
                        const char *name0, const char *name1, SDL_FColor col0, SDL_FColor col1) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    if (j->flash > 0.01f) {                                        /* the whole screen flashes once */
        SDL_SetRenderDrawColorFloat(r, 1, 1, 0.92f, 0.45f * j->flash);
        SDL_RenderFillRect(r, NULL);
    }
    float t = j->banner_t, in = clampf(t / 0.28f, 0, 1);
    in = 1.0f - (1.0f - in) * (1.0f - in) * (1.0f - in);           /* ease out */
    SDL_FColor tc = j->goal_team ? col1 : col0;
    float W = (float)w, H = (float)h, bh = 84.0f, y0 = H * 0.10f;
    SDL_FRect band = {-W + W * in, y0, W, bh};                     /* the scorer's colour sweeps in */
    SDL_SetRenderDrawColorFloat(r, tc.r * 0.8f, tc.g * 0.8f, tc.b * 0.8f, 0.6f);
    SDL_RenderFillRect(r, &band);
    SDL_FRect edge = {band.x, y0 + bh - 4, W, 4};
    SDL_SetRenderDrawColorFloat(r, 1, 0.92f, 0.35f, 0.9f);
    SDL_RenderFillRect(r, &edge);
    const char *msg = "G O A L !";
    float pop = 1.0f + 0.5f * expf(-t / 0.10f) * (t > 0.05f ? 1.0f : 0.0f);
    float sc = 4.0f * pop, tw = 8.0f * (float)strlen(msg) * sc;
    float lum = 0.3f * tc.r + 0.59f * tc.g + 0.11f * tc.b;
    SDL_FColor ink = lum > 0.62f ? (SDL_FColor){0.08f, 0.08f, 0.1f, 1} : (SDL_FColor){1, 1, 1, 1};   /* readable on any kit */
    float tx = (W - tw) * 0.5f, ty = y0 + bh * 0.5f - 4.0f * sc - 8.0f;
    text(r, tx + 3, ty + 3, sc, msg, (SDL_FColor){0, 0, 0, 0.45f});
    text(r, tx, ty, sc, msg, ink);
    char sub[64];
    snprintf(sub, sizeof sub, "%s  %d - %d  %s", name0, cur->score[0], cur->score[1], name1);
    float ssc = 2.0f, sw = 8.0f * (float)strlen(sub) * ssc;
    text(r, (W - sw) * 0.5f, y0 + bh - 26.0f, ssc, sub, ink);
}
