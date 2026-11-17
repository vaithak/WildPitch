/* header.c - playable contextual headers (D-010, CP0.2b). No header button: pressing PASS or SHOOT while a ball
 * is coming in above body height triggers a header. The player waits for the ball (and may still walk under it),
 * jumps at the right moment when it will be too high to reach standing, and contacts it in the HEAD band only.
 * Aim comes from the stick at the contact tick: PASS heads it to the best team-mate, SHOOT at goal (aim assist). */
#include "sim/sim_internal.h"

/* When will the ball pass closest to (x,y) and how high will it be? (flight approx: no drag) */
static void ball_approach(const SimState *s, fv2 at, fx *t_out, fx *z_out, fx *miss_out) {
    const WpBall *b = &s->ball;
    fv2 d = fv2_sub(at, b->pos);
    fx v2 = fv2_dot(b->vel, b->vel);
    fx t = v2 > FX(0.25) ? fx_clamp(fx_div(fv2_dot(d, b->vel), v2), 0, FX(2.0)) : 0;
    fx z = b->z + fx_mul(b->vz, t) - fx_mul(fx_mul(T_GRAVITY, t), t) / 2;
    *t_out = t;
    *z_out = z;
    *miss_out = fv2_len(fv2_sub(fv2_add(b->pos, fv2_scale(b->vel, t)), at));
}

int header_wanted(const SimState *s, int i, int *jump) { return header_wanted_at(s, i, jump, WP_BAND_BODY_MAX - FX(0.15)); }

/* ... with the ball at least zmin high where it reaches him (a buffered intent: the HEAD band, D-048) */
int header_wanted_at(const SimState *s, int i, int *jump, fx zmin) {
    const WpBall *b = &s->ball;
    const WpPlayer *p = &s->players[i];
    if (b->owner >= 0 || p->z > 0 || (b->z <= WP_BAND_BODY_MAX - FX(0.25) && b->vz <= 0)) return 0;
    fx t, z, miss;
    ball_approach(s, p->pos, &t, &z, &miss);
    if (t > FX(1.2) || miss > FX(2.0) || z < zmin) return 0;
    *jump = z > T_HEAD_Z + FX(0.20);
    return 1;
}

void header_start(SimState *s, int i, int shot, int jump) {
    WpPlayer *p = &s->players[i];
    p->action = jump ? WP_ACT_JUMP_HEADER : WP_ACT_STAND_HEADER;
    p->action_timer = 0;
    p->kick.kind = WP_KICK_HEADER;
    p->kick.foot = (uint8_t)shot;            /* header sub-kind: 0 = pass, 1 = shot, 2 = lofted (D-048) */
    p->kick.power = 0;                       /* 1 = aimed at a buffered press (kick.dir), D-048 */
    p->kick.done = 0;
    p->kick.windup = 0;                      /* 0 = waiting; then jump: take-off tick + 1, stand: contact tick + 1
                                                (the avatar's header pose is timed from it) */
    p->kick.to = 0;
    p->kick_charge = 0;
    p->charge_kind = 0xFF;
}

/* per tick while a header attempt is live: take off on time, then look for head contact */
void header_step(SimState *s, int i) {
    WpPlayer *p = &s->players[i];
    WpBall *b = &s->ball;
    if (p->kick.kind != WP_KICK_HEADER || p->kick.done) return;
    if (p->action != WP_ACT_JUMP_HEADER && p->action != WP_ACT_STAND_HEADER) return;
    fx t, z, miss;
    ball_approach(s, p->pos, &t, &z, &miss);
    if (p->action == WP_ACT_JUMP_HEADER && !p->kick.windup) {
        fv2 rel = fv2_sub(p->pos, b->pos);
        if (b->owner >= 0 || (fv2_dot(rel, b->vel) < 0 && fv2_len(rel) > FX(1.0))) {   /* passed before take-off */
            p->action = WP_ACT_NONE; p->action_timer = 0; p->kick.done = 1;
            return;
        }
        /* take off so the apex (~0.35 s) meets the ball; drift toward where it will be */
        if (t <= fx_div(WP_JUMP_VZ, T_GRAVITY) || p->action_timer > 45) {
            p->vz = WP_JUMP_VZ;
            fv2 meet = fv2_add(b->pos, fv2_scale(b->vel, t));
            p->vel = fv2_clamp_len(fv2_scale(fv2_sub(meet, p->pos), fx_div(FX_ONE, fx_max(t, FX(0.2)))), FX(3.5));
            p->kick.windup = (uint8_t)(p->action_timer + 1);
        }
        return;
    }
    fv2 head = fv2_add(p->pos, fv2_from_angle(p->facing, T_HEAD_FWD));
    /* missed: the ball has gone past (or someone has it) -> give up now, body collision back, free soon */
    fv2 to_head = fv2_sub(head, b->pos);
    if (b->owner >= 0 || (fv2_dot(to_head, b->vel) < 0 && fv2_len(to_head) > T_HEAD_REACH_XY)) {
        p->kick.done = 1;
        if (p->action == WP_ACT_STAND_HEADER) p->kick.windup = (uint8_t)(p->action_timer + 1 > 255 ? 255 : p->action_timer + 1);
        return;
    }
}

/* this tick, can player i head the ball? returns the head-ball distance (contest: the nearest head wins), or -1 */
fx header_contact_dist(const SimState *s, int i) {
    const WpPlayer *p = &s->players[i];
    const WpBall *b = &s->ball;
    if (p->kick.kind != WP_KICK_HEADER || p->kick.done || b->owner >= 0) return -1;
    if (p->action != WP_ACT_JUMP_HEADER && p->action != WP_ACT_STAND_HEADER) return -1;
    if (p->action == WP_ACT_JUMP_HEADER && !p->kick.windup) return -1;
    fv2 head = fv2_add(p->pos, fv2_from_angle(p->facing, T_HEAD_FWD));
    fx hz = p->z + T_HEAD_Z;
    fx d = fv2_len(fv2_sub(b->pos, head));
    if (fx_abs(b->z - hz) > T_HEAD_REACH_Z || d > T_HEAD_REACH_XY) return -1;
    return d + fx_abs(b->z - hz);
}

/* header pass solve (D-048): to team-mate `to` led onto his run (or `at`), nodded down to his feet when close, else
 * vz = 1.0 + 0.15 d (lofted: the lob formula), coming down through T_HEADPASS_RECV_Z at the point */
static fv2 headpass_solve(const SimState *s, int to, fv2 at, int lofted, fx *vz, fv2 *P, fx *t) {
    const WpBall *b = &s->ball;
    fx hl = s->pitch.length / 2 - T_THROUGH_MARGIN_X, hw = s->pitch.width / 2 - T_THROUGH_MARGIN_Y, tt = FX(0.6), vh = 0;
    fv2 pt = at;
    for (int it = 0; it < 3; it++) {
        if (to >= 0) pt = fv2_add(s->players[to].pos, fv2_scale(s->players[to].vel, tt));
        pt = fv2_make(fx_clamp(pt.x, -hl, hl), fx_clamp(pt.y, -hw, hw));
        fx d = fv2_len(fv2_sub(pt, b->pos));
        *vz = lofted ? T_LOB_VZ_BASE + fx_mul(T_LOB_VZ_PER_M, d) : d <= T_HEADPASS_NOD_D ? T_HEADPASS_NOD_VZ : T_HEADPASS_VZ0 + fx_mul(T_HEADPASS_VZ_K, d);
        fx lo = 0, hi = FX(30.0), dist;
        int ticks = 60;
        for (int k = 0; k < 16; k++) {                 /* the horizontal speed that gets it there */
            fx mid = (lo + hi) / 2;
            ball_flight_to(mid, *vz, b->z, T_HEADPASS_RECV_Z, &dist, &ticks);
            if (dist < d) lo = mid; else hi = mid;
        }
        vh = hi;
        ball_flight_to(vh, *vz, b->z, T_HEADPASS_RECV_Z, &dist, &ticks);
        tt = FX_INT(ticks) / WP_TICK_HZ;
    }
    *P = pt;
    *t = tt;
    return fv2_scale(fv2_norm(fv2_sub(pt, b->pos)), vh);
}

static fx len3(fv2 h, fx z) { return fv2_len(fv2_make(fv2_len(h), z)); }

/* the head's speed budget for sending the ball along (wh, wz) off an incoming relative velocity (rh, rz) */
static fx head_budget(fv2 rh, fx rz, fv2 wh, fx wz, fx vhead) {
    fx req = len3(wh, wz), rs = len3(rh, rz);
    if (req <= 0 || rs <= FX(0.1)) return fx_mul(T_HEAD_LAW_V, vhead);
    fx wx = fx_div(wh.x, req), wy = fx_div(wh.y, req), wzz = fx_div(wz, req);
    fx ux = fx_div(rh.x, rs), uy = fx_div(rh.y, rs), uz = fx_div(rz, rs);
    fx nx = wx - ux, ny = wy - uy, nz = wzz - uz, nl = len3(fv2_make(nx, ny), nz);
    if (nl <= FX(0.01)) return fx_mul(T_HEAD_LAW_V, vhead);
    nx = fx_div(nx, nl); ny = fx_div(ny, nl); nz = fx_div(nz, nl);
    fx rn = fx_mul(rh.x, nx) + fx_mul(rh.y, ny) + fx_mul(rz, nz);           /* < 0: into the head */
    fx tx = rh.x - fx_mul(rn, nx), ty = rh.y - fx_mul(rn, ny), tz = rz - fx_mul(rn, nz);
    fx a = fx_mul(T_HEAD_LAW_U, fx_max(-rn, 0)) + fx_mul(T_HEAD_LAW_V, vhead);
    fx c = fx_mul(T_HEAD_LAW_T, len3(fv2_make(tx, ty), tz));
    return len3(fv2_make(a, c), 0);
}

/* head contact by player i: direction from `in`'s stick (the human's, or the AI's aim). The ball's response follows the
 * measured header law (D-048): the wanted ball (a header-pass solve, or the shot) is capped by the head's budget
 * sqrt((0.63 u_n + 1.63 v_head)^2 + (0.85 |r_t|)^2) about the bisector normal (too far = it falls short); turned
 * beyond 135 deg it is a flick-on; a readable aim error grows with the turn and the pace. */
void header_contact(SimState *s, int i, const WpSeatInput *in) {
    WpPlayer *p = &s->players[i];
    WpBall *b = &s->ball;
    int has;
    fv2 aim = fv2_make((fx)in->move_x * (FX_ONE / 127), (fx)in->move_y * (FX_ONE / 127));
    has = in->move_x != 0 || in->move_y != 0;
    wp_angle dir = has ? fx_atan2(aim.y, aim.x) : p->kick.power ? p->kick.dir : p->facing;   /* (a buffered aim) */
    if (!has && p->kick.power) { aim = fv2_from_angle(dir, FX_ONE); has = 1; }
    fv2 rh = fv2_sub(b->vel, p->vel);
    fx rz = b->vz - p->vz, rs = len3(rh, rz);
    fx in_speed = ball_speed3(b);
    int to = -1;
    fv2 wh, P = b->pos;
    fx wz, t = FX(1.0);
    if (p->kick.foot == 1) {                  /* header at goal: aim assist, firm and downward */
        int human = 0;
        for (int k = 0; k < WP_MAX_SEATS; k++) human |= s->seat_player[k] == i;
        if (human) dir = kick_aim_assist(s, i, dir, aim, has);   /* (the AI aims where it chose, D-051) */
        wh = fv2_from_angle(dir, fx_min(T_HEADER_SHOT + fx_mul(in_speed, FX(0.25)), T_HEADER_SHOT_MAX));
        wz = T_HEADER_SHOT_VZ;
    } else {                                  /* header pass (lofted: kick.foot 2): solved onto the mate's run */
        to = pass_pick(s, i, dir);
        wh = headpass_solve(s, to, fv2_add(b->pos, fv2_from_angle(dir, T_HEADER_PASS_NOBODY_D)), p->kick.foot == 2, &wz, &P, &t);
        if (wh.x || wh.y) dir = fx_atan2(wh.y, wh.x);
    }
    /* the head's budget; a pass the head can't reach: the best-aimed team-mate it can (else it falls short) */
    fx vhead = p->action == WP_ACT_JUMP_HEADER ? T_HEAD_V_JUMP : T_HEAD_V_STAND;
    fx smax = head_budget(rh, rz, wh, wz, vhead);
    if (p->kick.foot != 1 && len3(wh, wz) > smax) {
        int best = -1;
        int32_t ba = 0;
        for (int j = p->team * s->n_per_side; j < (p->team + 1) * s->n_per_side; j++) {
            if (j == i || j == to || s->players[j].role == WP_ROLE_KEEPER) continue;
            const WpPlayer *q = &s->players[j];
            if (fx_abs(q->pos.x) > s->pitch.length / 2 || fx_abs(q->pos.y) > s->pitch.width / 2 ||
                fv2_len(fv2_sub(q->pos, b->pos)) > T_PASS_MAX_DIST) continue;
            fx vz2, t2;
            fv2 P2, w2 = headpass_solve(s, j, b->pos, p->kick.foot == 2, &vz2, &P2, &t2);
            if (len3(w2, vz2) > head_budget(rh, rz, w2, vz2, vhead)) continue;
            int32_t a = ang_diff(fx_atan2(w2.y, w2.x), dir);
            if (a < 0) a = -a;
            if (a > DEG_TO_ANG(90)) continue;
            if (best < 0 || a < ba) { best = j; ba = a; wh = w2; wz = vz2; P = P2; t = t2; }
        }
        if (best >= 0) { to = best; dir = fx_atan2(wh.y, wh.x); smax = head_budget(rh, rz, wh, wz, vhead); }
    }
    fx req = len3(wh, wz);
    if (req > smax) { fx k = fx_div(smax, req); wh = fv2_scale(wh, k); wz = fx_mul(wz, k); }
    /* turned away from where it came: phi (0 = straight back) */
    fx rhl = fv2_len(rh);
    int32_t phi = 0;
    if (rhl > FX(0.5)) { phi = ang_diff(dir, fx_atan2(-rh.y, -rh.x)); if (phi < 0) phi = -phi; }
    if (phi >= DEG_TO_ANG(T_FLICK_DEG)) {     /* flick-on: keeps most of the pace, bent at most T_FLICK_MAX_DEG */
        wp_angle inc = fx_atan2(rh.y, rh.x);
        int32_t bend = ang_diff(dir, inc), lim = DEG_TO_ANG(T_FLICK_MAX_DEG);
        bend = bend > lim ? lim : bend < -lim ? -lim : bend;
        dir = (wp_angle)(inc + bend);
        wh = fv2_from_angle(dir, fx_clamp(fv2_len(wh), fx_mul(rhl, T_FLICK_MIN_K), fx_mul(rhl, T_FLICK_K)));
    }
    fx over = fx_div(fx_max(rs - T_HEADER_CLEAN_V, 0), T_HEADER_CLEAN_V);
    fx sig = fx_mul(FX_INT(DEG_TO_ANG(T_HEADER_ERR_DEG)), fx_mul(FX_ONE + fx_div(FX_INT(phi), FX_INT(DEG_TO_ANG(90))), FX_ONE + over));
    if (phi > DEG_TO_ANG(90) && phi < DEG_TO_ANG(T_FLICK_DEG)) sig *= 3;
    /* the readable error (owner): on every header, human or AI (D-051: the AI's header shots too) */
    fx e = wp_rng_fx_signed(&s->rng, sig);
    fx sp = fv2_len(wh);
    dir = (wp_angle)(fx_atan2(wh.y, wh.x) + (e >> FX_SHIFT));
    b->vel = fv2_from_angle(dir, sp);
    b->vz = wz;
    b->spin = 0;
    b->owner = -1;
    b->last_touch = (int8_t)i;
    b->no_hands = 0;                                      /* headers back to the keeper are allowed */
    offside_on_kick(s, i);
    p->immune = T_KICK_IMMUNE;
    p->touch_cooldown = 18;
    p->kick.dir = dir;
    p->kick.done = 1;
    if (p->action == WP_ACT_STAND_HEADER) p->kick.windup = (uint8_t)(p->action_timer + 1 > 255 ? 255 : p->action_timer + 1);
    s->events |= WP_EV_KICK | WP_EV_HEADER;
    contact_mark(s, i, WP_LT_HEADER);
    if (to >= 0) seats_pass_handoff(s, i, to, in, P, t, WP_PLAN_RUN);
}

/* While a header attempt waits for the ball, the stick only AIMS; the legs step under the ball's path. */
void header_assist_move(const SimState *s, int i, WpSeatInput *io) {
    const WpPlayer *p = &s->players[i];
    if (p->kick.kind != WP_KICK_HEADER || p->kick.done || p->z > 0) return;
    if (p->action != WP_ACT_JUMP_HEADER && p->action != WP_ACT_STAND_HEADER) return;
    fx t, z, miss;
    ball_approach(s, p->pos, &t, &z, &miss);
    fv2 meet = fv2_sub(fv2_add(s->ball.pos, fv2_scale(s->ball.vel, t)), fv2_from_angle(p->facing, T_HEAD_FWD));
    uint16_t buttons = io->buttons;
    team_steer(p, meet, FX(0.08), FX(0.8), io);
    io->buttons = buttons;
}

/* action end rule for header actions (called from air_and_timers) */
int header_action_ends(const WpPlayer *p) {
    if (p->action == WP_ACT_JUMP_HEADER) {
        int took_off = p->kick.kind != WP_KICK_HEADER || p->kick.windup;
        return (took_off && p->z == 0 && p->vz == 0 && p->action_timer > 4) || p->action_timer > T_HEADER_WAIT_MAX + 45;
    }
    if (p->kick.kind == WP_KICK_HEADER && p->kick.done) return p->action_timer > (uint16_t)p->kick.windup + 12;
    return p->action_timer > T_HEADER_WAIT_MAX;
}
