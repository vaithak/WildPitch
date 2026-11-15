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

/* Best team-mate for a pass toward `dir`: within T_PASS_CONE_DEG, scored by angle off the stick and distance
 * (nearer and straighter wins). Keeper only if nearly straight at him. -1 if nobody fits (pass into space). */
int pass_pick(const SimState *s, int i, wp_angle dir) {
    const WpPlayer *p = &s->players[i];
    int best = -1; int32_t best_score = 0;
    fx hl = s->pitch.length / 2 - FX(1.0), hw = s->pitch.width / 2 - FX(1.0);
    /* outfield team-mates first; the keeper only when nobody else fits (no firm back-passes to him by accident) */
    for (int pass = 0; pass < 2 && best < 0; pass++)
    for (int j = p->team * s->n_per_side; j < (p->team + 1) * s->n_per_side; j++) {
        if (j == i || (s->players[j].role == WP_ROLE_KEEPER) != (pass == 1)) continue;
        const WpPlayer *q = &s->players[j];
        fx d0 = fv2_len(fv2_sub(q->pos, p->pos));
        if (d0 > T_PASS_MAX_DIST + FX(4.0)) continue;
        /* judge him where he'll be when the ball gets there (flight ~ distance / 12 m/s), kept on the pitch */
        fv2 qp = fv2_add(q->pos, fv2_scale(q->vel, fx_div(d0, FX(12.0))));
        qp = fv2_make(fx_clamp(qp.x, -hl, hl), fx_clamp(qp.y, -hw, hw));
        fv2 d = fv2_sub(qp, p->pos);
        fx dist = fv2_len(d);
        if (dist < FX(2.0) || dist > T_PASS_MAX_DIST) continue;
        int32_t off = ang_diff(fx_atan2(d.y, d.x), dir);
        if (off < 0) off = -off;
        int32_t lim = DEG_TO_ANG(q->role == WP_ROLE_KEEPER ? 20 : T_PASS_CONE_DEG);
        if (off > lim) continue;
        /* score: 1 degree off ~ 0.8 m further away; long balls scored down from T_PASS_PENALTY_M */
        int32_t score = (int32_t)(((int64_t)off * 360 * 10) / 65536) + (int32_t)(dist / (FX_ONE / 8));
        if (dist > T_PASS_PENALTY_M) score += (int32_t)((dist - T_PASS_PENALTY_M) / (FX_ONE / 16));
        if (best < 0 || score < best_score) { best = j; best_score = score; }
    }
    return best;
}

/* Aim assist (D-013): roughly toward goal, the stick picks a TARGET rather than an exact angle — up/down = that
 * side of the goal, straight/none = the far post from where you stand. A share of the stick's angular error
 * remains so aiming still matters. Shared by shots, chips and headers. */
wp_angle kick_aim_assist(const SimState *s, int i, wp_angle dir, fv2 aim, int has_aim) {
    const WpPlayer *p = &s->players[i];
    int sg = p->team == 0 ? 1 : -1;
    fx side = fx_mul(s->pitch.goal_width, T_AIM_SIDE);
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
static void make_kick(const SimState *s, int i, uint8_t kind, fx charge01, fv2 aim, int has_aim, WpKick *k) {
    const WpPlayer *p = &s->players[i];
    wp_angle dir = has_aim ? fx_atan2(aim.y, aim.x) : p->facing;
    k->kind = kind;
    k->spin = 0;
    k->to = 0;
    if (kind == WP_KICK_PASS) {
        k->power = T_PASS_SPEED; k->vz = 0; k->windup = T_KICK_WINDUP_PASS;
        int r = pass_pick(s, i, dir);
        if (r >= 0) {                                      /* targeted: aimed & weighted again at contact */
            fv2 d = fv2_sub(s->players[r].pos, p->pos);
            dir = fx_atan2(d.y, d.x);
            k->to = (uint8_t)(r + 1);
        }
    } else if (kind == WP_KICK_THROUGH || kind == WP_KICK_LOB) {
        /* power field = the hold h (0..1): through ball = how deep/soft (D-020); lob: unused */
        k->power = charge01;
        k->vz = 0;
        k->windup = kind == WP_KICK_LOB ? T_KICK_WINDUP_CHIP : T_KICK_WINDUP_PASS;
        int r = pass_pick(s, i, dir);
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
static fx pass_arrive(fx d) {
    return fx_clamp(T_PASS_ARR_MIN + fx_mul(T_PASS_ARR_PER_M, d - FX(5.0)), T_PASS_ARR_MIN, T_PASS_ARR_MAX);
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
static fv2 pass_solve(const SimState *s, int r, fv2 *meet, fx *t_meet, uint8_t *plan) {
    const WpPlayer *q = &s->players[r];
    fv2 b = s->ball.pos, aim = q->pos;
    fx f = 0, v0 = 0;
    for (int it = 0; it < 3; it++) {
        fx d = fv2_len(fv2_sub(aim, b));
        v0 = launch_for(d, pass_arrive(d), T_PASS_MAX, &f);
        aim = clamp_box(s, fv2_add(q->pos, fv2_scale(q->vel, f)), FX(1.0), FX(1.0));
    }
    fx d = fv2_len(fv2_sub(aim, b));
    v0 = launch_for(d, pass_arrive(d), T_PASS_MAX, &f);
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
static fv2 through_solve(const SimState *s, int r, fx h, fv2 *meet, fx *t_meet, uint8_t *plan) {
    const WpPlayer *q = &s->players[r];
    fv2 b = s->ball.pos;
    fx spd = fv2_len(q->vel), sp, delay = 0, hl = s->pitch.length / 2, hw = s->pitch.width / 2;
    fv2 u, attack = fv2_make(q->team == 0 ? FX_ONE : -FX_ONE, 0);
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
    fx tap_v0 = fv2_len(pass_solve(s, r, &tap_meet, &tap_t, &tap_plan));
    for (int attempt = 0; attempt < 3; attempt++) {
        fx s_plan = attempt == 2 ? fx_max(FX(5.0), fx_mul(sp, FX(0.75))) : sp;
        fx va_lo = fx_max(FX(5.0), s_plan - FX(1.5)), va_hi = fx_max(fx_min(fx_mul(s_plan, FX(1.5)), FX(12.0)), va_lo);
        fx va = attempt == 0 ? va_hi - fx_mul(h, va_hi - va_lo) : va_hi;
        fx R_va = ball_roll_distance(va), T_va = ball_roll_time(va);
        for (int k = 1; k <= 80; k++) {
            fx t = FX(0.05) * k;
            fx run = fx_max(fx_mul(s_plan, t - delay), 0);
            fv2 P = fv2_add(q->pos, fv2_scale(u, run + T_THROUGH_STRIDE));
            if (fx_abs(P.x) > hl - T_THROUGH_MARGIN_X || fx_abs(P.y) > hw - T_THROUGH_MARGIN_Y) break;
            fx d = fv2_len(fv2_sub(P, b));
            fx v0 = ball_roll_speed_for(d + R_va);
            if (v0 > T_THROUGH_MAX) continue;
            if (ball_roll_time(v0) - T_va > t) continue;   /* ball not there in time yet */
            *meet = fv2_sub(P, fv2_scale(u, T_THROUGH_STRIDE));
            *t_meet = t;
            *plan = attempt < 2 ? WP_PLAN_STRIDE : WP_PLAN_RUN;
            return fv2_scale(fv2_norm(fv2_sub(P, b)), fx_max(v0, tap_v0));
        }
    }
    /* short room: lead pass clamped inside the margins, runner eases to collect it */
    fv2 aim = clamp_box(s, tap_meet, T_THROUGH_MARGIN_X, T_THROUGH_MARGIN_Y);
    fx d = fv2_len(fv2_sub(aim, b)), f;
    fx v0 = launch_for(d, pass_arrive(d), T_THROUGH_MAX, &f);
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
    fx vz = p->kick.vz, t_meet = FX(1.5);
    int r = (int)p->kick.to - 1;
    fv2 meet = r >= 0 ? s->players[r].pos : b->pos;
    uint8_t plan = WP_PLAN_CLOSEST;
    if (p->kick.kind == WP_KICK_LOB) b->vel = lob_velocity(s, r, p->kick.dir, &vz);
    else if (p->kick.kind == WP_KICK_THROUGH && r >= 0) b->vel = through_solve(s, r, p->kick.power, &meet, &t_meet, &plan);
    else if (r >= 0) b->vel = pass_solve(s, r, &meet, &t_meet, &plan);
    else if (p->kick.kind == WP_KICK_THROUGH)               /* hold with nobody in the cone: the hold is power */
        b->vel = fv2_add(fv2_from_angle(p->kick.dir, fx_mul(fx_lerp(T_THROUGH_FREE_MIN, T_THROUGH_FREE_MAX, p->kick.power), q)),
                         fv2_scale(p->vel, INHERIT_VEL));
    else b->vel = fv2_add(fv2_from_angle(p->kick.dir, fx_mul(p->kick.power, q)), fv2_scale(p->vel, INHERIT_VEL));
    b->vz = vz;
    b->spin = p->kick.spin;
    b->owner = -1;
    b->last_touch = (int8_t)i;
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
    fx top = (in->buttons & WP_BTN_SPRINT) ? T_SPRINT_SPEED : T_WALK_SPEED;
    /* dribbling slows you while the ball is at your feet; ramps off smoothly (no flicker at the reach edge)
     * when you're running onto a ball that got away */
    if (s->ball.owner == i) {
        fx gapd = fv2_len(fv2_sub(s->ball.pos, p->pos));
        fx k = fx_clamp(fx_div(gapd - T_TOUCH_REACH, FX(0.6)), 0, FX_ONE);   /* 0 at reach .. 1 at reach+0.6 */
        top = fx_mul(top, fx_lerp(T_DRIBBLE_SPEED_MUL, FX_ONE, k));
    }
    int winding = p->action == WP_ACT_KICK && !p->kick.done;
    if (winding) top = fx_mul(top, T_KICK_SPEED_MUL);
    int step_up = 0;
    if (!has && s->ball.owner == i && p->action == WP_ACT_NONE &&
        fv2_len(fv2_sub(s->ball.pos, p->pos)) > T_TOUCH_REACH - FX(0.15)) {
        /* stick released with the ball out of foot reach: step up to it (at most jogging), then stop it */
        dir = fv2_norm(fv2_sub(s->ball.pos, p->pos));
        top = fx_min(top, T_WALK_SPEED);
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
static void control(SimState *s, int i, const WpSeatInput *in) {
    WpPlayer *p = &s->players[i];
    WpBall *b = &s->ball;
    if (s->phase != WP_PHASE_PLAY) return;
    fv2 d = fv2_sub(b->pos, p->pos);
    fx dist = fv2_len(d);
    int has;
    fv2 sdir = stick_dir(in, &has);

    if (b->owner == i) {
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
        fx top = fx_mul(sprint ? T_SPRINT_SPEED : T_WALK_SPEED, T_DRIBBLE_SPEED_MUL);
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
        s->events |= WP_EV_TOUCH;
        return;
    }
    /* gain control of a loose ball */
    if (b->owner >= 0 || p->touch_cooldown || p->action != WP_ACT_NONE || p->z > 0) return;
    if (b->z > WP_BAND_BODY_MAX) return;
    /* reach is SWEPT over the coming tick (ball.pos .. ball.pos + vel*dt): a ball passing between ticks counts.
     * The pass's receiver may also stretch beyond clean reach, with a heavier touch. */
    fv2 seg = fv2_scale(b->vel, WP_DT);
    fx sl2 = fv2_dot(seg, seg), u = 0;
    if (sl2 > 0) u = fx_clamp(fx_div(fv2_dot(fv2_sub(p->pos, b->pos), seg), sl2), 0, FX_ONE);
    fx dseg = fv2_len(fv2_sub(p->pos, fv2_add(b->pos, fv2_scale(seg, u))));
    (void)dist;
    fx reach = T_CONTROL_REACH;
    if (s->pass_target == i && dseg > T_CONTROL_REACH) {
        /* stretch (heavier touch) only for a ball that would otherwise pass him out of clean reach;
         * one coming to him rolls in to his foot */
        fx bs2 = fv2_dot(b->vel, b->vel);
        fx tc = bs2 > 0 ? fx_max(fx_div(fv2_dot(fv2_sub(p->pos, b->pos), b->vel), bs2), 0) : 0;
        fx closest = fv2_len(fv2_sub(p->pos, fv2_add(b->pos, fv2_scale(b->vel, tc))));
        if (closest > T_CONTROL_REACH) reach = T_CONTROL_STRETCH;
    }
    if (dseg > reach) return;
    if (s->pass_target == i && s->pass_plan != WP_PLAN_CLOSEST && s->pass_stick[0] != -128) {
        /* a runner on a planned meet lets a ball passing clear of him run until shortly before the meeting point
         * (he'd otherwise trap a through ball that passes him early) - unless the human is steering him */
        int dx = in->move_x - s->pass_stick[0], dy = in->move_y - s->pass_stick[1];
        int opinion = (in->move_x || in->move_y) && dx * dx + dy * dy > T_RECV_STICK_TOL * T_RECV_STICK_TOL;
        int32_t left = (int32_t)(int16_t)(uint16_t)(s->pass_meet_tick - (uint16_t)s->tick);
        /* ...but a ball coming through his body/feet is taken in stride (never rolls through him, D-016) */
        if (!opinion && left > 12 && dseg > FX(0.5)) return;
    }
    fv2 rel = fv2_sub(b->vel, p->vel);
    fx rs = fv2_len(rel);
    if (rs > T_CONTROL_MAX_REL) return;                    /* too hot: body collision deflects it */
    /* first touch: kills most of the relative speed; harder balls spill further (visible cause) */
    fx spill = fx_mul(fx_div(rs, T_CONTROL_MAX_REL), FX(0.45));
    if (in->buttons & WP_BTN_SPRINT) spill += T_SPRINT_SPILL;     /* receiving at a sprint: heavier touch */
    if (dseg > T_CONTROL_REACH)                                       /* stretching for it */
        spill += fx_mul(T_STRETCH_SPILL, fx_div(dseg - T_CONTROL_REACH, T_CONTROL_STRETCH - T_CONTROL_REACH));
    b->vel = fv2_add(p->vel, fv2_scale(rel, spill));
    if (b->z > WP_BAND_FEET_MAX) b->vz = 0;                /* chest/thigh: drop it to the feet */
    b->spin = 0;
    b->owner = (int8_t)i;
    b->last_touch = (int8_t)i;
    p->touch_cooldown = 6;
    s->events |= WP_EV_TOUCH;
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

void players_step(SimState *s, const WpInput *in) {
    int np = 2 * s->n_per_side;
    /* pin = what the player DOES (raw human input, or placeholder AI); mv = how he moves (an assisted receive steers
     * the legs only - aim and buttons always come from the human's real stick) */
    WpSeatInput pin[WP_MAX_PLAYERS], mv[WP_MAX_PLAYERS];
    seats_update(s, in);                                  /* switching first: this tick's inputs go to the new player */
    for (int i = 0; i < np; i++) {
        int seat = seat_of(s, i);
        if (seat >= 0) {
            pin[i] = mv[i] = in->seat[seat];
            if (s->latch_ticks[seat] && s->ball.owner == i) {   /* inherited stick: don't dribble with it */
                pin[i].move_x = pin[i].move_y = 0;
                mv[i] = pin[i];
            }
            seats_assist_receive(s, seat, &mv[i]);
            header_assist_move(s, i, &mv[i]);
        }
        else if (s->flags & WP_SIMF_STATIC_OTHERS) pin[i] = mv[i] = (WpSeatInput){0, 0, 0};
        else { team_input(s, i, &pin[i]); mv[i] = pin[i]; }
    }
    for (int i = 0; i < np; i++) {
        int seat = seat_of(s, i);
        if (seat >= 0) handle_buttons(s, i, &pin[i], s->prev_buttons[seat]);
        move(s, i, &mv[i]);
        WpPlayer *p = &s->players[i];
        p->pos = fv2_add(p->pos, fv2_scale(p->vel, WP_DT));
        if (p->action == WP_ACT_KICK && !p->kick.done && p->action_timer + 1 >= p->kick.windup) contact(s, i, &pin[i]);
        if (seat >= 0) header_step(s, i, &pin[i]);
        air_and_timers(p);
    }
    for (int i = 0; i < np; i++)
        if (seat_of(s, i) >= 0) control(s, i, &pin[i]);
}
