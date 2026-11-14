/* sim.c - match simulation step. CP0.0 skeleton: kickoff layout, player movement from
 * seat input, basic ball flight/bounce/roll. CP0.1 adds dribbling, kicking, goals. */
#include "sim/sim.h"
#include <string.h>

#define WP_SIMSTATE_SIZE_EXPECTED 728  /* x86-64 gcc/clang; see _Static_assert near sim_hash */

/* ---- tuning (metres, seconds) ---- */
#define GRAVITY          FX(9.81)
#define BALL_RADIUS      FX(0.11)
#define BOUNCE_REST      FX(0.55)   /* vertical energy kept on bounce */
#define BOUNCE_FRICTION  FX(0.85)   /* horizontal speed kept on bounce */
#define ROLL_DECEL       FX(1.2)    /* m/s^2 rolling resistance on grass */
#define AIR_DRAG         FX(0.012)  /* per (m/s) per second, quadratic-ish */
#define PLAYER_ACCEL     FX(18.0)
#define PLAYER_MAX_SPEED FX(6.0)
#define PLAYER_SPRINT    FX(8.0)
#define PLAYER_FRICTION  FX(10.0)   /* decel when no input */

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

/* Kickoff formation for 5-a-side (keeper + 2-1-1), expressed as fractions of half-length/width.
 * Team 0 defends -x. Larger sides fall back to a simple spread until real formations exist. */
static void place_kickoff(SimState *s) {
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
        if (team == 1) { px = -px; py = -py; if (k == 4) px = FX(3.5); } /* non-kicking side stays outside circle */
        p->pos = fv2_make(px, py);
        p->vel = fv2_make(0, 0);
        p->facing = team == 0 ? 0 : 32768;
        p->team = (uint8_t)team;
        p->role = k == 0 ? WP_ROLE_KEEPER : WP_ROLE_OUTFIELD;
        p->kick_charge = 0;
        p->touch_cooldown = 0;
    }
    memset(&s->ball, 0, sizeof s->ball);
    s->ball.owner = -1;
    s->ball.last_touch = -1;
}

void sim_init(SimState *s, const WpMatchConfig *cfg) {
    memset(s, 0, sizeof *s);
    wp_rng_seed(&s->rng, cfg->seed, 0x5157u);
    s->pitch = cfg->pitch;
    s->n_per_side = cfg->n_per_side < 1 ? 1 : (cfg->n_per_side > WP_MAX_PER_SIDE ? WP_MAX_PER_SIDE : cfg->n_per_side);
    for (int i = 0; i < WP_MAX_PLAYERS; i++) s->players[i].stamina = FX_ONE;
    for (int i = 0; i < WP_MAX_SEATS; i++) s->seat_player[i] = -1;
    s->seat_player[0] = (int8_t)(s->n_per_side - 1); /* seat 0 = team 0 striker */
    place_kickoff(s);
    s->phase = WP_PHASE_PLAY;
}

static void step_player_from_input(WpPlayer *p, const WpSeatInput *in) {
    fv2 want = fv2_make((fx)in->move_x * (FX_ONE / 127), (fx)in->move_y * (FX_ONE / 127));
    want = fv2_clamp_len(want, FX_ONE);
    fx top = (in->buttons & WP_BTN_SPRINT) ? PLAYER_SPRINT : PLAYER_MAX_SPEED;
    fv2 target = fv2_scale(want, top);
    fv2 dv = fv2_sub(target, p->vel);
    fx max_dv = fx_mul(fv2_len2_wide(want) ? PLAYER_ACCEL : PLAYER_FRICTION, WP_DT);
    p->vel = fv2_add(p->vel, fv2_clamp_len(dv, max_dv));
    if (fv2_len2_wide(want)) p->facing = fx_atan2(want.y, want.x);
}

static void step_ball(SimState *s) {
    WpBall *b = &s->ball;
    fx speed = fv2_len(b->vel);
    /* air drag (always) */
    if (speed > 0) {
        fx drag = fx_mul(fx_mul(AIR_DRAG, speed), WP_DT);
        b->vel = fv2_sub(b->vel, fv2_scale(b->vel, fx_min(drag, FX_ONE)));
    }
    if (b->z > 0 || b->vz > 0) {
        b->vz -= fx_mul(GRAVITY, WP_DT);
        b->z += fx_mul(b->vz, WP_DT);
        if (b->z <= 0) {
            b->z = 0;
            if (b->vz < -FX(0.6)) {
                b->vz = -fx_mul(b->vz, BOUNCE_REST);
                b->vel = fv2_scale(b->vel, BOUNCE_FRICTION);
            } else {
                b->vz = 0;
            }
        }
    } else {
        /* rolling: constant deceleration */
        speed = fv2_len(b->vel);
        fx dec = fx_mul(ROLL_DECEL, WP_DT);
        if (speed <= dec) b->vel = fv2_make(0, 0);
        else b->vel = fv2_scale(b->vel, fx_div(speed - dec, speed));
    }
    b->pos = fv2_add(b->pos, fv2_scale(b->vel, WP_DT));
}

void sim_step(SimState *s, const WpInput *in) {
    for (int i = 0; i < 2 * s->n_per_side; i++) {
        WpPlayer *p = &s->players[i];
        int seat = -1;
        for (int k = 0; k < WP_MAX_SEATS; k++) if (s->seat_player[k] == i) { seat = k; break; }
        if (seat >= 0) step_player_from_input(p, &in->seat[seat]);
        else { WpSeatInput idle = {0, 0, 0}; step_player_from_input(p, &idle); }
        p->pos = fv2_add(p->pos, fv2_scale(p->vel, WP_DT));
        if (p->touch_cooldown) p->touch_cooldown--;
    }
    step_ball(s);
    for (int k = 0; k < WP_MAX_SEATS; k++) s->prev_buttons[k] = in->seat[k].buttons;
    s->tick++;
}

/* ---- hashing: explicit fields so struct padding never leaks in ----
 * If this assert fires you changed SimState/WpPlayer/WpBall: hash the new fields in sim_hash(), then update the sizes. */
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
    fnv_i(&f, s->score[0]); fnv_i(&f, s->score[1]);
    for (int k = 0; k < WP_MAX_SEATS; k++) { fnv_i(&f, s->seat_player[k]); fnv_i(&f, s->prev_buttons[k]); }
    for (int i = 0; i < 2 * s->n_per_side; i++) {
        const WpPlayer *p = &s->players[i];
        fnv_i(&f, p->pos.x); fnv_i(&f, p->pos.y); fnv_i(&f, p->vel.x); fnv_i(&f, p->vel.y);
        fnv_i(&f, p->facing); fnv_i(&f, p->team); fnv_i(&f, p->role);
        fnv_i(&f, p->kick_charge); fnv_i(&f, p->touch_cooldown); fnv_i(&f, p->stamina);
    }
    return f.h;
}
