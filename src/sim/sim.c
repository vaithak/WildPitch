/* sim.c - orchestration: config, init, kickoff reset, per-tick order, match flow, hashing.
 * Tick order: clear events -> players (input, movement, kicks, control) -> ball (substepped physics,
 * collisions, goal/out detection) -> match flow (goal/out pauses, resets) -> bookkeeping. */
#include "sim/sim_internal.h"
#include <string.h>

#define WP_SIMSTATE_SIZE_EXPECTED 1520  /* x86-64 gcc/clang; see _Static_assert near sim_hash */

WpMatchConfig sim_default_config(void) {
    WpMatchConfig c;
    memset(&c, 0, sizeof c);
    c.seed = 1;
    c.n_per_side = 5;
    c.pitch.length = FX(40.0);
    c.pitch.width = FX(25.0);
    c.pitch.goal_width = FX(4.0);
    c.pitch.goal_height = FX(2.0);
    c.pitch.goal_depth = FX(1.2);
    return c;
}

/* Kickoff formation for 5-a-side (keeper + 2-1-1), as fractions of half-length/width. Team 0 defends -x.
 * Larger sides fall back to a placeholder spread until per-format formations exist (ROADMAP "Format support").
 * Resets ALL transient player state so nothing (a jump, a kick) survives a reset. */
void match_place_kickoff(SimState *s) {
    static const fx form5[5][2] = {
        {FX(-0.92), FX(0.0)},   /* keeper */
        {FX(-0.55), FX(-0.45)}, {FX(-0.55), FX(0.45)},
        {FX(-0.30), FX(0.0)},
        {FX(-0.06), FX(0.0)},
    };
    fx hl = s->pitch.length / 2, hw = s->pitch.width / 2;
    for (int i = 0; i < 2 * s->n_per_side; i++) {
        WpPlayer *p = &s->players[i];
        int team = i / s->n_per_side, k = i % s->n_per_side;
        fx fxl, fyl;
        if (k < 5) { fxl = form5[k][0]; fyl = form5[k][1]; }
        else { fxl = FX(-0.5) + (fx)(k - 5) * FX(0.08); fyl = (fx)((k % 3) - 1) * FX(0.5); }
        fx px = fx_mul(fxl, hl), py = fx_mul(fyl, hw);
        if (team == 1) { px = -px; py = -py; }
        if (k == 4 && team != s->kickoff_team) px = team == 0 ? -FX(3.5) : FX(3.5);  /* outside the circle */
        fx stamina = p->stamina;
        memset(p, 0, sizeof *p);
        p->stamina = stamina;
        p->pos = fv2_make(px, py);
        p->facing = team == 0 ? 0 : 32768;
        p->team = (uint8_t)team;
        p->role = k == 0 ? WP_ROLE_KEEPER : WP_ROLE_OUTFIELD;
        p->charge_kind = 0xFF;
    }
    memset(&s->ball, 0, sizeof s->ball);
    s->ball.owner = -1;
    s->ball.last_touch = -1;
    s->events |= WP_EV_RESET;
}

void sim_init(SimState *s, const WpMatchConfig *cfg) {
    memset(s, 0, sizeof *s);
    wp_rng_seed(&s->rng, cfg->seed, 0x5157u);
    s->pitch = cfg->pitch;
    s->n_per_side = cfg->n_per_side < 1 ? 1 : (cfg->n_per_side > WP_MAX_PER_SIDE ? WP_MAX_PER_SIDE : cfg->n_per_side);
    for (int i = 0; i < WP_MAX_PLAYERS; i++) s->players[i].stamina = FX_ONE;
    for (int i = 0; i < WP_MAX_SEATS; i++) s->seat_player[i] = -1;
    s->seat_player[0] = (int8_t)(s->n_per_side - 1); /* seat 0 = team 0 striker */
    s->kickoff_team = 0;
    match_place_kickoff(s);
    s->phase = WP_PHASE_PLAY;
    s->events = 0;
}

/* Goal celebration -> kickoff reset; temporary out-of-play rule: ball placed just inside the line. */
static void match_flow(SimState *s) {
    if (s->phase == WP_PHASE_PLAY || s->phase_timer == 0) return;
    if (--s->phase_timer) return;
    if (s->phase == WP_PHASE_GOAL) {
        match_place_kickoff(s);
    } else if (s->phase == WP_PHASE_OUT) {
        WpBall *b = &s->ball;
        fx hl = s->pitch.length / 2 - FX(1.0), hw = s->pitch.width / 2 - FX(0.5);
        fv2 at = fv2_make(fx_clamp(b->pos.x, -hl, hl), fx_clamp(b->pos.y, -hw, hw));
        memset(b, 0, sizeof *b);
        b->pos = at;
        b->owner = -1;
        b->last_touch = -1;
        s->events |= WP_EV_RESET;
    }
    s->phase = WP_PHASE_PLAY;
}

void sim_step(SimState *s, const WpInput *in) {
    s->events = 0;
    players_step(s, in);
    ball_step(s);
    match_flow(s);
    for (int k = 0; k < WP_MAX_SEATS; k++) s->prev_buttons[k] = in->seat[k].buttons;
    s->tick++;
}

/* ---- hashing: explicit fields so struct padding never leaks in ----
 * If this assert fires you changed SimState/WpPlayer/WpBall: hash the new fields in sim_hash(), then update the size. */
_Static_assert(sizeof(SimState) == WP_SIMSTATE_SIZE_EXPECTED, "SimState changed: update sim_hash() and the size");
typedef struct { uint64_t h; } Fnv;
static void fnv_u64(Fnv *f, uint64_t v) {
    for (int i = 0; i < 8; i++) { f->h ^= (v >> (i * 8)) & 0xFF; f->h *= 1099511628211ULL; }
}
static void fnv_i(Fnv *f, int64_t v) { fnv_u64(f, (uint64_t)v); }

uint64_t sim_hash(const SimState *s) {
    Fnv f = {1469598103934665603ULL};
    fnv_i(&f, s->tick); fnv_u64(&f, s->rng.state); fnv_u64(&f, s->rng.inc);
    fnv_i(&f, s->pitch.length); fnv_i(&f, s->pitch.width);
    fnv_i(&f, s->pitch.goal_width); fnv_i(&f, s->pitch.goal_height); fnv_i(&f, s->pitch.goal_depth);
    const WpBall *b = &s->ball;
    fnv_i(&f, b->pos.x); fnv_i(&f, b->pos.y); fnv_i(&f, b->z);
    fnv_i(&f, b->vel.x); fnv_i(&f, b->vel.y); fnv_i(&f, b->vz); fnv_i(&f, b->spin);
    fnv_i(&f, b->owner); fnv_i(&f, b->last_touch);
    fnv_i(&f, s->n_per_side); fnv_i(&f, s->phase); fnv_i(&f, s->phase_timer);
    fnv_i(&f, s->score[0]); fnv_i(&f, s->score[1]); fnv_i(&f, s->kickoff_team); fnv_i(&f, s->events);
    for (int k = 0; k < WP_MAX_SEATS; k++) { fnv_i(&f, s->seat_player[k]); fnv_i(&f, s->prev_buttons[k]); }
    for (int i = 0; i < 2 * s->n_per_side; i++) {
        const WpPlayer *p = &s->players[i];
        fnv_i(&f, p->pos.x); fnv_i(&f, p->pos.y); fnv_i(&f, p->vel.x); fnv_i(&f, p->vel.y);
        fnv_i(&f, p->z); fnv_i(&f, p->vz); fnv_i(&f, p->action); fnv_i(&f, p->action_timer);
        fnv_i(&f, p->facing); fnv_i(&f, p->team); fnv_i(&f, p->role);
        fnv_i(&f, p->kick_charge); fnv_i(&f, p->charge_kind); fnv_i(&f, p->immune);
        fnv_i(&f, p->touch_cooldown); fnv_i(&f, p->stamina);
        const WpKick *k = &p->kick;
        fnv_i(&f, k->power); fnv_i(&f, k->vz); fnv_i(&f, k->spin); fnv_i(&f, k->dir);
        fnv_i(&f, k->foot); fnv_i(&f, k->kind); fnv_i(&f, k->windup); fnv_i(&f, k->done);
    }
    return f.h;
}
