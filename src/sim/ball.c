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
        if (b->vz < -T_BOUNCE_MIN_VZ) {
            if (b->vz < -FX(1.5)) s->events |= WP_EV_BOUNCE;
            b->vz = -fx_mul(b->vz, T_BOUNCE_REST);
            b->vel = fv2_scale(b->vel, T_BOUNCE_KEEP);
        } else {
            b->vz = 0;
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
        if (b->z > p->z + T_BODY_H || b->z + T_BALL_R < p->z) continue;
        fx r = (p->role == WP_ROLE_KEEPER ? T_KEEPER_BODY_R : T_BODY_R) + T_BALL_R;
        fv2 d = fv2_sub(b->pos, p->pos);
        if (fx_abs(d.x) > r || fx_abs(d.y) > r) continue;
        fx dist = fv2_len(d);
        if (dist >= r || dist == 0) continue;
        fv2 rel = fv2_sub(b->vel, p->vel);
        if (s->phase == WP_PHASE_PLAY && player_can_control(s, i) && p->touch_cooldown == 0 &&
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
static void lines_and_net(SimState *s, fv2 prev, fx prev_z) {
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
            }
            b->owner = -1;
        }
        if (x1 <= hl) continue;
        /* net box: hl..hl+gd behind the line, |y| < gw2, z < gh. Soft walls from both sides. */
        int was_in = sg * prev.x > hl && fx_abs(prev.y) < gw2 && prev_z < gh;
        if (was_in) {
            fx k = FX_ONE - fx_min(fx_mul(T_NET_DRAG, WP_DT / 4), FX_ONE);   /* net drag (per substep, approx.) */
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
        s->phase = WP_PHASE_OUT;
        s->phase_timer = T_OUT_PAUSE_TICKS;
        s->events |= WP_EV_OUT;
        b->owner = -1;
    }
}

void ball_step(SimState *s) {
    WpBall *b = &s->ball;
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
        lines_and_net(s, prev, prev_z);
    }
}
