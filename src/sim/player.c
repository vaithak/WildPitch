/* player.c - player movement, turning, buttons -> parametric kicks (D-012), ball control & dribbling.
 *
 * Feel rules (see tests/test_ball_control.c):
 *  - Dribble touches follow the STICK direction, not the slower-turning body, so cuts feel immediate.
 *  - An owned ball is "gathered" during a kick wind-up: a kick by the owner can never whiff. The foot only drags
 *    it a limited speed and never through the body (contact point = ball side, turned toward the kick by
 *    <= T_GATHER_MAX_DEG); an awkward contact (ball off the spot, kicked across the body) gives a weaker kick.
 *  - Releasing the stick: the player steps up to the ball if it is out of reach, then sole-traps it (reach-limited,
 *    capped, on the ball's own side). No remote braking, no reeling back, never through the body.
 *  - Inaccuracy comes from visible causes only (no dice): the ball inherits part of the kicker's own
 *    velocity, and harder strikes rise more.
 *  - Ball contact uses height bands (WP_BAND_*): a ball above body height can't be controlled. */
#include "sim/sim_internal.h"

#define KICK_REACH       FX(1.15)   /* a loose ball this close (feet band) can be struck */
#define KICK_START_REACH FX(1.6)    /* releasing a charged kick only starts the action if the ball is this close */
#define FOOT_SPOT        FX(0.45)   /* where the ball sits at contact, ahead of the player centre */
#define INHERIT_VEL      FX(0.5)    /* fraction of the kicker's velocity the ball inherits */

static int seat_of(const SimState *s, int i) {
    for (int k = 0; k < WP_MAX_SEATS; k++) if (s->seat_player[k] == i) return k;
    return -1;
}

static fv2 stick_dir(const WpSeatInput *in, int *has) {
    fv2 v = fv2_make((fx)in->move_x * (FX_ONE / 127), (fx)in->move_y * (FX_ONE / 127));
    *has = in->move_x != 0 || in->move_y != 0;
    return *has ? fv2_norm(v) : v;
}

static int ball_reachable(const SimState *s, int i, fx reach) {
    const WpBall *b = &s->ball;
    if (b->owner == i) return 1;
    if (b->owner >= 0) return 0;                          /* someone else's ball: tackle it, don't kick it */
    if (b->z > WP_BAND_FEET_MAX) return 0;
    fv2 d = fv2_sub(b->pos, s->players[i].pos);
    return fv2_len(d) <= reach;
}

/* Where the foot meets the ball during a kick: on the ball's side of the body, turned toward the kick direction
 * by at most T_GATHER_MAX_DEG (so a gathered ball never crosses the body). Recomputed each tick: no state. */
static wp_angle contact_dir(const SimState *s, int i) {
    const WpPlayer *p = &s->players[i];
    fv2 d = fv2_sub(s->ball.pos, p->pos);
    if (fx_abs(d.x) + fx_abs(d.y) < FX(0.05)) return p->kick.dir;
    wp_angle a = fx_atan2(d.y, d.x);
    int32_t diff = ang_diff(p->kick.dir, a), lim = DEG_TO_ANG(T_GATHER_MAX_DEG);
    if (diff > lim) diff = lim;
    if (diff < -lim) diff = -lim;
    return (wp_angle)(a + diff);
}

void sim_player_kick(SimState *s, int i, const WpKick *k) {
    WpPlayer *p = &s->players[i];
    if (p->action != WP_ACT_NONE || p->z > 0) return;
    p->kick = *k;
    if (p->kick.windup < 1) p->kick.windup = 1;
    p->kick.done = 0;
    p->action = WP_ACT_KICK;
    p->action_timer = 0;
}


/* Aim assist (D-013): roughly toward goal, the stick picks a TARGET rather than an exact angle — up/down = that
 * side of the goal, straight/none = the far post from where you stand. A share of the stick's angular error
 * remains so aiming still matters. Shared by shots, chips and headers. */
wp_angle kick_aim_assist(const SimState *s, int i, wp_angle dir, fv2 aim, int has_aim) {
    const WpPlayer *p = &s->players[i];
    int sg = team_dir(s, p->team);
    fx margin = fx_mul(T_AIM_POST_MARGIN, FX_ONE - fx_mul(T_STAT_SHOT_MARGIN, stat_k(s, i, WP_STAT_SHOOTING)));   /* D-034 */
    fx side = fx_max(fx_mul(s->pitch.goal_width, T_AIM_SIDE), s->pitch.goal_width / 2 - margin);
    fx gy;
    if (has_aim && aim.y > FX(0.25)) gy = side;
    else if (has_aim && aim.y < -FX(0.25)) gy = -side;
    else gy = p->pos.y > FX(0.5) ? -side : side;
    fv2 to_goal = fv2_sub(fv2_make(sg * (s->pitch.length / 2), gy), p->pos);
    wp_angle ga = fx_atan2(to_goal.y, to_goal.x);
    int32_t diff = ang_diff(dir, ga);
    if (diff > -DEG_TO_ANG(T_AIM_WINDOW_DEG) && diff < DEG_TO_ANG(T_AIM_WINDOW_DEG))
        dir = (wp_angle)(ga + (int32_t)(((int64_t)diff * T_AIM_RESIDUAL) >> FX_SHIFT));
    return dir;
}

/* Build kick parameters from a human's input. charge01 in [0,1]. */
/* receiver for a pass kind: the previewed pick (what the ring shows) - the keeper never for through balls/lobs */
static int kick_receiver(const SimState *s, int i, int seat, uint8_t kind, wp_angle dir) {
    int r = seat >= 0 ? s->pick_recv[seat] : pass_pick(s, i, dir);
    if (r >= 0 && kind != WP_KICK_PASS && s->players[r].role == WP_ROLE_KEEPER)
        r = pass_pick_ex(s, i, dir, PICK_DIGITAL, -1, 0);
    return r;
}

static void make_kick(const SimState *s, int i, uint8_t kind, fx charge01, fv2 aim, int has_aim, WpKick *k) {
    const WpPlayer *p = &s->players[i];
    wp_angle dir = has_aim ? fx_atan2(aim.y, aim.x) : p->facing;
    int seat = seat_of(s, i);
    if ((kind == WP_KICK_PASS || kind == WP_KICK_THROUGH || kind == WP_KICK_LOB || kind == WP_KICK_THROW ||
         kind == WP_KICK_PUNT) && seat >= 0)
        dir = s->pick_dir[seat];                          /* stick, latched diagonal, memory, facing (D-021) */
    k->kind = kind;
    k->spin = 0;
    k->to = 0;
    if (kind == WP_KICK_PASS) {
        k->power = T_PASS_SPEED; k->vz = 0; k->windup = T_KICK_WINDUP_PASS;
        int r = kick_receiver(s, i, seat, kind, dir);
        if (r >= 0) {                                      /* targeted: aimed & weighted again at contact */
            fv2 d = fv2_sub(s->players[r].pos, p->pos);
            dir = fx_atan2(d.y, d.x);
            k->to = (uint8_t)(r + 1);
        }
    } else if (kind == WP_KICK_THROW || kind == WP_KICK_PUNT) {
        k->power = kind == WP_KICK_THROW ? T_THROW_SPEED : T_PUNT_SPEED;
        k->vz = kind == WP_KICK_PUNT ? T_PUNT_VZ : 0;
        k->windup = kind == WP_KICK_THROW ? T_KICK_WINDUP_CHIP : 8;
        if (kind == WP_KICK_THROW) {
            int r = kick_receiver(s, i, seat, WP_KICK_PASS, dir);
            if (r >= 0 && s->players[r].role != WP_ROLE_KEEPER) {
                fv2 d = fv2_sub(s->players[r].pos, p->pos);
                dir = fx_atan2(d.y, d.x);
                k->to = (uint8_t)(r + 1);
            }
        }
    } else if (kind == WP_KICK_THROUGH || kind == WP_KICK_LOB) {
        /* power field = the hold h (0..1): through ball = how deep/soft (D-020); lob: unused */
        k->power = charge01;
        k->vz = 0;
        k->windup = kind == WP_KICK_LOB ? T_KICK_WINDUP_CHIP : T_KICK_WINDUP_PASS;
        int r = kick_receiver(s, i, seat, kind, dir);
        if (r >= 0) {
            fv2 d = fv2_sub(s->players[r].pos, p->pos);
            dir = fx_atan2(d.y, d.x);
            k->to = (uint8_t)(r + 1);
        }
    } else if (kind == WP_KICK_SHOT) {
        k->power = fx_lerp(T_SHOT_MIN, T_SHOT_MAX, charge01);
        k->vz = fx_lerp(T_SHOT_VZ_MIN, T_SHOT_VZ_MAX, fx_mul(charge01, charge01));
        k->windup = T_KICK_WINDUP_SHOT;
    } else {
        k->power = fx_lerp(T_CHIP_MIN, T_CHIP_MAX, charge01);
        k->vz = fx_lerp(T_CHIP_VZ_MIN, T_CHIP_VZ_MAX, charge01);
        k->windup = T_KICK_WINDUP_CHIP;
    }
    if (kind == WP_KICK_SHOT || kind == WP_KICK_CHIP) {
        dir = kick_aim_assist(s, i, dir, aim, has_aim);
        /* curl: striking across the direction of running bends the ball the way you turned */
        fx spd = fv2_len(p->vel);
        if (spd > FX(2.0)) {
            int32_t across = ang_diff(dir, fx_atan2(p->vel.y, p->vel.x));
            if (across > DEG_TO_ANG(60)) across = DEG_TO_ANG(60);
            if (across < -DEG_TO_ANG(60)) across = -DEG_TO_ANG(60);
            k->spin = (fx)(across * 18);   /* 60 deg across the run -> spin 3.0 (~1 m of bend on a shot) */
        }
    }
    k->dir = dir;
    k->foot = ang_diff(dir, p->facing) > DEG_TO_ANG(20) ? 0 : 1;   /* far left -> left foot, else right */
}

/* ---- pass solving (D-020): every pass solves WHERE and WHEN ball and receiver meet, using the exact roll tables
 * (R = roll distance, T = time to stop): a launch v0 that covers d and arrives at va takes T(v0) - T(va). ---- */

/* tap pass arrival pace grows with distance: 5.5 m/s at 5 m, +0.25 per metre, max 9 */
static fx pass_arrive(fx d, fx pace) {                 /* pace: the passer's weight (1 = the old pass, D-034) */
    return fx_mul(fx_clamp(T_PASS_ARR_MIN + fx_mul(T_PASS_ARR_PER_M, d - FX(5.0)), T_PASS_ARR_MIN, T_PASS_ARR_MAX), pace);
}

/* launch speed to cover d arriving at va (capped at vmax: then it arrives slower); *f = flight time */
static fx launch_for(fx d, fx va, fx vmax, fx *f) {
    fx v0 = ball_roll_speed_for(d + ball_roll_distance(va));
    if (v0 > vmax) { v0 = vmax; va = ball_roll_speed_after(v0, d); }
    *f = ball_roll_time(v0) - ball_roll_time(va);
    return v0;
}

static fv2 clamp_box(const SimState *s, fv2 p, fx mx, fx my) {
    fx hl = s->pitch.length / 2 - mx, hw = s->pitch.width / 2 - my;
    return fv2_make(fx_clamp(p.x, -hl, hl), fx_clamp(p.y, -hw, hw));
}

/* Targeted ground (tap) pass: lead the receiver by the REAL flight time (3 fixed-point iterations), pace by
 * distance, no inherited run velocity. A runner (moving away from the ball > 2 m/s) keeps his line and runs through
 * the meeting point; anyone else steps to the closest reachable point. */
static fv2 pass_solve(const SimState *s, int r, fx pace, fv2 *meet, fx *t_meet, uint8_t *plan) {
    const WpPlayer *q = &s->players[r];
    fv2 b = s->ball.pos, aim = q->pos;
    fx f = 0, v0 = 0;
    for (int it = 0; it < 3; it++) {
        fx d = fv2_len(fv2_sub(aim, b));
        v0 = launch_for(d, pass_arrive(d, pace), T_PASS_MAX, &f);
        aim = clamp_box(s, fv2_add(q->pos, fv2_scale(q->vel, f)), FX(1.0), FX(1.0));
    }
    fx d = fv2_len(fv2_sub(aim, b));
    v0 = launch_for(d, pass_arrive(d, pace), T_PASS_MAX, &f);
    fx away = fv2_dot(q->vel, fv2_norm(fv2_sub(q->pos, b)));
    *meet = aim;
    *t_meet = f;
    *plan = away > T_PASS_RUNNER_MS ? WP_PLAN_RUN : WP_PLAN_CLOSEST;
    return fv2_scale(fv2_norm(fv2_sub(aim, b)), v0);
}

/* Through ball: find the first time t at which the ball, launched now, reaches the runner's predicted path point
 * P(t) = pos + u*(s*t + stride) no later than he does. Arrival pace sits in a band tied to his speed:
 * [max(5, s-1.5), min(1.5 s, 12)] m/s; the hold h picks inside it (h=0 firm & early, h=1 soft & deep). Constraints:
 * launch <= 24 m/s, meeting point >= 3 m inside the goal line and 1.5 m inside the touchline, never softer than a
 * tap to the same man. No solution: firmest pace, then plan the runner 25% slower (>= 5 m/s), then a lead pass
 * clamped inside the margins (he eases to collect it). */
static fv2 through_solve(const SimState *s, int r, fx h, fx pace, fv2 *meet, fx *t_meet, uint8_t *plan) {
    const WpPlayer *q = &s->players[r];
    fv2 b = s->ball.pos;
    fx spd = fv2_len(q->vel), sp, delay = 0, hl = s->pitch.length / 2, hw = s->pitch.width / 2;
    fv2 u, attack = fv2_make(FX_INT(team_dir(s, q->team)), 0);
    /* a through ball goes into space toward goal: a runner drifting back/sideways (> ~70 deg off the attacking
     * direction) is planned as setting off on a fresh forward run instead */
    if (spd > FX(1.5) && fv2_dot(q->vel, attack) > fx_mul(spd, FX(0.35))) { u = fv2_scale(q->vel, fx_div(FX_ONE, spd)); sp = spd; }
    else {
        u = fv2_scale(attack, FX(2.0));
        if (spd > FX(0.2) && fv2_dot(q->vel, attack) > 0) u = fv2_add(u, fv2_norm(q->vel));
        u = fv2_norm(u);
        sp = T_THROUGH_STAND_V;
        delay = T_THROUGH_STAND_DELAY;
    }
    fv2 tap_meet; fx tap_t; uint8_t tap_plan;
    fx tap_v0 = fv2_len(pass_solve(s, r, pace, &tap_meet, &tap_t, &tap_plan));
    /* CP0.3e (owner: "the meter's power should always be more than a normal pass"): every candidate meeting point
     * P(t) on his run is met IN STRIDE (the ball gets there exactly when he does - solved per t), and is only allowed
     * if that ball is struck at least T_THROUGH_MIN_K x a tap to him and arrives at a controllable pace (va band).
     * Of the allowed window [t_a, t_b] the hold picks the depth: h = 0 the earliest/firmest, h = 1 the deepest. */
    fx v_min = fx_mul(tap_v0, T_THROUGH_MIN_K);
    for (int attempt = 0; attempt < 3; attempt++) {
        fx s_plan = attempt == 2 ? fx_max(FX(5.0), fx_mul(sp, FX(0.75))) : sp;
        fx va_lo = fx_max(FX(5.0), s_plan - FX(1.5)), va_hi = fx_max(fx_min(fx_mul(s_plan, FX(1.5)), FX(12.0)), va_lo);
        int ka = -1, kb = -1;
        fv2 Pk[81]; fx vk[81] = {0};
        for (int k = 1; k <= 80; k++) {
            fx t = FX(0.05) * k;
            fx run = fx_max(fx_mul(s_plan, t - delay), 0);
            fv2 P = fv2_add(q->pos, fv2_scale(u, run + T_THROUGH_STRIDE));
            if (fx_abs(P.x) > hl - T_THROUGH_MARGIN_X || fx_abs(P.y) > hw - T_THROUGH_MARGIN_Y) break;
            fx d = fv2_len(fv2_sub(P, b));
            /* launch speed covering d in exactly t: travel time falls as v0 rises - bisect */
            fx lo = ball_roll_speed_for(d), hi = T_THROUGH_MAX;
            if (lo >= hi) continue;
            for (int it = 0; it < 18; it++) {
                fx mid = (lo + hi) / 2;
                fx tt = ball_roll_time(mid) - ball_roll_time(ball_roll_speed_for(ball_roll_distance(mid) - d));
                if (tt > t) lo = mid; else hi = mid;
            }
            fx v0 = hi, va = ball_roll_speed_for(ball_roll_distance(v0) - d);
            if (v0 > T_THROUGH_MAX || v0 < v_min || va < va_lo || va > va_hi) continue;
            if (ka < 0) ka = k;
            kb = k;
            Pk[k] = P; vk[k] = v0;
        }
        if (ka > 0) {
            int k = ka + (int)(fx_mul(FX_INT(kb - ka), h) / FX_ONE);
            while (k > ka && vk[k] == 0) k--;            /* a gap in the window: the nearest allowed point */
            if (vk[k] == 0) k = ka;
            *meet = fv2_sub(Pk[k], fv2_scale(u, T_THROUGH_STRIDE));
            *t_meet = FX(0.05) * k;
            *plan = attempt < 2 ? WP_PLAN_STRIDE : WP_PLAN_RUN;
            return fv2_scale(fv2_norm(fv2_sub(Pk[k], b)), vk[k]);
        }
    }
    /* short room: lead pass clamped inside the margins, runner eases to collect it */
    fv2 aim = clamp_box(s, tap_meet, T_THROUGH_MARGIN_X, T_THROUGH_MARGIN_Y);
    fx d = fv2_len(fv2_sub(aim, b)), f;
    fx v0 = fx_max(launch_for(d, pass_arrive(d, pace), T_THROUGH_MAX, &f), fx_mul(tap_v0, T_THROUGH_MIN_K));
    *meet = aim;
    *t_meet = f;
    *plan = WP_PLAN_RUN;
    return fv2_scale(fv2_norm(fv2_sub(aim, b)), v0);
}

/* Lofted pass: dropped on the receiver (where he'll be at landing), higher for longer balls. */
static fv2 lob_velocity(const SimState *s, int r, wp_angle dir, fx *vz) {
    fv2 target;
    if (r >= 0) {
        const WpPlayer *q = &s->players[r];
        target = fv2_add(q->pos, fv2_scale(q->vel, FX(0.8)));
    } else {
        target = fv2_add(s->ball.pos, fv2_from_angle(dir, FX(18.0)));
    }
    fv2 d = fv2_sub(target, s->ball.pos);
    fx dist = fv2_len(d);
    *vz = fx_clamp(T_LOB_VZ_BASE + fx_mul(T_LOB_VZ_PER_M, dist), T_LOB_VZ_MIN, T_LOB_VZ_MAX);
    return fv2_scale(fv2_norm(d), ball_lob_speed_for(dist, *vz));
}

static void contact(SimState *s, int i, const WpSeatInput *in) {
    WpPlayer *p = &s->players[i];
    WpBall *b = &s->ball;
    p->kick.done = 1;
    if (s->phase != WP_PHASE_PLAY || !ball_reachable(s, i, KICK_REACH)) return;   /* whiff */
    /* no teleport to the foot: the wind-up gather already brought the ball there, and moving it here could
     * skip it across a line without ball.c seeing the crossing */
    /* contact quality (visible cause, no dice): striking across the body and the ball being off the ideal spot
     * each cost up to 25% of the power */
    wp_angle cd = contact_dir(s, i);
    fx across = FX_INT(ang_diff(p->kick.dir, cd) < 0 ? -ang_diff(p->kick.dir, cd) : ang_diff(p->kick.dir, cd)) / 32768;
    fv2 spot = fv2_add(p->pos, fv2_from_angle(cd, FOOT_SPOT));
    fx off = fx_min(fx_div(fv2_len(fv2_sub(b->pos, spot)), KICK_REACH), FX_ONE);
    fx q = fx_max(FX_ONE - across / 4 - off / 4, FX(0.75));
    fx pace = FX_ONE + fx_mul(T_STAT_PASS_PACE, stat_k(s, i, WP_STAT_PASSING));     /* D-034: 1 when neutral */
    fx shot_q = fx_mul(q, FX_ONE + fx_mul(T_STAT_SHOT_POWER, stat_k(s, i, WP_STAT_SHOOTING)));
    int from_hands = b->held;
    if (from_hands) { b->held = 0; b->held_ticks = 0; }
    fx vz = p->kick.vz, t_meet = FX(1.5);
    int r = (int)p->kick.to - 1;
    fv2 meet = r >= 0 ? s->players[r].pos : b->pos;
    uint8_t plan = WP_PLAN_CLOSEST;
    if (p->kick.kind == WP_KICK_THROW && p->role == WP_ROLE_KEEPER && from_hands) {   /* keeper's overarm throw */
        b->vel = lob_velocity(s, r, p->kick.dir, &vz);
    }
    else if (p->kick.kind == WP_KICK_THROW) {             /* throw-in: from over the head, dropping into him */
        if (r >= 0) b->vel = pass_solve(s, r, pace, &meet, &t_meet, &plan);
        else b->vel = fv2_from_angle(p->kick.dir, T_THROW_SPEED);
        vz = FX(1.0);
        fx hv = fv2_len(b->vel);
        if (r >= 0 && hv > FX(1.0)) {                     /* arc it so it arrives at ~0.9 m (controllable, CP0.3e) */
            fx t = fx_clamp(fx_div(fv2_len(fv2_sub(meet, b->pos)), hv), FX(0.2), FX(2.0));
            vz = fx_div(T_THROWIN_ARRIVE_Z - b->z + fx_mul(fx_mul(T_GRAVITY, t), t) / 2, t);
        }
    }
    else if (p->kick.kind == WP_KICK_ROLL) {              /* keeper's underarm roll: along the grass */
        b->z = FX(0.1);
        if (r >= 0) b->vel = pass_solve(s, r, pace, &meet, &t_meet, &plan);
        else b->vel = fv2_from_angle(p->kick.dir, T_ROLL_SPEED);
        vz = 0;
    }
    else if (p->kick.kind == WP_KICK_PUNT) b->vel = fv2_from_angle(p->kick.dir, T_PUNT_SPEED);
    else if (p->kick.kind == WP_KICK_LOB) b->vel = lob_velocity(s, r, p->kick.dir, &vz);
    else if (p->kick.kind == WP_KICK_THROUGH && r >= 0) b->vel = through_solve(s, r, p->kick.power, pace, &meet, &t_meet, &plan);
    else if (r >= 0) b->vel = pass_solve(s, r, pace, &meet, &t_meet, &plan);
    else if (p->kick.kind == WP_KICK_PASS || p->kick.kind == WP_KICK_THROUGH) {
        /* nobody fits: a deliberate ball into space, weighted to stop inside the pitch (no inherited run, so a
         * sideways run can't carry it out). Hold = longer, up to the line (D-021). */
        fv2 u = fv2_from_angle(p->kick.dir, FX_ONE);
        fx hl = s->pitch.length / 2, hw = s->pitch.width / 2;
        fx d_tap = fx_clamp(ray_exit_dist(b->pos, u, hl - T_FREE_MARGIN_X, hw - T_FREE_MARGIN_Y), T_FREE_MIN_ROLL, T_FREE_MAX_ROLL);
        fx d = d_tap;
        if (p->kick.kind == WP_KICK_THROUGH) {
            fx d_max = fx_max(d_tap, fx_min(ray_exit_dist(b->pos, u, hl, hw), ball_roll_distance(T_THROUGH_FREE_MAX)));
            d = fx_lerp(d_tap, d_max, p->kick.power);
        }
        fx v = ball_roll_speed_for(d);
        if (p->kick.kind == WP_KICK_THROUGH)                      /* the meter is always firmer than a tap (CP0.3e) */
            v = fx_max(v, fx_mul(ball_roll_speed_for(d_tap), T_THROUGH_MIN_K));
        b->vel = fv2_scale(u, v);
    }
    else if (p->kick.kind == WP_KICK_SHOT || p->kick.kind == WP_KICK_CHIP) {
        /* shots keep only the part of the run ALONG the strike (extra pace); the sideways part would drag an
         * aimed shot toward the middle of the goal (CP0.3b: shots at the corners were ending at the keeper) */
        fv2 u = fv2_from_angle(p->kick.dir, FX_ONE);
        fx along = fx_max(fv2_dot(p->vel, u), 0);
        b->vel = fv2_scale(u, fx_mul(p->kick.power, shot_q) + fx_mul(along, INHERIT_VEL));
    }
    else b->vel = fv2_add(fv2_from_angle(p->kick.dir, fx_mul(p->kick.power, q)), fv2_scale(p->vel, INHERIT_VEL));
    b->vz = vz;
    b->spin = p->kick.spin;
    b->owner = -1;
    b->last_touch = (int8_t)i;
    b->no_hands = p->kick.kind == WP_KICK_HEADER ? 0 : (uint8_t)(p->team + 1);
    p->immune = T_KICK_IMMUNE;
    p->touch_cooldown = 18;
    s->events |= WP_EV_KICK;
    if (r >= 0) seats_pass_handoff(s, i, r, in, meet, t_meet, plan);
}

static void handle_buttons(SimState *s, int i, const WpSeatInput *in, uint16_t prev) {
    WpPlayer *p = &s->players[i];
    uint16_t now = in->buttons, pressed = now & (uint16_t)~prev, released = prev & (uint16_t)~now;
    int has;
    fv2 aim = stick_dir(in, &has);
    if (s->phase != WP_PHASE_PLAY) { p->kick_charge = 0; p->charge_kind = 0xFF; return; }

    /* ball in the hands. Throw-in: S / A throw it. Keeper (CP0.3e, like real keepers): S rolls it to the ringed
     * mate when he's close, throws it overarm when he's further; A throws it overarm; D/W punt it */
    if (s->ball.held && s->ball.owner == i) {
        if (p->action == WP_ACT_NONE && (pressed & (WP_BTN_PASS | WP_BTN_LOB | WP_BTN_SHOOT | WP_BTN_CHIP))) {
            WpKick k;
            int throwin = s->restart_kind == WP_RS_THROW;
            uint8_t kind = (pressed & (WP_BTN_PASS | WP_BTN_LOB)) ? WP_KICK_THROW : WP_KICK_PUNT;
            if (kind == WP_KICK_PUNT && throwin) return;  /* a throw-in is thrown */
            if (throwin && (pressed & WP_BTN_LOB)) kind = WP_KICK_LOB;   /* the long throw (as before) */
            make_kick(s, i, kind, 0, aim, has, &k);
            if (!throwin && (pressed & WP_BTN_PASS) && k.to &&
                fv2_len(fv2_sub(s->players[k.to - 1].pos, p->pos)) <= T_ROLL_MAX)
                k.kind = WP_KICK_ROLL;                    /* short: roll it along the ground */
            sim_player_kick(s, i, &k);
        }
        p->kick_charge = 0; p->charge_kind = 0xFF;
        return;
    }
    /* contextual tackle: J pressed while an opponent has the ball close by */
    if ((pressed & WP_BTN_PASS) && p->action == WP_ACT_NONE && tackle_wanted(s, i)) {
        tackle_start(s, i);
        return;
    }
    /* contextual header (D-010): pass/shoot pressed while a ball comes in above body height */
    int jump;
    if ((pressed & (WP_BTN_PASS | WP_BTN_SHOOT)) && p->action == WP_ACT_NONE && header_wanted(s, i, &jump)) {
        header_start(s, i, (pressed & WP_BTN_SHOOT) != 0, jump);
        return;
    }
    if ((pressed & WP_BTN_LOB) && p->action == WP_ACT_NONE && ball_reachable(s, i, KICK_START_REACH)) {
        WpKick k; make_kick(s, i, WP_KICK_LOB, 0, aim, has, &k);
        sim_player_kick(s, i, &k);
    }
    /* pass, shoot and chip share the hold/release mechanic. Pass: a tap (<= T_PASS_TAP_TICKS) = targeted pass,
     * held longer = through ball whose lead grows with the hold. */
    for (int c = 0; c < 3; c++) {
        uint16_t btn = c == 0 ? WP_BTN_PASS : c == 1 ? WP_BTN_SHOOT : WP_BTN_CHIP;
        uint8_t kind = c == 0 ? WP_KICK_PASS : c == 1 ? WP_KICK_SHOT : WP_KICK_CHIP;
        if ((pressed & btn) && p->charge_kind == 0xFF) { p->charge_kind = kind; p->kick_charge = 0; }
        if ((now & btn) && p->charge_kind == kind && p->kick_charge < T_CHARGE_MAX_TICKS) p->kick_charge++;
        if ((released & btn) && p->charge_kind == kind) {
            if (p->action == WP_ACT_NONE && ball_reachable(s, i, KICK_START_REACH)) {
                WpKick k;
                if (kind == WP_KICK_PASS && p->kick_charge > T_PASS_TAP_TICKS)
                    make_kick(s, i, WP_KICK_THROUGH, fx_div(FX_INT(p->kick_charge - T_PASS_TAP_TICKS),
                                                            FX_INT(T_CHARGE_MAX_TICKS - T_PASS_TAP_TICKS)), aim, has, &k);
                else
                    make_kick(s, i, kind, fx_div(FX_INT(p->kick_charge), FX_INT(T_CHARGE_MAX_TICKS)), aim, has, &k);
                sim_player_kick(s, i, &k);
            }
            p->charge_kind = 0xFF;
            p->kick_charge = 0;
        }
    }
}

static void move(SimState *s, int i, const WpSeatInput *in) {
    WpPlayer *p = &s->players[i];
    int has;
    fv2 dir = stick_dir(in, &has);
    if (p->z > 0) return;                                 /* airborne: momentum only */
    fx top = player_top_speed(s, i, (in->buttons & WP_BTN_SPRINT) != 0);
    /* dribbling slows you while the ball is at your feet; ramps off smoothly (no flicker at the reach edge)
     * when you're running onto a ball that got away */
    if (s->ball.owner == i) {
        fx gapd = fv2_len(fv2_sub(s->ball.pos, p->pos));
        fx k = fx_clamp(fx_div(gapd - T_TOUCH_REACH, FX(0.6)), 0, FX_ONE);   /* 0 at reach .. 1 at reach+0.6 */
        top = fx_mul(top, fx_lerp(T_DRIBBLE_SPEED_MUL, FX_ONE, k));
    }
    int winding = p->action == WP_ACT_KICK && !p->kick.done;
    if (winding) top = fx_mul(top, T_KICK_SPEED_MUL);
    if (restart_is_taker(s, i)) {                         /* taking a set piece: stands still, may turn to aim */
        p->vel = fv2_make(0, 0);
        if (has) {
            wp_angle w = fx_atan2(dir.y, dir.x);
            int32_t st = DEG_TO_ANG(T_TURN_DEG_S) / WP_TICK_HZ, dd = ang_diff(w, p->facing);
            p->facing = (wp_angle)(p->facing + (dd > st ? st : (dd < -st ? -st : dd)));
        }
        return;
    }
    if (p->action == WP_ACT_KEEPER_DIVE) {                /* flying, then down: slides to a stop */
        if (p->z == 0) p->vel = fv2_scale(p->vel, FX(0.85));
        return;
    }
    if (p->action == WP_ACT_TACKLE) {                     /* lunging, then committed: can't turn or accelerate */
        p->vel = fv2_clamp_len(p->vel, p->kick.done ? FX(2.5) : player_top_speed(s, i, 1));
        return;
    }
    int step_up = 0;
    if (!has && s->ball.owner == i && p->action == WP_ACT_NONE &&
        fv2_len(fv2_sub(s->ball.pos, p->pos)) > T_TOUCH_REACH - FX(0.15)) {
        /* stick released with the ball out of foot reach: step up to it (at most jogging), then stop it */
        dir = fv2_norm(fv2_sub(s->ball.pos, p->pos));
        top = fx_min(top, player_top_speed(s, i, 0));
        has = step_up = 1;
    }
    if (has && !step_up && s->ball.owner == i) {
        /* the ball got out of foot reach: go and get it first, then carry on the way the stick says */
        fv2 to_ball = fv2_sub(s->ball.pos, p->pos);
        if (fv2_len(to_ball) > T_TOUCH_REACH) dir = fv2_norm(to_ball);
    }
    if (has && !step_up) {                                /* analogue magnitude scales speed */
        fx mag = fx_min(fv2_len(fv2_make((fx)in->move_x * (FX_ONE / 127), (fx)in->move_y * (FX_ONE / 127))), FX_ONE);
        top = fx_mul(top, mag);
    }
    fv2 dv = fv2_sub(fv2_scale(dir, top), p->vel);
    p->vel = fv2_add(p->vel, fv2_clamp_len(dv, fx_mul(has ? T_ACCEL : T_STOP_DECEL, WP_DT)));

    /* body turn rate (the ball follows the stick; the body catches up) */
    wp_angle want = p->facing;
    if (winding) want = s->ball.owner == i ? contact_dir(s, i) : p->kick.dir;   /* turn to meet the ball */
    else if (has) want = fx_atan2(dir.y, dir.x);
    int deg = (in->buttons & WP_BTN_SPRINT) && fv2_len(p->vel) > FX(6.5) ? T_TURN_DEG_S_SPRINT
            : (s->ball.owner == i ? T_TURN_DEG_S_BALL : T_TURN_DEG_S);
    if (winding) deg = 1080;
    int32_t step = DEG_TO_ANG(deg) / WP_TICK_HZ, d = ang_diff(want, p->facing);
    p->facing = (wp_angle)(p->facing + (d > step ? step : (d < -step ? -step : d)));
}

/* Possession, first touch, dribble touches, gather during own kick wind-up. */
/* the ball's owner: lose it, gather for a kick, sole trap, or the rhythmic dribble touch */
static void control_owner(SimState *s, int i, const WpSeatInput *in) {
    WpPlayer *p = &s->players[i];
    WpBall *b = &s->ball;
    if (s->phase != WP_PHASE_PLAY) return;
    if (b->held) {                                        /* in the keeper's hands: carried, can't be lost */
        ball_to_hands(s, i);
        if (b->held_ticks < 65535) b->held_ticks++;
        if (b->held_ticks > T_KEEPER_HOLD_MAX && p->action == WP_ACT_NONE && !restart_is_taker(s, i)) {   /* time's up */
            WpKick k = {0};
            k.kind = WP_KICK_PUNT; k.dir = p->facing; k.windup = T_KICK_WINDUP_CHIP;
            sim_player_kick(s, i, &k);
        }
        return;
    }
    if (restart_is_taker(s, i) && p->action == WP_ACT_NONE) {   /* set piece: the ball sits dead on its spot */
        b->vel = fv2_make(0, 0);
        return;
    }
    fv2 d = fv2_sub(b->pos, p->pos);
    fx dist = fv2_len(d);
    int has;
    fv2 sdir = stick_dir(in, &has);

    {
        if (dist > T_LOSE_DIST || b->z > WP_BAND_BODY_MAX) { b->owner = -1; return; }
        if (p->action == WP_ACT_KICK && !p->kick.done) {   /* gather: the foot brings the ball to the contact point */
            if (b->z > WP_BAND_FEET_MAX) return;            /* a dropping ball falls by itself (volley) */
            fv2 spot = fv2_add(p->pos, fv2_from_angle(contact_dir(s, i), FOOT_SPOT));
            /* aim to arrive on the contact tick (moved by ball physics, so line crossings are still seen), but the
             * foot can only drag it so fast; whatever is left shows up as a weaker contact */
            int remaining = (int)p->kick.windup - (int)p->action_timer;
            if (remaining < 1) remaining = 1;
            fv2 rel = fv2_clamp_len(fv2_scale(fv2_sub(spot, b->pos), FX_INT(WP_TICK_HZ) / remaining), T_GATHER_MAX_REL);
            b->vel = fv2_add(p->vel, rel);
            if (b->vz > 0) b->vz = 0;
            return;
        }
        if (b->z > WP_BAND_FEET_MAX || p->action != WP_ACT_NONE) return;
        if (dist > T_TOUCH_REACH) return;                  /* must actually reach the ball with a foot */
        if (!has) {
            /* No stick: sole trap. The ball slows WITH the player, steered toward a spot T_STOP_SPOT out on the
             * side it is already on (never through the body), at a capped speed. Only within foot reach (out of
             * reach, move() steps up to it first). May cut a dribble rhythm short, like a direction change. */
            if (p->touch_cooldown > T_TOUCH_GAP_SPRINT - T_TOUCH_MIN_GAP) return;
            fv2 side = fv2_sub(b->pos, p->pos);
            fv2 sdn = fx_abs(side.x) + fx_abs(side.y) < FX(0.02) ? fv2_from_angle(p->facing, FX_ONE) : fv2_norm(side);
            fv2 rel = fv2_scale(fv2_sub(fv2_add(p->pos, fv2_scale(sdn, T_STOP_SPOT)), b->pos), T_TRAP_GAIN);
            fv2 nv = fv2_add(p->vel, fv2_clamp_len(rel, T_TRAP_MAX_REL));
            if (fv2_len(fv2_sub(nv, b->vel)) > FX(0.6)) {          /* a visible touch, not just the sole riding it */
                b->last_touch = (int8_t)i;
                b->no_hands = (uint8_t)(p->team + 1);
                s->events |= WP_EV_TOUCH;
            }
            b->vel = nv;
            b->spin = 0;
            if (b->vz > 0) b->vz = 0;
            return;
        }
        fv2 mdir = sdir;
        int wrong_way = fv2_dot(fv2_norm(b->vel), mdir) < FX(0.8);
        int sprint = (in->buttons & WP_BTN_SPRINT) != 0;
        int gap = sprint ? T_TOUCH_GAP_SPRINT : T_TOUCH_GAP_WALK;
        /* rhythm: touch whenever the cooldown expires; a change of direction may cut it short */
        if (p->touch_cooldown && !(wrong_way && p->touch_cooldown <= gap - T_TOUCH_MIN_GAP)) return;
        /* Size the touch so that, when the next touch is due (T = gap ticks), the ball sits LEAD ahead of where
         * the player WILL be: predict the player (current velocity accelerating toward stick*top, capped by
         * T_ACCEL), aim the ball at prediction + mdir*lead, and add back what rolling friction takes. One rule
         * covers straight runs, cuts and reversals (a reversal becomes a drag-back). */
        fx T = FX_INT(gap) / WP_TICK_HZ;
        fx lead = sprint ? T_TOUCH_LEAD_SPRINT : T_TOUCH_LEAD_JOG;
        fx top = fx_mul(player_top_speed(s, i, sprint), T_DRIBBLE_SPEED_MUL);
        fv2 v_end = fv2_add(p->vel, fv2_clamp_len(fv2_sub(fv2_scale(mdir, top), p->vel), fx_mul(T_ACCEL, T)));
        fv2 p_pred = fv2_add(p->pos, fv2_scale(fv2_add(p->vel, v_end), T / 2));
        fv2 target = fv2_add(p_pred, fv2_scale(mdir, lead));
        fv2 need = fv2_scale(fv2_sub(target, b->pos), fx_div(FX_ONE, T));
        fv2 ndir = fv2_norm(need);
        b->vel = fv2_add(need, fv2_scale(ndir, fx_mul(ball_roll_decel(fv2_len(need)), T) / 2));
        b->spin = 0;
        if (b->vz > 0) b->vz = 0;
        p->touch_cooldown = (uint16_t)gap;
        b->last_touch = (int8_t)i;
        b->no_hands = (uint8_t)(p->team + 1);
        s->events |= WP_EV_TOUCH;
        return;
    }
}

/* Can player i take the loose ball this tick? Returns the swept distance to it, or -1. (The closest eligible
 * player wins a contest - not the lowest index.) */
static fx gain_check(const SimState *s, int i, const WpSeatInput *in) {
    const WpPlayer *p = &s->players[i];
    const WpBall *b = &s->ball;
    if (s->phase != WP_PHASE_PLAY || !player_can_control(s, i)) return -1;
    if (b->owner >= 0 || p->touch_cooldown || p->action != WP_ACT_NONE || p->z > 0) return -1;
    int gk = keeper_hands(s, i);                          /* hands: own box, no back-pass (D-032) */
    if (b->z > (gk ? T_KEEPER_HANDS_Z : WP_BAND_BODY_MAX)) return -1;
    /* reach is SWEPT over the coming tick (ball.pos .. ball.pos + vel*dt): a ball passing between ticks counts.
     * The pass's receiver may also stretch beyond clean reach, with a heavier touch. */
    fv2 seg = fv2_scale(b->vel, WP_DT);
    fx sl2 = fv2_dot(seg, seg), u = 0;
    if (sl2 > 0) u = fx_clamp(fx_div(fv2_dot(fv2_sub(p->pos, b->pos), seg), sl2), 0, FX_ONE);
    fx dseg = fv2_len(fv2_sub(p->pos, fv2_add(b->pos, fv2_scale(seg, u))));
    /* cutting out the other team's pass in flight needs a closer foot than receiving one */
    int intercept = s->pass_target >= 0 && s->players[s->pass_from].team != p->team;
    fx reach = gk ? T_KEEPER_REACH : intercept ? T_INTERCEPT_REACH : T_CONTROL_REACH;
    if (s->pass_target == i && dseg > T_CONTROL_REACH) {
        /* stretch (heavier touch) only for a ball that would otherwise pass him out of clean reach;
         * one coming to him rolls in to his foot */
        fx bs2 = fv2_dot(b->vel, b->vel);
        fx tc = bs2 > 0 ? fx_max(fx_div(fv2_dot(fv2_sub(p->pos, b->pos), b->vel), bs2), 0) : 0;
        fx closest = fv2_len(fv2_sub(p->pos, fv2_add(b->pos, fv2_scale(b->vel, tc))));
        if (closest > T_CONTROL_REACH) reach = T_CONTROL_STRETCH;
    }
    if (dseg > reach) return -1;
    if (s->pass_target == i && s->pass_plan != WP_PLAN_CLOSEST && s->pass_stick[0] != -128) {
        /* a runner on a planned meet lets a ball passing clear of him run until shortly before the meeting point
         * (he'd otherwise trap a through ball that passes him early) - unless the human is steering him */
        int dx = in->move_x - s->pass_stick[0], dy = in->move_y - s->pass_stick[1];
        int opinion = (in->move_x || in->move_y) && dx * dx + dy * dy > T_RECV_STICK_TOL * T_RECV_STICK_TOL;
        int32_t left = (int32_t)(int16_t)(uint16_t)(s->pass_meet_tick - (uint16_t)s->tick);
        /* ...but a ball coming through his body/feet is taken in stride (never rolls through him, D-016) */
        if (!opinion && left > 12 && dseg > FX(0.5)) return -1;
    }
    if (fv2_len(fv2_sub(b->vel, p->vel)) > (gk ? T_KEEPER_CATCH_REL : T_CONTROL_MAX_REL)) return -1;   /* too hot */
    return dseg;
}

/* first touch: kills most of the relative speed; harder balls spill further (visible cause) */
static void gain_apply(SimState *s, int i, const WpSeatInput *in, fx dseg, const WpInput *all) {
    WpPlayer *p = &s->players[i];
    WpBall *b = &s->ball;
    fv2 rel = fv2_sub(b->vel, p->vel);
    fx rs = fv2_len(rel);
    /* first touch: kills most of the relative speed; harder balls spill further (visible cause) */
    fx spill = fx_mul(fx_div(rs, T_CONTROL_MAX_REL), FX(0.45));
    if (in->buttons & WP_BTN_SPRINT) spill += T_SPRINT_SPILL;     /* receiving at a sprint: heavier touch */
    if (dseg > T_CONTROL_REACH)                                       /* stretching for it */
        spill += fx_mul(T_STRETCH_SPILL, fx_div(dseg - T_CONTROL_REACH, T_CONTROL_STRETCH - T_CONTROL_REACH));
    spill = fx_mul(spill, FX_ONE - fx_mul(T_STAT_CONTROL, stat_k(s, i, WP_STAT_CONTROL)));   /* first touch (D-034) */
    b->vel = fv2_add(p->vel, fv2_scale(rel, spill));
    if (b->z > WP_BAND_FEET_MAX) b->vz = 0;                /* chest/thigh: drop it to the feet */
    b->spin = 0;
    b->owner = (int8_t)i;
    b->last_touch = (int8_t)i;
    p->touch_cooldown = 6;
    s->events |= WP_EV_TOUCH;
    int hands = keeper_hands(s, i);
    b->no_hands = 0;                                      /* controlled: a new deliberate touch sets it again */
    if (hands) {                                          /* gathered into his hands */
        ball_to_hands(s, i);
        b->held_ticks = 0;
    }
    seats_on_gain(s, i, all);                             /* our AI team-mate / keeper won it: control jumps */
}

static void air_and_timers(WpPlayer *p) {
    if (p->z > 0 || p->vz > 0) {
        p->vz -= fx_mul(T_GRAVITY, WP_DT);
        p->z += fx_mul(p->vz, WP_DT);
        if (p->z <= 0) { p->z = 0; p->vz = 0; }
    }
    if (p->action != WP_ACT_NONE) {
        p->action_timer++;
        int ends = p->action == WP_ACT_KICK ? p->action_timer >= T_KICK_TOTAL_TICKS
                 : p->action == WP_ACT_TACKLE ? p->action_timer >= T_TACKLE_TICKS
                 : p->action == WP_ACT_KEEPER_DIVE ? p->action_timer >= T_DIVE_TICKS
                 : (p->action == WP_ACT_JUMP_HEADER || p->action == WP_ACT_STAND_HEADER) ? header_action_ends(p)
                 : p->action_timer > 30;
        if (ends) { p->action = WP_ACT_NONE; p->action_timer = 0; }
    }
    if (p->touch_cooldown) p->touch_cooldown--;
    if (p->immune) p->immune--;
}

void sim_player_jump(SimState *s, int i, uint8_t action) {
    WpPlayer *p = &s->players[i];
    if (p->z > 0 || p->vz != 0) return;
    p->vz = WP_JUMP_VZ;
    p->action = action;
    p->action_timer = 0;
    p->kick.done = 1;                                     /* a plain jump: no header contact attempt */
    p->kick.windup = 1;
}

/* Players block each other: overlapping bodies are pushed apart equally and their closing speed removed.
 * Statues (WP_SIMF_STATIC_OTHERS) are ghosts, as before, so isolation tests keep their meaning. */
void bodies_separate(SimState *s) {
    int np = 2 * s->n_per_side;
    for (int i = 0; i < np; i++)
        for (int j = i + 1; j < np; j++) {
            WpPlayer *a = &s->players[i], *c = &s->players[j];
            if ((s->flags & WP_SIMF_STATIC_OTHERS) && (!player_can_control(s, i) || !player_can_control(s, j))) continue;
            if (a->z > FX(0.5) || c->z > FX(0.5)) continue;
            fv2 d = fv2_sub(c->pos, a->pos);
            if (fx_abs(d.x) >= T_PLAYER_GAP || fx_abs(d.y) >= T_PLAYER_GAP) continue;
            fx dist = fv2_len(d);
            if (dist >= T_PLAYER_GAP) continue;
            fv2 n = dist > FX(0.01) ? fv2_scale(d, fx_div(FX_ONE, dist)) : fv2_make(FX_ONE, 0);
            fv2 half = fv2_scale(n, (T_PLAYER_GAP - dist) / 2);
            a->pos = fv2_sub(a->pos, half);
            c->pos = fv2_add(c->pos, half);
            fx closing = fv2_dot(fv2_sub(a->vel, c->vel), n);          /* > 0 = moving into each other */
            if (closing > 0) {
                fv2 cut = fv2_scale(n, closing / 2);
                a->vel = fv2_sub(a->vel, cut);
                c->vel = fv2_add(c->vel, cut);
            }
        }
}

void players_step(SimState *s, const WpInput *in) {
    int np = 2 * s->n_per_side;
    /* pin = what the player DOES (raw human input, or placeholder AI); mv = how he moves (an assisted receive steers
     * the legs only - aim and buttons always come from the human's real stick) */
    WpSeatInput pin[WP_MAX_PLAYERS], mv[WP_MAX_PLAYERS];
    seats_update(s, in);                                  /* switching first: this tick's inputs go to the new player */
    team_plan_update(s);                                  /* the team layer (D-030), before anyone decides */
    for (int i = 0; i < np; i++) {
        int seat = seat_of(s, i);
        if (seat >= 0) {
            pin[i] = mv[i] = in->seat[seat];
            pass_pick_update(s, seat, i, &pin[i]);            /* the receiver preview the HUD shows */
            if (s->latch_ticks[seat] && s->ball.owner == i) {   /* inherited stick: don't dribble with it */
                pin[i].move_x = pin[i].move_y = 0;
                mv[i] = pin[i];
            }
            if (!seats_assist_receive(s, seat, &mv[i])) seats_chase_assist(s, seat, &mv[i]);   /* D-037 */
        }
        else if (s->flags & WP_SIMF_STATIC_OTHERS) pin[i] = mv[i] = (WpSeatInput){0, 0, 0};
        else { brain_input(s, i, &pin[i]); mv[i] = pin[i]; }
        header_assist_move(s, i, &mv[i]);                 /* going for a header: the legs step under it */
    }
    for (int i = 0; i < np; i++) {
        int seat = seat_of(s, i);
        if (seat >= 0) handle_buttons(s, i, &pin[i], s->prev_buttons[seat]);
        move(s, i, &mv[i]);
        WpPlayer *p = &s->players[i];
        p->pos = fv2_add(p->pos, fv2_scale(p->vel, WP_DT));
        stamina_step(s, i);
        {   /* the advertising boards (D-033): nobody walks through them */
            fx lx = s->pitch.length / 2 + T_BOARD_END - T_BOARD_BODY, ly = s->pitch.width / 2 + T_BOARD_SIDE - T_BOARD_BODY;
            if (fx_abs(p->pos.x) > lx) { p->pos.x = p->pos.x > 0 ? lx : -lx; p->vel.x = 0; }
            if (fx_abs(p->pos.y) > ly) { p->pos.y = p->pos.y > 0 ? ly : -ly; p->vel.y = 0; }
        }
        if (s->ball.held && s->ball.owner == i && p->role == WP_ROLE_KEEPER && !restart_is_taker(s, i))
            p->pos = clamp_own_box(s, p->team, p->pos);   /* can't carry it out of his box */
        if (p->action == WP_ACT_KICK && !p->kick.done && p->action_timer + 1 >= p->kick.windup) contact(s, i, &pin[i]);
        header_step(s, i);
        tackle_step(s, i);
        keeper_step(s, i, in);
        air_and_timers(p);
    }
    {   /* head contact: of everyone who can head it this tick, the nearest head wins (not the lower index) */
        int best = -1; fx bd = 0;
        for (int i = 0; i < np; i++) {
            fx d = header_contact_dist(s, i);
            if (d >= 0 && (best < 0 || d < bd)) { best = i; bd = d; }
        }
        if (best >= 0) {
            WpSeatInput aim = pin[best];
            if (seat_of(s, best) < 0) {                   /* the AI aims at its chosen target */
                fv2 d = fv2_norm(fv2_sub(s->players[best].ai_goal, s->ball.pos));
                aim.move_x = (int8_t)fx_clamp(d.x * 127 / FX_ONE, -127, 127);
                aim.move_y = (int8_t)fx_clamp(d.y * 127 / FX_ONE, -127, 127);
            }
            header_contact(s, best, &aim);
        }
    }
    restart_step(s);                                      /* set piece: taker still, opponents at distance */
    bodies_separate(s);                                   /* players block each other */
    if (s->ball.owner >= 0) {
        control_owner(s, s->ball.owner, &pin[s->ball.owner]);
    } else {                                              /* loose ball: the closest eligible player gets it */
        int best = -1; fx bd = 0;
        for (int i = 0; i < np; i++) {
            fx d = gain_check(s, i, &pin[i]);
            if (d >= 0 && (best < 0 || d < bd)) { best = i; bd = d; }
        }
        if (best >= 0) gain_apply(s, best, &pin[best], bd, in);
    }
}
