/* contact.c - aerial reception (D-047, CP0.5a stage 2): the body part follows the ball's height at the PLANNED
 * contact tick, never its current height.
 *  - Prediction: a ring of T_CONTACT_RING ticks of the untouched ball, stepped by ball.c's own integrator
 *    (ball_predict_tick), so while nothing touches the ball it IS the real ball's future. Kept in a static cache
 *    keyed on the complete ball state + pitch: a hit returns exactly what a fresh fill would (the suffix of the same
 *    sequence, topped up to the full horizon), so replays / snapshots / several sims stay deterministic. Derived data:
 *    not in SimState, not hashed. Only maintained while the loose ball is in the air.
 *  - Planner (the players who matter: the pass's receiver, seat players, each team's ball-winner): every reachable
 *    tick of the ring is a candidate with the stance of its band (CONTROL intent: GROUND trap, LOW instep, MID thigh,
 *    CHEST chest, HEAD header-control only for a slow ball); score = -W_DIFF difficulty - W_TIME dt/30 - W_PRESS
 *    pressure (difficulty = the stance's base + speed over its v_clean + 0.6 setup deficit); ties -> the earliest.
 *    The plan freezes T_CONTACT_COMMIT ticks before contact. The human's receive assist goes to its point (the AI
 *    keeps its own receive / chase targets until stage 4; its touches use the same stance model).
 *  - Touch: outcome + residual. The clean ball pops h_pop and drops d_ready along the exit (stick, recent stick, or
 *    facing) at T_CONTACT_READY_Z, plus his run; what he fails to absorb, (1 - q) e_firm x the 3-D relative velocity
 *    mirrored about the stance's normal, is added; then a "readable" angle error (one RNG draw, only if q < 1). He owns
 *    the dropping ball for its fall time (settle_ticks). Speed is graded: q falls with the 3-D relative speed over
 *    v_clean; above T_CONTACT_NOCTRL_K x v_clean (head: above v_clean) there's no control and the body deflects it. */
#include "sim/sim_internal.h"
#include <string.h>

typedef struct { fx h_pop, d_ready, e_firm, v_clean; int setup, elev_deg; fx reach, base; } Stance;
static const Stance ST[WP_CK_N] = { {0, 0, 0, FX_ONE, 1, 0, 0, 0}, T_ST_TRAP, T_ST_INSTEP, T_ST_THIGH, T_ST_CHEST, T_ST_HEADCTL };

static int band_of(fx z) {
    return z <= T_CB_GROUND ? WP_CB_GROUND : z <= T_CB_LOW ? WP_CB_LOW : z <= T_CB_MID ? WP_CB_MID
         : z <= T_CB_CHEST ? WP_CB_CHEST : z <= T_CB_HEAD ? WP_CB_HEAD : z <= T_CB_JUMP ? WP_CB_JUMP : WP_CB_NONE;
}
static int control_kind(int band) {          /* CONTROL intent: the stance for each band */
    static const uint8_t k[WP_CB_NONE + 1] = { WP_CK_TRAP, WP_CK_INSTEP, WP_CK_THIGH, WP_CK_CHEST, WP_CK_HEADCTL,
                                               WP_CK_NONE, WP_CK_NONE };
    return k[band];
}
static fx speed3(fv2 v, fx vz) { return fv2_len(fv2_make(fv2_len(v), vz)); }
static fx ctrl_limit(int k) { return k == WP_CK_HEADCTL ? ST[k].v_clean : fx_mul(ST[k].v_clean, T_CONTACT_NOCTRL_K); }

fx contact_reach(int kind) { return ST[kind].reach; }

static int seat_of_c(const SimState *s, int i) {
    for (int k = 0; k < WP_MAX_SEATS; k++) if (s->seat_player[k] == i) return k;
    return -1;
}

void contact_mark(SimState *s, int i, uint8_t kind) {    /* his last contact, for the poses (D-052) */
    s->players[i].last_touch_kind = kind;
    s->players[i].last_touch_tick = (uint16_t)s->tick;
}

int contact_stance(const SimState *s, int i, const WpBall *b) {
    const WpPlayer *p = &s->players[i];
    int k = control_kind(band_of(b->z));
    if (k == WP_CK_NONE) return 0;
    if (k == WP_CK_TRAP) return fv2_len(fv2_sub(b->vel, p->vel)) <= T_CONTROL_MAX_REL ? k : 0;   /* the old rule */
    return speed3(fv2_sub(b->vel, p->vel), b->vz - p->vz) <= ctrl_limit(k) ? k : 0;
}

/* ---------------- prediction ring ---------------- */
#define RN (T_CONTACT_RING + 1)
static struct { WpPitch pitch; WpBall b[RN]; int head, valid; } R;

static int same_ball(const WpBall *a, const WpBall *c) {
    return a->pos.x == c->pos.x && a->pos.y == c->pos.y && a->vel.x == c->vel.x && a->vel.y == c->vel.y &&
           a->z == c->z && a->vz == c->vz && a->spin == c->spin;
}
static int same_pitch(const WpPitch *a, const WpPitch *c) {
    return a->length == c->length && a->width == c->width && a->goal_width == c->goal_width &&
           a->goal_height == c->goal_height && a->goal_depth == c->goal_depth;
}
static const WpBall *ring_at(int t) { return &R.b[(R.head + t) % RN]; }

static void ring_update(const SimState *s) {
    const WpBall *cur = &s->ball;
    if (R.valid && same_pitch(&R.pitch, &s->pitch)) {
        if (same_ball(ring_at(0), cur)) return;
        if (same_ball(ring_at(1), cur)) {                 /* one tick on, untouched: shift, top up the horizon */
            WpBall nb = *ring_at(T_CONTACT_RING);
            ball_predict_tick(&R.pitch, &nb);
            R.head = (R.head + 1) % RN;
            R.b[(R.head + T_CONTACT_RING) % RN] = nb;
            return;
        }
    }
    R.pitch = s->pitch;
    R.head = 0;
    R.b[0] = *cur;
    for (int t = 1; t < RN; t++) { R.b[t] = R.b[t - 1]; ball_predict_tick(&R.pitch, &R.b[t]); }
    R.valid = 1;
}

/* ---------------- planner ---------------- */
static fx pressure_at(const SimState *s, int team, fv2 at) {     /* 1 within NEAR of an opponent, 0 beyond FAR */
    int64_t best = -1;
    for (int j = (1 - team) * s->n_per_side; j < (2 - team) * s->n_per_side; j++) {
        int64_t d2 = fv2_len2_wide(fv2_sub(s->players[j].pos, at));
        if (best < 0 || d2 < best) best = d2;
    }
    if (best < 0) return 0;
    int64_t n2 = (int64_t)T_CONTACT_PRESS_NEAR * T_CONTACT_PRESS_NEAR, f2 = (int64_t)T_CONTACT_PRESS_FAR * T_CONTACT_PRESS_FAR;
    if (best <= n2) return FX_ONE;
    if (best >= f2) return 0;
    return (fx)(((f2 - best) << FX_SHIFT) / (f2 - n2));            /* falls with the squared distance */
}

fx contact_pressure(const SimState *s, int team, fv2 at) { return pressure_at(s, team, at); }

static void plan_clear(WpPlayer *p) { memset(&p->plan, 0, sizeof p->plan); }

static int is_seat(const SimState *s, int i) {
    for (int k = 0; k < WP_MAX_SEATS; k++) if (s->seat_player[k] == i) return 1;
    return 0;
}

static void plan_one(SimState *s, int i) {
    WpPlayer *p = &s->players[i];
    WpPlan *pl = &p->plan;
    uint16_t now = (uint16_t)s->tick;
    if (pl->kind) {
        int left = (int16_t)(uint16_t)(pl->tick - now);
        if (left < -T_CONTACT_COMMIT) plan_clear(p);                      /* it never came: gone */
        else if (left <= T_CONTACT_COMMIT) { pl->flags |= WP_PLANF_COMMITTED; return; }   /* frozen */
    }
    if (p->z > 0 || (p->action != WP_ACT_NONE && p->action != WP_ACT_KICK)) { plan_clear(p); return; }
    fx vr = fx_mul(player_top_speed(s, i, 1), T_CONTACT_REACH_K);
    int already = pl->kind ? (int)(uint16_t)(now - pl->since) : 0;
    int volley = p->ai_intent == WP_KICK_SHOT + 1 && !is_seat(s, i);   /* the AI going to volley it: its bands only */
    int best = -1, t_first = -1, bk = 0;
    fx best_sc = 0;
    for (int t = 1; t <= T_CONTACT_RING; t++) {
        if (t_first >= 0 && t > t_first + T_CONTACT_WINDOW) break;
        const WpBall *bt = ring_at(t);
        int rolling = bt->z == 0 && bt->vz == 0;
        fx reach = T_CONTACT_PLAN_REACH + fx_mul(vr, fx_max(FX_INT(t) / WP_TICK_HZ - T_CONTACT_REACT, 0));
        if (fv2_len2_wide(fv2_sub(bt->pos, p->pos)) > (int64_t)reach * reach) { if (rolling) break; continue; }
        int k = control_kind(band_of(bt->z));
        if (k == WP_CK_NONE) continue;
        if (volley && k != WP_CK_INSTEP && k != WP_CK_THIGH) { if (rolling) break; continue; }   /* (D-051) */
        fx sp = speed3(bt->vel, bt->vz);
        if (k != WP_CK_TRAP && sp > ctrl_limit(k)) { if (rolling) break; continue; }
        if (t_first < 0) t_first = t;
        const Stance *st = &ST[k];
        fx over = fx_div(fx_max(sp - st->v_clean, 0), st->v_clean);
        fx deficit = fx_div(FX_INT(st->setup - t - already > 0 ? st->setup - t - already : 0), FX_INT(st->setup));
        fx diff = st->base + over + fx_mul(T_CONTACT_SETUP_K, deficit);
        fx sc = -fx_mul(T_CONTACT_W_DIFF, diff) - fx_mul(T_CONTACT_W_TIME, FX_INT(t - t_first) / 30)
                - fx_mul(T_CONTACT_W_PRESS, pressure_at(s, p->team, bt->pos));
        if (best < 0 || sc > best_sc) { best = t; best_sc = sc; bk = k; }
        if (rolling) break;
    }
    if (best < 0) { plan_clear(p); return; }
    const WpBall *bb = ring_at(best);
    if (!pl->kind) pl->since = now;
    pl->kind = (uint8_t)bk;
    pl->band = (uint8_t)band_of(bb->z);
    pl->flags = 0;
    pl->tick = (uint16_t)(now + best);
    pl->at = bb->pos;
    pl->z = bb->z;
}

void contact_update(SimState *s) {
    const WpBall *b = &s->ball;
    int np = 2 * s->n_per_side;
    int air = s->phase == WP_PHASE_PLAY && b->owner < 0 && !b->held && (b->z > 0 || b->vz > 0);
    if (air) ring_update(s);
    for (int i = 0; i < np; i++) {
        WpPlayer *p = &s->players[i];
        int who = air && p->role != WP_ROLE_KEEPER &&
                  (i == s->pass_target || is_seat(s, i) || s->plan[p->team].winner == i);
        if (who) plan_one(s, i);
        else if (p->plan.kind) plan_clear(p);
    }
}

/* the predicted ball t ticks ahead (0..T_CONTACT_RING), NULL = the ring isn't this ball's (D-051) */
const WpBall *contact_ring(const SimState *s, int t) {
    if (!R.valid || !same_ball(ring_at(0), &s->ball) || !same_pitch(&R.pitch, &s->pitch)) return 0;
    return ring_at(t < 0 ? 0 : t > T_CONTACT_RING ? T_CONTACT_RING : t);
}

/* the earliest ring tick (1..tmax) at which player i can reach the predicted ball with its height in [zlo, zhi],
 * by the planner's reach model; -1 = none (or the ring isn't this ball's: not a loose ball in the air). D-051. */
int contact_earliest(const SimState *s, int i, fx zlo, fx zhi, int tmax, fv2 *at, fx *z) {
    return contact_reachable(s, i, zlo, zhi, tmax, 0, at, z);
}

/* ... nearest = 1: of those reachable ticks, the one where the ball passes closest to him (ties: the earliest) -
 * a ball played to him is met where it comes to him, not raced for */
int contact_reachable(const SimState *s, int i, fx zlo, fx zhi, int tmax, int nearest, fv2 *at, fx *z) {
    if (!R.valid || !same_ball(ring_at(0), &s->ball) || !same_pitch(&R.pitch, &s->pitch)) return -1;
    const WpPlayer *p = &s->players[i];
    fx vr = fx_mul(player_top_speed(s, i, 1), T_CONTACT_REACH_K);
    int best = -1;
    int64_t bd = 0;
    if (tmax > T_CONTACT_RING) tmax = T_CONTACT_RING;
    for (int t = 1; t <= tmax; t++) {
        const WpBall *bt = ring_at(t);
        if (bt->z < zlo || bt->z > zhi) {
            if (bt->z == 0 && bt->vz == 0 && zlo > 0) break;          /* rolling now: never high again */
            if (best >= 0 && bt->z < zlo) break;                     /* (it has dropped through the window) */
            continue;
        }
        fx reach = T_CONTACT_PLAN_REACH + fx_mul(vr, fx_max(FX_INT(t) / WP_TICK_HZ - T_CONTACT_REACT, 0));
        int64_t d2 = fv2_len2_wide(fv2_sub(bt->pos, p->pos));
        if (d2 > (int64_t)reach * reach) continue;
        if (best < 0 || d2 < bd) { best = t; bd = d2; *at = bt->pos; *z = bt->z; }
        if (!nearest) break;
    }
    return best;
}

/* the difficulty of player i's planned control touch (the planner's: stance base + speed over v_clean), -1 = none */
fx contact_plan_difficulty(const SimState *s, int i) {
    const WpPlan *pl = &s->players[i].plan;
    if (!pl->kind || !R.valid || !same_ball(ring_at(0), &s->ball)) return -FX_ONE;
    int t = (int16_t)(uint16_t)(pl->tick - (uint16_t)s->tick);
    if (t < 0) t = 0;
    if (t > T_CONTACT_RING) t = T_CONTACT_RING;
    const WpBall *bt = ring_at(t);
    const Stance *st = &ST[pl->kind];
    return st->base + fx_div(fx_max(speed3(bt->vel, bt->vz) - st->v_clean, 0), st->v_clean);
}

/* where player i should stand for his planned touch: T_CONTACT_STAND_BACK beyond the contact point along the ball's
 * flight, so it drops into his chest / thigh in front of him instead of clipping his head on the way (the body
 * deflects a ball above the chest band). No plan: his position. (The AI, D-051.) */
fv2 contact_stand_at(const SimState *s, int i) {
    const WpPlayer *p = &s->players[i];
    if (!p->plan.kind) return p->pos;
    fv2 v = s->ball.vel;
    if (R.valid && same_ball(ring_at(0), &s->ball)) {
        int t = (int16_t)(uint16_t)(p->plan.tick - (uint16_t)s->tick);
        v = ring_at(t < 0 ? 0 : t > T_CONTACT_RING ? T_CONTACT_RING : t)->vel;
    }
    fx vl = fv2_len(v);
    if (vl < FX(0.5)) return p->plan.at;
    return fv2_add(p->plan.at, fv2_scale(v, fx_div(T_CONTACT_STAND_BACK, vl)));
}

int contact_waiting(const SimState *s, int i, fx dseg) {
    const WpPlayer *p = &s->players[i];
    const WpBall *b = &s->ball;
    if (!p->plan.kind || dseg <= T_CONTACT_WAIT_D) return 0;
    if ((int16_t)(uint16_t)(p->plan.tick - (uint16_t)s->tick) <= 3) return 0;
    return fv2_dot(fv2_sub(p->pos, b->pos), b->vel) > 0;            /* still coming: let it drop to the plan */
}

/* ---------------- the control touch ---------------- */
static fx fall_time(fx vz, fx dz) {            /* from launch speed vz, time to come down dz below the start */
    fx t = fx_div(vz + fx_sqrt(fx_mul(vz, vz) + fx_mul(2 * T_GRAVITY, fx_max(dz, 0))), T_GRAVITY);
    return fx_max(t, FX(0.1));
}

void contact_touch(SimState *s, int i, const WpSeatInput *in, fx dseg) {
    WpPlayer *p = &s->players[i];
    WpBall *b = &s->ball;
    int k = contact_stance(s, i, b);
    if (k == WP_CK_NONE || k == WP_CK_TRAP) k = control_kind(band_of(b->z));
    if (k == WP_CK_NONE || k == WP_CK_TRAP) k = WP_CK_HEADCTL;          /* (callers check; never a trap here) */
    const Stance *st = &ST[k];
    fv2 rh = fv2_sub(b->vel, p->vel);
    fx rz = b->vz - p->vz, rs = speed3(rh, rz);
    /* touch quality */
    int planned = p->plan.kind ? (int)(uint16_t)((uint16_t)s->tick - p->plan.since) : 0;
    fx over = fx_div(fx_max(rs - st->v_clean, 0), st->v_clean);
    fx deficit = fx_div(FX_INT(st->setup > planned ? st->setup - planned : 0), FX_INT(st->setup));
    fx stretch = dseg > T_CONTROL_REACH
               ? fx_mul(T_STRETCH_SPILL, fx_div(dseg - T_CONTROL_REACH, T_CONTROL_STRETCH - T_CONTROL_REACH)) : 0;
    fx q = FX_ONE - over - fx_mul(T_CONTACT_SETUP_K, deficit) - fx_mul(T_CONTACT_PRESS_K, pressure_at(s, p->team, p->pos)) - stretch;
    q = fx_clamp(fx_mul(q, FX_ONE + fx_mul(T_CONTACT_STAT_K, stat_k(s, i, WP_STAT_CONTROL))), 0, FX_ONE);
    /* exit: the stick now, else the stick held a moment ago (seat), else facing */
    wp_angle ex = p->facing;
    if (in->move_x || in->move_y) ex = fx_atan2((fx)in->move_y, (fx)in->move_x);
    else for (int seat = 0; seat < WP_MAX_SEATS; seat++)
        if (s->seat_player[seat] == i && s->pick_stick_age[seat] <= T_CONTACT_STICK_MEM)
            ex = fx_atan2((fx)s->pick_stick[seat][1], (fx)s->pick_stick[seat][0]);
    /* the clean cushion: pops h_pop, at the feet (READY_Z) d_ready ahead */
    fx vzi = fx_sqrt(fx_mul(2 * T_GRAVITY, st->h_pop));
    fx tr = fall_time(vzi, b->z - T_CONTACT_READY_Z);
    fv2 out = fv2_add(p->vel, fv2_from_angle(ex, fx_div(st->d_ready, tr)));
    fx outz = vzi;
    /* the residual: what he fails to absorb, mirrored about the stance's normal */
    fx res = fx_mul(FX_ONE - q, st->e_firm);
    if (res > 0) {
        int elev = k == WP_CK_CHEST && b->vz > -T_ST_DRIVEN_VZ ? T_ST_CHEST_DRIVEN_DEG : st->elev_deg;
        fx rhl = fv2_len(rh);
        fv2 nh = rhl > FX(0.05) ? fv2_scale(rh, -fx_div(FX_ONE, rhl)) : fv2_from_angle(p->facing, FX_ONE);
        wp_angle ea = (wp_angle)DEG_TO_ANG(elev);
        fx cn = fx_cos(ea), sn = fx_sin(ea);
        fv2 n = fv2_scale(nh, cn);
        fx rn = fv2_dot(rh, n) + fx_mul(rz, sn);                          /* r . n (< 0: into the body) */
        fv2 mh = fv2_sub(rh, fv2_scale(n, 2 * rn));
        fx mz = rz - fx_mul(sn, 2 * rn);
        out = fv2_add(out, fv2_scale(mh, res));
        outz += fx_mul(mz, res);
    }
    if (q < FX_ONE) {                                                    /* readable error: one draw */
        fx mag = fx_mul(FX_ONE - q, FX_INT(DEG_TO_ANG(T_CONTACT_ERR_DEG)));
        fx e = wp_rng_fx_signed(&s->rng, mag);
        fv2 rel = fv2_sub(out, p->vel);
        fx rl = fv2_len(rel);
        if (rl > 0) out = fv2_add(p->vel, fv2_from_angle((wp_angle)(fx_atan2(rel.y, rel.x) + (e >> FX_SHIFT)), rl));
    }
    b->vel = out;
    b->vz = outz;
    b->spin = 0;
    int settle = (int)(fall_time(fx_max(outz, 0), b->z - T_CONTACT_READY_Z) * WP_TICK_HZ / FX_ONE) + T_SETTLE_EXTRA;
    p->settle_ticks = (uint8_t)(settle > 255 ? 255 : settle);
    contact_mark(s, i, (uint8_t)k);                       /* (WP_LT_* = WP_CK_* for the stances) */
    if (seat_of_c(s, i) < 0) p->ai_intent = 0;           /* the AI plays on from its brain once it has it (D-051) */
    plan_clear(p);
}

/* ---------------- first-time contacts (D-048) ---------------- */
static fx len3(fv2 h, fx z) { return fv2_len(fv2_make(fv2_len(h), z)); }

static void strike_common(SimState *s, int i, uint8_t kind, wp_angle dir) {
    WpPlayer *p = &s->players[i];
    WpBall *b = &s->ball;
    b->spin = 0;
    b->owner = -1;
    b->held = 0;
    b->last_touch = (int8_t)i;
    b->no_hands = (uint8_t)(p->team + 1);
    offside_on_kick(s, i);
    p->immune = T_KICK_IMMUNE;
    p->touch_cooldown = 18;
    p->settle_ticks = 0;
    p->kick.kind = kind;
    p->kick.dir = dir;
    p->kick.foot = ang_diff(dir, p->facing) > DEG_TO_ANG(20) ? 0 : 1;
    p->kick.windup = 1;                                   /* struck on the contact tick: the pose is the follow-through */
    p->kick.done = 1;
    p->kick.to = 0;
    p->action = WP_ACT_KICK;
    p->action_timer = 1;
    plan_clear(p);
    s->events |= WP_EV_KICK;
}

/* the aim of a first-time strike: the ring's direction for passes, the stick (else facing) for shots */
static wp_angle strike_dir(const SimState *s, int i, const WpSeatInput *in, uint8_t kind, fv2 *aim, int *has) {
    const WpPlayer *p = &s->players[i];
    int seat = seat_of_c(s, i);
    *has = in->move_x || in->move_y;
    *aim = *has ? fv2_norm(fv2_make((fx)in->move_x * (FX_ONE / 127), (fx)in->move_y * (FX_ONE / 127))) : fv2_make(0, 0);
    if (kind != WP_KICK_SHOT && seat >= 0 && !*has) return s->pick_dir[seat];
    return *has ? fx_atan2(aim->y, aim->x) : p->facing;
}

void contact_strike(SimState *s, int i, const WpSeatInput *in, uint8_t kind, uint16_t gauge) {
    WpPlayer *p = &s->players[i];
    WpBall *b = &s->ball;
    int seat = seat_of_c(s, i), has;
    fv2 aim;
    wp_angle dir = strike_dir(s, i, in, kind, &aim, &has);
    fv2 rh = fv2_sub(b->vel, p->vel);
    fx rz = b->vz - p->vz, z = b->z;
    int band = band_of(z);
    if (kind == WP_KICK_SHOT) {                           /* volley: the foot law, error grows with height and pace */
        fx h = fx_clamp(fx_div(FX_INT(gauge), FX_INT(T_CHARGE_MAX_TICKS)), T_VOLLEY_MIN_H, FX_ONE);
        dir = kick_aim_assist(s, i, dir, aim, has);
        fv2 w = fv2_from_angle(dir, FX_ONE);
        fx sp = fx_min(T_SHOT_MAX, fx_mul(T_VOLLEY_K, fx_lerp(T_SHOT_MIN, T_SHOT_MAX, h)) +
                                   fx_mul(T_VOLLEY_IN_K, fx_max(-fv2_dot(rh, w), 0)));
        int k = control_kind(band);
        fx vc = k > WP_CK_TRAP ? ST[k].v_clean : ST[WP_CK_INSTEP].v_clean;
        fx over = fx_div(fx_max(len3(rh, rz) - vc, 0), vc);
        fx mag = fx_mul(FX_INT(DEG_TO_ANG(T_VOLLEY_ERR_DEG) * (band >= WP_CB_MID ? 3 : 2)), FX_ONE + over);
        fx e = wp_rng_fx_signed(&s->rng, mag);
        dir = (wp_angle)(dir + (e >> FX_SHIFT));
        b->vel = fv2_from_angle(dir, sp);
        b->vz = fx_lerp(T_SHOT_VZ_MIN, T_SHOT_VZ_MAX, fx_mul(h, h)) + fx_mul(sp, fx_mul(T_VOLLEY_RISE, fx_max(z - T_VOLLEY_RISE_Z, 0)));
        strike_common(s, i, WP_KICK_SHOT, dir);
        p->kick.power = sp;
        contact_mark(s, i, WP_LT_VOLLEY);
        return;
    }
    int r = seat >= 0 && dir == s->pick_dir[seat] ? s->pick_recv[seat]
          : seat < 0 && p->ai_intent && p->ai_mate >= 0 && s->players[p->ai_mate].team == p->team && p->ai_mate != i
          ? p->ai_mate : pass_pick(s, i, dir);                     /* (the AI: the mate it chose, D-051) */
    fv2 meet = b->pos;
    fx t_meet = FX(1.0), vz = 0;
    uint8_t plan = WP_PLAN_CLOSEST;
    if (kind == WP_KICK_LOB) {                            /* volleyed lob / clearance */
        if (r >= 0 && s->players[r].role == WP_ROLE_KEEPER) r = -1;
        fx h = gauge <= T_LOB_TAP_TICKS ? 0 : fx_max(fx_div(FX_INT(gauge - T_LOB_TAP_TICKS), FX_INT(T_CHARGE_MAX_TICKS - T_LOB_TAP_TICKS)), 1);
        b->vel = lob_solve(s, r, dir, h, 0, 0, &vz, &meet, &t_meet);
        plan = WP_PLAN_RUN;
    } else if (r >= 0) {                                  /* half-volley / volley pass: the ground pass, struck down */
        b->vel = fv2_scale(player_pass_solve(s, i, r, &meet, &t_meet, &plan), T_HALFVOLLEY_K);
        vz = T_HALFVOLLEY_VZ;
        dir = fx_atan2(b->vel.y, b->vel.x);
    } else {
        b->vel = fv2_from_angle(dir, ball_roll_speed_for(T_FREE_MIN_ROLL + T_FREE_MIN_ROLL));
        vz = T_HALFVOLLEY_VZ;
    }
    b->vz = vz;
    strike_common(s, i, kind, b->vel.x || b->vel.y ? fx_atan2(b->vel.y, b->vel.x) : dir);
    p->kick.power = 0;
    contact_mark(s, i, WP_LT_HALFVOLLEY);
    if (r >= 0) { p->kick.to = (uint8_t)(r + 1); seats_pass_handoff(s, i, r, in, meet, t_meet, plan); }
}

/* chest lay-off: to a team-mate close by in the aim's cone, at the pace the chest can give */
static int layoff(SimState *s, int i, const WpSeatInput *in) {
    WpPlayer *p = &s->players[i];
    WpBall *b = &s->ball;
    int has;
    fv2 aim;
    wp_angle dir = strike_dir(s, i, in, WP_KICK_PASS, &aim, &has);
    int r = -1;
    fx bd = 0;
    for (int j = p->team * s->n_per_side; j < (p->team + 1) * s->n_per_side; j++) {
        if (j == i || s->players[j].role == WP_ROLE_KEEPER) continue;
        fv2 d = fv2_sub(s->players[j].pos, b->pos);
        fx dl = fv2_len(d);
        if (dl > T_LAYOFF_DIST || dl < FX(1.0)) continue;
        int32_t a = ang_diff(fx_atan2(d.y, d.x), dir);
        if (a > DEG_TO_ANG(T_LAYOFF_DEG) || a < -DEG_TO_ANG(T_LAYOFF_DEG)) continue;
        if (r < 0 || dl < bd) { r = j; bd = dl; }
    }
    if (r < 0) return 0;
    fv2 rh = fv2_sub(b->vel, p->vel);
    fv2 to = fv2_sub(s->players[r].pos, b->pos);
    fx into = fx_max(-fv2_dot(rh, fv2_norm(to)), 0);
    fx sp = fx_min(fx_mul(T_LAYOFF_K, into) + T_LAYOFF_BASE, T_LAYOFF_MAX);
    fv2 P = fv2_add(s->players[r].pos, fv2_scale(s->players[r].vel, fx_div(bd, sp)));
    fv2 u = fv2_norm(fv2_sub(P, b->pos));
    b->vel = fv2_scale(u, sp);
    b->vz = 0;
    strike_common(s, i, WP_KICK_PASS, fx_atan2(u.y, u.x));
    p->kick.power = sp;
    contact_mark(s, i, WP_LT_LAYOFF);
    p->kick.to = (uint8_t)(r + 1);
    seats_pass_handoff(s, i, r, in, P, fx_div(fv2_len(fv2_sub(P, b->pos)), sp), WP_PLAN_CLOSEST);
    return 1;
}

int contact_first_time(SimState *s, int i, const WpSeatInput *in) {
    uint16_t gauge;
    int kind = intent_of(s, i, &gauge);
    if (kind < 0) return 0;
    const WpPlayer *p = &s->players[i];
    const WpBall *b = &s->ball;
    int band = band_of(b->z), seat = seat_of_c(s, i), done = 0;
    if (kind == WP_KICK_SHOT && (band == WP_CB_LOW || band == WP_CB_MID)) done = 1;
    else if (kind == WP_KICK_LOB && (band == WP_CB_LOW || band == WP_CB_MID)) done = 1;
    else if (kind == WP_KICK_PASS && band == WP_CB_LOW) done = 1;
    else if (kind == WP_KICK_PASS && band == WP_CB_MID) {   /* a volley pass only if it's easy */
        wp_angle pd = seat >= 0 ? s->pick_dir[seat] : fx_atan2(p->ai_aim.y - b->pos.y, p->ai_aim.x - b->pos.x);
        int32_t a = ang_diff(pd, p->facing);
        done = speed3(fv2_sub(b->vel, p->vel), b->vz - p->vz) <= T_VOLLEYPASS_MAX_REL &&
               a <= DEG_TO_ANG(T_VOLLEYPASS_DEG) && a >= -DEG_TO_ANG(T_VOLLEYPASS_DEG);
    }
    WpSeatInput a = intent_aim(s, i, in);
    if (done) {
        contact_strike(s, i, &a, (uint8_t)kind, gauge);
        intent_clear_player(s, i);
        return 1;
    }
    if (kind == WP_KICK_PASS && band == WP_CB_CHEST && layoff(s, i, &a)) { intent_clear_player(s, i); return 1; }
    return 0;                                             /* control it; the intent stays for the next touch */
}
