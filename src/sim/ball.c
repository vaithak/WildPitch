/* ball.c - ball physics. Substepped so the ball never travels more than T_SUBSTEP_MAX_MOVE per substep:
 * no tunnelling through posts (r=0.06) or bodies (r=0.3) even for 30 m/s shots.
 * Per substep: forces (drag, Magnus curl, gravity) -> integrate -> grass bounce/roll -> posts/crossbar ->
 * player bodies -> goal line / touchline / net. */
#include "sim/sim_internal.h"

static void reflect(fx *vx, fx *vy, fv2 n, fx rest) {
    fx vn = fx_mul(*vx, n.x) + fx_mul(*vy, n.y);
    if (vn >= 0) return;                              /* already separating */
    fx k = fx_mul(vn, FX_ONE + rest);
    *vx -= fx_mul(n.x, k);
    *vy -= fx_mul(n.y, k);
}

/* ---- forces + integration for one substep ---- */
static void integrate(SimState *s, fx dt) {
    WpBall *b = &s->ball;
    int airborne = b->z > 0 || b->vz > 0;
    fx hs = fv2_len(b->vel);
    if (airborne) {
        fx sp = fv2_len(fv2_make(hs, b->vz));
        fx kd = fx_min(fx_mul(fx_mul(T_AIR_DRAG, sp), dt), FX_ONE);   /* a = k v^2 -> dv = k|v| v dt */
        fx ax = -fx_mul(fx_mul(T_CURL, b->spin), b->vel.y);           /* Magnus: perpendicular to velocity */
        fx ay =  fx_mul(fx_mul(T_CURL, b->spin), b->vel.x);
        b->vel.x += fx_mul(ax, dt) - fx_mul(b->vel.x, kd);
        b->vel.y += fx_mul(ay, dt) - fx_mul(b->vel.y, kd);
        b->vz -= fx_mul(b->vz, kd) + fx_mul(T_GRAVITY, dt);
        b->spin -= fx_mul(fx_mul(b->spin, T_SPIN_DECAY_AIR), dt);
    } else {
        fx dec = fx_mul(ball_roll_decel(hs), dt);
        if (hs <= dec) b->vel = fv2_make(0, 0);
        else b->vel = fv2_scale(b->vel, fx_div(hs - dec, hs));
        b->spin -= fx_mul(fx_mul(b->spin, T_SPIN_DECAY_GROUND), dt);
    }
    b->pos = fv2_add(b->pos, fv2_scale(b->vel, dt));
    b->z += fx_mul(b->vz, dt);
    if (b->z <= 0 && airborne) {
        b->z = 0;
        fx vin = -b->vz;                                  /* impact speed */
        if (vin > FX(1.5)) s->events |= WP_EV_BOUNCE;
        b->vz = fx_mul(vin, T_BOUNCE_REST) - T_BOUNCE_LIN;
        if (b->vz < T_BOUNCE_CUT) b->vz = 0;
        /* impact friction: continuous in vin (no threshold cliff), harder landings lose more, capped (grip) */
        fx hsp = fv2_len(b->vel);
        if (hsp > 0) {
            fx loss = fx_min(fx_mul(fx_mul(T_BOUNCE_MU, FX_ONE + T_BOUNCE_REST), vin), fx_mul(T_BOUNCE_GRIP, hsp));
            b->vel = fv2_scale(b->vel, fx_div(hsp - loss, hsp));
        }
    }
    if (b->z < 0) b->z = 0;
}

/* ---- goal frame: two posts + crossbar per goal ---- */
static void collide_frame(SimState *s) {
    WpBall *b = &s->ball;
    fx hl = s->pitch.length / 2, gw2 = s->pitch.goal_width / 2, gh = s->pitch.goal_height;
    fx reach = T_POST_R + T_BALL_R;
    for (int sg = -1; sg <= 1; sg += 2) {
        fx gx = sg * hl;
        if (fx_abs(b->pos.x - gx) > reach + FX(0.05)) continue;
        for (int side = -1; side <= 1; side += 2) {        /* posts (vertical cylinders) */
            fx py = side * (gw2 + T_POST_R);
            if (b->z > gh + T_POST_R) continue;
            fv2 d = fv2_make(b->pos.x - gx, b->pos.y - py);
            fx dist = fv2_len(d);
            if (dist < reach && dist > 0) {
                fv2 n = fv2_scale(d, fx_div(FX_ONE, dist));
                reflect(&b->vel.x, &b->vel.y, n, T_POST_REST);
                b->pos = fv2_make(gx + fx_mul(n.x, reach), py + fx_mul(n.y, reach));
                s->events |= WP_EV_POST;
            }
        }
        if (fx_abs(b->pos.y) < gw2 + T_POST_R) {          /* crossbar (horizontal cylinder along y) */
            fv2 d = fv2_make(b->pos.x - gx, b->z - (gh + T_POST_R));
            fx dist = fv2_len(d);
            if (dist < reach && dist > 0) {
                fv2 n = fv2_scale(d, fx_div(FX_ONE, dist));
                reflect(&b->vel.x, &b->vz, n, T_POST_REST);
                b->pos.x = gx + fx_mul(n.x, reach);
                b->z = gh + T_POST_R + fx_mul(n.y, reach);
                if (b->z < 0) b->z = 0;
                s->events |= WP_EV_POST;
            }
        }
    }
}

/* ---- player bodies: upright cylinders. Skipped for the owner (feet handle it), the kicker right after a
 * kick, and a player able to control a ball arriving slowly enough at feet/body height (control wins). ---- */
static void collide_bodies(SimState *s) {
    WpBall *b = &s->ball;
    for (int i = 0; i < 2 * s->n_per_side; i++) {
        WpPlayer *p = &s->players[i];
        if (i == b->owner || p->immune) continue;
        if ((p->action == WP_ACT_JUMP_HEADER || p->action == WP_ACT_STAND_HEADER) && p->kick.kind == WP_KICK_HEADER &&
            !p->kick.done && b->z > WP_BAND_FEET_MAX) continue;    /* going for a header: the head decides (D-010) */
        if (b->z > p->z + T_BODY_H || b->z + T_BALL_R < p->z) continue;
        fx r = (p->role == WP_ROLE_KEEPER ? T_KEEPER_BODY_R : T_BODY_R) + T_BALL_R;
        fv2 d = fv2_sub(b->pos, p->pos);
        if (fx_abs(d.x) > r || fx_abs(d.y) > r) continue;
        fx dist = fv2_len(d);
        if (dist >= r || dist == 0) continue;
        fv2 rel = fv2_sub(b->vel, p->vel);
        if (s->phase == WP_PHASE_PLAY && b->owner < 0 && player_can_control(s, i) && p->touch_cooldown == 0 &&
            b->z <= WP_BAND_BODY_MAX && fv2_len(rel) <= T_CONTROL_MAX_REL && p->action == WP_ACT_NONE)
            continue;
        fv2 n = fv2_scale(d, fx_div(FX_ONE, dist));
        reflect(&rel.x, &rel.y, n, T_BODY_REST);
        b->vel = fv2_add(rel, p->vel);
        b->pos = fv2_add(p->pos, fv2_scale(n, r));
        b->last_touch = (int8_t)i;
        if (p->touch_cooldown < T_BODY_HIT_COOLDOWN) p->touch_cooldown = T_BODY_HIT_COOLDOWN;
        s->events |= WP_EV_BODY;
    }
}

/* ---- goal line crossing (interpolated), touchline, and the net box behind each goal line ---- */
static void lines_and_net(SimState *s, fv2 prev, fx prev_z, fx dt) {
    WpBall *b = &s->ball;
    fx hl = s->pitch.length / 2, hw = s->pitch.width / 2;
    fx gw2 = s->pitch.goal_width / 2, gh = s->pitch.goal_height, gd = s->pitch.goal_depth, R = T_BALL_R;
    for (int sg = -1; sg <= 1; sg += 2) {
        fx x0 = sg * prev.x, x1 = sg * b->pos.x, line = hl + R;
        if (x0 <= line && x1 > line && s->phase == WP_PHASE_PLAY) {       /* whole ball crossed this goal line */
            fx t = fx_div(line - x0, x1 - x0);
            fx yc = prev.y + fx_mul(b->pos.y - prev.y, t);
            fx zc = prev_z + fx_mul(b->z - prev_z, t);
            if (fx_abs(yc) < gw2 && zc < gh) {
                int scorer = sg > 0 ? 0 : 1;                              /* team 0 attacks +x */
                s->score[scorer]++;
                s->kickoff_team = (uint8_t)(1 - scorer);
                s->phase = WP_PHASE_GOAL;
                s->phase_timer = T_GOAL_PAUSE_TICKS;
                s->events |= WP_EV_GOAL;
            } else {
                s->phase = WP_PHASE_OUT;
                s->phase_timer = T_OUT_PAUSE_TICKS;
                s->events |= WP_EV_OUT;
                restart_out(s, 0, yc, sg);                                /* corner or goal kick */
            }
            b->owner = -1;
        }
        if (x1 <= hl) continue;
        /* net box: hl..hl+gd behind the line, |y| < gw2, z < gh. Soft walls from both sides. */
        int was_in = sg * prev.x > hl && fx_abs(prev.y) < gw2 && prev_z < gh;
        if (was_in) {
            fx k = FX_ONE - fx_min(fx_mul(T_NET_DRAG, dt), FX_ONE);          /* net drag, per substep */
            b->vel = fv2_scale(b->vel, k);
            if (x1 > hl + gd - R) { b->pos.x = sg * (hl + gd - R); b->vel.x = -fx_mul(b->vel.x, T_NET_KEEP);
                                    b->vel.y = fx_mul(b->vel.y, FX(0.5)); b->vz = fx_mul(b->vz, FX(0.5)); s->events |= WP_EV_NET; }
            if (fx_abs(b->pos.y) > gw2 - R) { b->pos.y = b->pos.y > 0 ? gw2 - R : -(gw2 - R);
                                    b->vel.y = -fx_mul(b->vel.y, T_NET_KEEP); b->vel.x = fx_mul(b->vel.x, FX(0.5)); s->events |= WP_EV_NET; }
            if (b->z > gh - R) { b->z = gh - R; b->vz = -fx_mul(b->vz, T_NET_KEEP); s->events |= WP_EV_NET; }
        } else if (x1 < hl + gd + R) {
            if (fx_abs(prev.y) >= gw2 + R && fx_abs(b->pos.y) < gw2 + R && b->z < gh) {   /* side netting */
                b->pos.y = prev.y > 0 ? gw2 + R : -(gw2 + R);
                b->vel.y = -fx_mul(b->vel.y, T_NET_KEEP);
                s->events |= WP_EV_NET;
            }
            if (prev_z >= gh + R && b->z < gh + R && fx_abs(b->pos.y) < gw2) {              /* roof of the net */
                b->z = gh + R;
                b->vz = -fx_mul(b->vz, FX(0.3));
                s->events |= WP_EV_NET;
            }
        }
    }
    if (s->phase == WP_PHASE_PLAY && fx_abs(b->pos.y) > hw + R) {     /* touchline */
        restart_out(s, 1, b->pos.x, 0);                                   /* throw-in */
        s->phase = WP_PHASE_OUT;
        s->phase_timer = T_OUT_PAUSE_TICKS;
        s->events |= WP_EV_OUT;
        b->owner = -1;
    }
}

/* ---- ground-roll tables (D-020): R(v) = distance a ground ball at speed v rolls before stopping, T(v) = time it
 * takes. Built once from ONE trajectory of the same per-tick integration integrate() uses (the decel law depends
 * only on speed, so every speed's future is a tail of that trajectory). Lazily built, deterministic, read-only
 * afterwards. Replaces the old per-call bisection (100x cheaper). ---- */
#define RT_STEP  FX(0.25)
#define RT_N     125                     /* 0 .. 31 m/s */
static fx RT_R[RT_N], RT_T[RT_N];
static int rt_ready;

static void rt_build(void) {
    static fx tv[1400], tx[1400];        /* speed and distance after each tick, starting above the table */
    int n = 0;
    fx v = RT_STEP * RT_N, x = 0;
    tv[n] = v; tx[n] = 0; n++;
    while (v > 0 && n < 1400) {
        int ns = 1 + (int)(fx_mul(v, WP_DT) / T_SUBSTEP_MAX_MOVE);
        if (ns > 12) ns = 12;
        fx dt = WP_DT / ns;
        for (int k = 0; k < ns && v > 0; k++) {
            fx dec = fx_mul(ball_roll_decel(v), dt);
            v = v > dec ? v - dec : 0;
            x += fx_mul(v, dt);
        }
        tv[n] = v; tx[n] = x; n++;
    }
    fx X = tx[n - 1], Tn = FX_INT(n - 1);
    int k = 0;
    for (int i = RT_N - 1; i >= 0; i--) {
        fx g = RT_STEP * i;
        while (k + 1 < n - 1 && tv[k + 1] >= g) k++;          /* tv[k] >= g > tv[k+1] */
        fx span = tv[k] - tv[k + 1];
        fx frac = span > 0 ? fx_div(tv[k] - g, span) : 0;
        if (g == 0) { RT_R[i] = 0; RT_T[i] = 0; continue; }
        fx xa = tx[k] + fx_mul(tx[k + 1] - tx[k], frac);
        fx ta = FX_INT(k) + frac;
        RT_R[i] = X - xa;
        RT_T[i] = (Tn - ta) / WP_TICK_HZ;
    }
    rt_ready = 1;
}

static fx rt_lookup(const fx *tab, fx v) {
    if (!rt_ready) rt_build();
    if (v <= 0) return 0;
    int i = (int)(v / RT_STEP);
    if (i >= RT_N - 1) i = RT_N - 2;
    fx frac = fx_div(v - RT_STEP * i, RT_STEP);
    return tab[i] + fx_mul(tab[i + 1] - tab[i], frac);
}

fx ball_roll_distance(fx v) { return rt_lookup(RT_R, v); }
fx ball_roll_time(fx v) { return rt_lookup(RT_T, v); }

fx ball_roll_speed_for(fx d) {              /* inverse of R: binary search + interpolation */
    if (!rt_ready) rt_build();
    if (d <= 0) return 0;
    int lo = 0, hi = RT_N - 1;
    if (d >= RT_R[hi]) return RT_STEP * hi;
    while (hi - lo > 1) { int mid = (lo + hi) / 2; if (RT_R[mid] < d) lo = mid; else hi = mid; }
    fx span = RT_R[hi] - RT_R[lo];
    return RT_STEP * lo + (span > 0 ? fx_mul(RT_STEP, fx_div(d - RT_R[lo], span)) : 0);
}

/* speed of a ground ball launched at v0 after it has rolled d (0 if it stops first) */
fx ball_roll_speed_after(fx v0, fx d) {
    fx left = ball_roll_distance(v0) - d;
    return left > 0 ? ball_roll_speed_for(left) : 0;
}

/* where a ball launched at (vh, vz) from the grass first lands (mirrors integrate()'s air branch + substeps) */
fx ball_carry_distance(fx vh, fx vz) {
    fx x = 0, z = 0;
    for (int t = 0; t < 600; t++) {
        fx sp = fv2_len(fv2_make(vh, vz));
        int n = 1 + (int)(fx_mul(sp, WP_DT) / T_SUBSTEP_MAX_MOVE);
        if (n > 12) n = 12;
        fx dt = WP_DT / n;
        for (int k = 0; k < n; k++) {
            fx s3 = fv2_len(fv2_make(vh, vz));
            fx kd = fx_min(fx_mul(fx_mul(T_AIR_DRAG, s3), dt), FX_ONE);
            vh -= fx_mul(vh, kd);
            vz -= fx_mul(vz, kd) + fx_mul(T_GRAVITY, dt);
            x += fx_mul(vh, dt);
            z += fx_mul(vz, dt);
            if (z <= 0) return x;
        }
    }
    return x;
}

fx ball_lob_speed_for(fx d, fx vz) {
    fx lo = 0, hi = FX(35.0);
    for (int it = 0; it < 16; it++) {
        fx mid = (lo + hi) / 2;
        if (ball_carry_distance(mid, vz) < d) lo = mid; else hi = mid;
    }
    return hi;
}

void ball_step(SimState *s) {
    WpBall *b = &s->ball;
    if (b->held) return;                                  /* in a keeper's hands: carried by control_owner, no physics */
    fx move = fx_mul(ball_speed3(b), WP_DT);
    int n = 1 + (int)(move / T_SUBSTEP_MAX_MOVE);
    if (n > 12) n = 12;
    fx dt = WP_DT / n;
    for (int k = 0; k < n; k++) {
        fv2 prev = b->pos;
        fx prev_z = b->z;
        integrate(s, dt);
        collide_frame(s);
        collide_bodies(s);
        lines_and_net(s, prev, prev_z, dt);
    }
}
