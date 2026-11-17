/* sfx.c - see sfx.h. Kick sounds are chosen by the kicker's WpKick.kind and graded by the ball's speed off the boot;
 * throws and rolls from the hands make no boot sound. Restart whistles have no event bit: a kick-off / penalty
 * whistle is the moment the set piece becomes takeable (phase PLAY with that restart, not so a tick earlier). */
#include <math.h>
#include "game/sfx.h"

static float tof(fx v) { return (float)v / 65536.0f; }
static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

/* small, repeatable pitch variation so repeated kicks don't sound machine-made (presentation hash, not sim RNG) */
static float vary(uint32_t n, float amount) {
    uint32_t h = n * 2654435761u;
    h ^= h >> 15;
    return 1.0f + amount * ((float)(h & 1023) / 511.5f - 1.0f);
}

void wp_sfx_reset(WpSfx *s) {
    *s = (WpSfx){0};
    s->start_whistle = true;
}

static bool takeable(const SimState *x, int kind) { return x->phase == WP_PHASE_PLAY && x->restart_kind == kind; }

void wp_sfx_tick(WpSfx *s, WpAudio *a, const SimState *prev, const SimState *cur, bool replay, float cam_cx, float half_view_m) {
    s->n++;
    uint16_t ev = cur->events;
    const WpBall *b = &cur->ball;
    float bx = tof(b->pos.x), by = tof(b->pos.y);
    float vx = tof(b->vel.x), vy = tof(b->vel.y), vz = tof(b->vz);
    float spd = sqrtf(vx * vx + vy * vy + vz * vz);
    float hl = tof(cur->pitch.length) * 0.5f, hv = half_view_m > 1 ? half_view_m : 1, hgw = tof(cur->pitch.goal_width) * 0.5f;
    float pan = clampf((bx - cam_cx) / hv, -1, 1) * 0.75f;

    if (ev & WP_EV_KICK) {
        int k = b->last_touch;
        uint8_t kind = k >= 0 ? cur->players[k].kick.kind : WP_KICK_PASS;
        if (ev & WP_EV_HEADER) wp_audio_play(a, WP_SND_HEADER, 0.55f + 0.45f * clampf(spd / 15, 0, 1), pan, vary(s->n, 0.06f), 0);
        else if (kind == WP_KICK_SHOT || kind == WP_KICK_PUNT)
            wp_audio_play(a, WP_SND_KICK_SHOT, 0.5f + 0.5f * clampf(spd / 30, 0, 1), pan, vary(s->n, 0.05f) * (kind == WP_KICK_PUNT ? 0.9f : 1), 0);
        else if (kind == WP_KICK_CHIP || kind == WP_KICK_LOB)
            wp_audio_play(a, WP_SND_KICK_LOB, 0.55f + 0.45f * clampf(spd / 25, 0, 1), pan, vary(s->n, 0.05f), 0);
        else if (kind != WP_KICK_THROW && kind != WP_KICK_ROLL)                   /* passes, through balls, volleys */
            wp_audio_play(a, WP_SND_KICK_PASS, 0.4f + 0.6f * clampf(spd / 18, 0, 1), pan, vary(s->n, 0.07f), 0);
        /* goalward strike: going for the goal it is heading to, close enough, on target-ish -> a miss groans */
        float gx = vx > 0 ? hl : -hl;
        if (fabsf(vx) > 7 && fabsf(gx - bx) < 0.45f * tof(cur->pitch.length)) {
            float t = (gx - bx) / vx, yy = by + vy * t;
            if (fabsf(yy) < hgw + 4.0f) s->shot_live = 150;
        }
    } else if ((ev & WP_EV_TOUCH) && s->n - s->last_touch > 8) {             /* dribble touches: a soft tap */
        s->last_touch = s->n;
        wp_audio_play(a, WP_SND_KICK_PASS, 0.16f, pan, 1.15f * vary(s->n, 0.08f), 0);
    }
    if ((ev & WP_EV_BODY) && s->n - s->last_body > 20) {          /* scrambles hit bodies every few ticks */
        s->last_body = s->n;
        wp_audio_play(a, WP_SND_HEADER, 0.35f, pan, 0.85f, 0);
    }
    if ((ev & WP_EV_BOUNCE) && s->n - s->last_bounce > 4) {
        s->last_bounce = s->n;
        float vin = -tof(prev->ball.vz);                                     /* how hard it came down */
        float g = clampf(vin / 8.0f, 0, 1);
        if (g > 0.12f) wp_audio_play(a, WP_SND_BOUNCE, g, pan, vary(s->n, 0.06f), 0);
    }
    if (ev & WP_EV_POST) {
        wp_audio_play(a, WP_SND_POST, 0.6f + 0.4f * clampf(spd / 20, 0, 1), pan, vary(s->n, 0.03f), 0);
        if (!replay) wp_audio_play(a, WP_SND_CROWD_GROAN, 0.9f, 0, 1, 0.12f);
        s->shot_live = 0;
    }
    if ((ev & WP_EV_NET) && s->n - s->last_net > 12) {
        s->last_net = s->n;
        wp_audio_play(a, WP_SND_NET, 0.45f + 0.55f * clampf(spd / 20, 0, 1), pan, vary(s->n, 0.05f), 0);
    }
    for (int i = 0; i < 2 * cur->n_per_side; i++)                            /* slide starts: a scuff */
        if (cur->players[i].action == WP_ACT_SLIDE && prev->players[i].action != WP_ACT_SLIDE)
            wp_audio_play(a, WP_SND_TACKLE, 0.6f, clampf((tof(cur->players[i].pos.x) - cam_cx) / hv, -1, 1) * 0.75f,
                          vary(s->n + (uint32_t)i, 0.08f), 0);
    if (ev & WP_EV_TACKLE) wp_audio_play(a, WP_SND_TACKLE, 0.5f, pan, 1.2f, 0);
    if (replay) return;                                                      /* the referee and crowd don't replay */

    if (ev & WP_EV_GOAL) {
        wp_audio_play(a, WP_SND_CROWD_ROAR, 1.0f, 0, 1, 0);
        wp_audio_play(a, WP_SND_WHISTLE_SHORT, 0.8f, 0, 1, 0.45f);
        s->goal_hold = 4.0f;
        s->shot_live = 0;
    }
    if ((ev & WP_EV_OUT) && s->shot_live > 0 && fabsf(bx) >= hl - 0.2f) {     /* over the bar / wide: "ooh" */
        wp_audio_play(a, WP_SND_CROWD_GROAN, 0.8f, 0, vary(s->n, 0.05f), 0);
        s->shot_live = 0;
    }
    if (s->shot_live > 0) s->shot_live--;
    if (ev & (WP_EV_FOUL | WP_EV_OFFSIDE)) wp_audio_play(a, WP_SND_WHISTLE_SHORT, 0.9f, 0, 1, 0);
    if (ev & WP_EV_WHISTLE) {
        if (cur->phase == WP_PHASE_FULLTIME) {                               /* full time: three blasts */
            wp_audio_play(a, WP_SND_WHISTLE_SHORT, 0.9f, 0, 1, 0);
            wp_audio_play(a, WP_SND_WHISTLE_SHORT, 0.9f, 0, 1, 0.42f);
            wp_audio_play(a, WP_SND_WHISTLE_LONG, 0.9f, 0, 1, 0.84f);
        } else wp_audio_play(a, WP_SND_WHISTLE_LONG, 0.9f, 0, 1, 0);         /* half time */
    }
    bool ko_now = takeable(cur, WP_RS_KICKOFF) || takeable(cur, WP_RS_PENALTY);
    bool ko_before = takeable(prev, cur->restart_kind) && prev->restart_kind == cur->restart_kind;
    if (ko_now && (!ko_before || s->start_whistle)) wp_audio_play(a, WP_SND_WHISTLE_SHORT, 0.8f, 0, 1, 0.1f);
    s->start_whistle = false;
}

void wp_sfx_frame(WpSfx *s, WpAudio *a, const SimState *cur, float dt) {
    float hl = tof(cur->pitch.length) * 0.5f, hw = tof(cur->pitch.width) * 0.5f;
    float bx = tof(cur->ball.pos.x), by = tof(cur->ball.pos.y), raw = 0;
    if (cur->phase == WP_PHASE_PLAY && !cur->restart_kind) {
        float d = hl - fabsf(bx);                                            /* metres from the nearer goal line */
        raw = clampf(1.0f - d / (0.32f * 2 * hl), 0, 1);
        raw *= fabsf(by) < hw * 0.6f ? 1.0f : 0.6f;                          /* central is more dangerous than wide */
        raw = raw * raw;
    }
    float k = 1.0f - expf(-dt / 0.6f);
    s->danger += (raw - s->danger) * k;
    if (s->goal_hold > 0) s->goal_hold -= dt;
    float excite = 0.75f * s->danger + (s->goal_hold > 0 ? clampf(s->goal_hold / 2.0f, 0, 1) : 0);
    float bed = cur->phase == WP_PHASE_FULLTIME ? 0.45f : 0.6f + 0.2f * s->danger;
    wp_audio_crowd(a, bed, clampf(excite, 0, 1));
}
