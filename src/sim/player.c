/* player.c - player movement, turning, buttons -> parametric kicks (D-012), ball control & dribbling.
 *
 * Feel rules (see tests/test_ball_control.c):
 *  - Dribble touches follow the STICK direction, not the slower-turning body, so cuts feel immediate.
 *  - An owned ball is "gathered" during a kick wind-up: a kick by the owner can never whiff.
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

void sim_player_kick(SimState *s, int i, const WpKick *k) {
    WpPlayer *p = &s->players[i];
    if (p->action != WP_ACT_NONE || p->z > 0) return;
    p->kick = *k;
    if (p->kick.windup < 1) p->kick.windup = 1;
    p->kick.done = 0;
    p->action = WP_ACT_KICK;
    p->action_timer = 0;
}

/* Build kick parameters from a human's input. charge01 in [0,1]. */
static void make_kick(const SimState *s, int i, uint8_t kind, fx charge01, fv2 aim, int has_aim, WpKick *k) {
    const WpPlayer *p = &s->players[i];
    wp_angle dir = has_aim ? fx_atan2(aim.y, aim.x) : p->facing;
    k->kind = kind;
    k->spin = 0;
    if (kind == WP_KICK_PASS) {
        k->power = T_PASS_SPEED; k->vz = 0; k->windup = T_KICK_WINDUP_PASS;
    } else if (kind == WP_KICK_SHOT) {
        k->power = fx_lerp(T_SHOT_MIN, T_SHOT_MAX, charge01);
        k->vz = fx_lerp(T_SHOT_VZ_MIN, T_SHOT_VZ_MAX, fx_mul(charge01, charge01));
        k->windup = T_KICK_WINDUP_SHOT;
    } else {
        k->power = fx_lerp(T_CHIP_MIN, T_CHIP_MAX, charge01);
        k->vz = fx_lerp(T_CHIP_VZ_MIN, T_CHIP_VZ_MAX, charge01);
        k->windup = T_KICK_WINDUP_CHIP;
    }
    if (kind != WP_KICK_PASS) {
        /* Aim assist toward the opponent goal (8-way keyboard can't otherwise hit a 4 m goal).
         * Stick up/down picks the far side of the goal; straight picks the middle. */
        int sg = p->team == 0 ? 1 : -1;
        fx gy = 0;
        if (has_aim && aim.y > FX(0.25)) gy = fx_mul(s->pitch.goal_width, FX(0.32));
        if (has_aim && aim.y < -FX(0.25)) gy = -fx_mul(s->pitch.goal_width, FX(0.32));
        fv2 to_goal = fv2_sub(fv2_make(sg * (s->pitch.length / 2), gy), p->pos);
        wp_angle ga = fx_atan2(to_goal.y, to_goal.x);
        int32_t diff = ang_diff(ga, dir), lim = DEG_TO_ANG(T_AIM_ASSIST_DEG);
        if (diff > -2 * lim && diff < 2 * lim) dir = (wp_angle)(dir + (diff > lim ? lim : (diff < -lim ? -lim : diff)));
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

static void contact(SimState *s, int i) {
    WpPlayer *p = &s->players[i];
    WpBall *b = &s->ball;
    p->kick.done = 1;
    if (s->phase != WP_PHASE_PLAY || !ball_reachable(s, i, KICK_REACH)) return;   /* whiff */
    if (b->owner == i) b->pos = fv2_add(p->pos, fv2_from_angle(p->kick.dir, FOOT_SPOT));
    b->vel = fv2_add(fv2_from_angle(p->kick.dir, p->kick.power), fv2_scale(p->vel, INHERIT_VEL));
    b->vz = p->kick.vz;
    b->spin = p->kick.spin;
    b->owner = -1;
    b->last_touch = (int8_t)i;
    p->immune = T_KICK_IMMUNE;
    p->touch_cooldown = 18;
    s->events |= WP_EV_KICK;
}

static void handle_buttons(SimState *s, int i, const WpSeatInput *in, uint16_t prev) {
    WpPlayer *p = &s->players[i];
    uint16_t now = in->buttons, pressed = now & (uint16_t)~prev, released = prev & (uint16_t)~now;
    int has;
    fv2 aim = stick_dir(in, &has);
    if (s->phase != WP_PHASE_PLAY) { p->kick_charge = 0; p->charge_kind = 0xFF; return; }

    if ((pressed & WP_BTN_PASS) && p->action == WP_ACT_NONE && ball_reachable(s, i, KICK_START_REACH)) {
        WpKick k; make_kick(s, i, WP_KICK_PASS, 0, aim, has, &k);
        sim_player_kick(s, i, &k);
    }
    for (int c = 0; c < 2; c++) {                         /* shoot and chip share the charge mechanic */
        uint16_t btn = c == 0 ? WP_BTN_SHOOT : WP_BTN_CHIP;
        uint8_t kind = c == 0 ? WP_KICK_SHOT : WP_KICK_CHIP;
        if ((pressed & btn) && p->charge_kind == 0xFF) { p->charge_kind = kind; p->kick_charge = 0; }
        if ((now & btn) && p->charge_kind == kind && p->kick_charge < T_CHARGE_MAX_TICKS) p->kick_charge++;
        if ((released & btn) && p->charge_kind == kind) {
            if (p->action == WP_ACT_NONE && ball_reachable(s, i, KICK_START_REACH)) {
                WpKick k;
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
    /* dribbling slows you only while the ball is at your feet; running onto a knock-on is full speed */
    if (s->ball.owner == i && fv2_len(fv2_sub(s->ball.pos, p->pos)) <= T_TOUCH_REACH) top = fx_mul(top, T_DRIBBLE_SPEED_MUL);
    int winding = p->action == WP_ACT_KICK && !p->kick.done;
    if (winding) top = fx_mul(top, T_KICK_SPEED_MUL);
    if (has && s->ball.owner == i) {
        /* Reversing on a ball that's out of reach: go and get it first, then turn with it.
         * (Otherwise sprint knock-ons + a 180 would simply abandon the ball.) */
        fv2 to_ball = fv2_sub(s->ball.pos, p->pos);
        if (fv2_len(to_ball) > T_TOUCH_REACH && fv2_dot(to_ball, dir) < 0) dir = fv2_norm(to_ball);
    }
    if (has) {                                            /* analogue magnitude scales speed */
        fx mag = fx_min(fv2_len(fv2_make((fx)in->move_x * (FX_ONE / 127), (fx)in->move_y * (FX_ONE / 127))), FX_ONE);
        top = fx_mul(top, mag);
    }
    fv2 dv = fv2_sub(fv2_scale(dir, top), p->vel);
    p->vel = fv2_add(p->vel, fv2_clamp_len(dv, fx_mul(has ? T_ACCEL : T_STOP_DECEL, WP_DT)));

    /* body turn rate (the ball follows the stick; the body catches up) */
    wp_angle want = p->facing;
    if (winding) want = p->kick.dir;
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
        if (p->action == WP_ACT_KICK && !p->kick.done) {   /* gather: settle ball at the foot for contact */
            fv2 spot = fv2_add(p->pos, fv2_from_angle(p->kick.dir, FOOT_SPOT));
            b->vel = fv2_add(p->vel, fv2_scale(fv2_sub(spot, b->pos), FX(8.0)));
            b->vz = 0; b->z = 0;
            return;
        }
        if (b->z > WP_BAND_FEET_MAX || p->action != WP_ACT_NONE) return;
        if (!has) {                                        /* no stick: stop the ball under the sole / at the feet */
            if (p->touch_cooldown) return;
            fv2 spot = fv2_add(p->pos, fv2_from_angle(p->facing, FX(0.4)));
            b->vel = fv2_add(p->vel, fv2_scale(fv2_sub(spot, b->pos), FX(3.0)));
            b->spin = 0;
            p->touch_cooldown = 4;
            return;
        }
        if (dist > T_TOUCH_REACH) return;                  /* must actually reach the ball with a foot */
        fv2 mdir = sdir;
        fx ahead = fv2_dot(d, mdir);
        fv2 bdir = fv2_norm(b->vel);
        int lagging = ahead < T_TOUCH_AHEAD;
        int wrong_way = fv2_dot(bdir, mdir) < FX(0.8);
        /* a deliberate change of direction may cut the touch rhythm short */
        if (p->touch_cooldown && !(wrong_way && p->touch_cooldown <= T_TOUCH_GAP_WALK - T_TOUCH_MIN_GAP)) return;
        if (lagging || wrong_way) {
            int sprint = (in->buttons & WP_BTN_SPRINT) != 0;
            /* push relative to how fast the player is going the NEW way: a cut or reversal is a soft touch,
             * a straight run knocks it further (more so when sprinting) */
            fx along = fx_max(fv2_dot(p->vel, mdir), 0);
            fx push = fx_max(fx_mul(along, sprint ? T_TOUCH_PUSH_SPRINT : T_TOUCH_PUSH), T_TOUCH_MIN_SPEED);
            fv2 lat = fv2_sub(d, fv2_scale(mdir, ahead));  /* sideways offset from the running line */
            b->vel = fv2_sub(fv2_scale(mdir, push), fv2_scale(lat, FX(4.0)));
            b->spin = 0;
            if (b->vz > 0) b->vz = 0;
            p->touch_cooldown = (uint16_t)(sprint ? T_TOUCH_GAP_SPRINT : T_TOUCH_GAP_WALK);
            b->last_touch = (int8_t)i;
            s->events |= WP_EV_TOUCH;
        }
        return;
    }
    /* gain control of a loose ball */
    if (b->owner >= 0 || p->touch_cooldown || p->action != WP_ACT_NONE || p->z > 0) return;
    if (dist > T_CONTROL_REACH || b->z > WP_BAND_BODY_MAX) return;
    fv2 rel = fv2_sub(b->vel, p->vel);
    fx rs = fv2_len(rel);
    if (rs > T_CONTROL_MAX_REL) return;                    /* too hot: body collision deflects it */
    /* first touch: kills most of the relative speed; harder balls spill further (visible cause) */
    fx spill = fx_mul(fx_div(rs, T_CONTROL_MAX_REL), FX(0.45));
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
        int ends = p->action == WP_ACT_JUMP_HEADER ? (p->z == 0 && p->vz == 0 && p->action_timer > 4)
                 : p->action == WP_ACT_KICK ? p->action_timer >= T_KICK_TOTAL_TICKS
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
}

void players_step(SimState *s, const WpInput *in) {
    static const WpSeatInput idle = {0, 0, 0};
    int np = 2 * s->n_per_side;
    for (int i = 0; i < np; i++) {
        int seat = seat_of(s, i);
        const WpSeatInput *si = seat >= 0 ? &in->seat[seat] : &idle;
        if (seat >= 0) handle_buttons(s, i, si, s->prev_buttons[seat]);
        move(s, i, si);
        WpPlayer *p = &s->players[i];
        p->pos = fv2_add(p->pos, fv2_scale(p->vel, WP_DT));
        if (p->action == WP_ACT_KICK && !p->kick.done && p->action_timer + 1 >= p->kick.windup) contact(s, i);
        air_and_timers(p);
    }
    for (int i = 0; i < np; i++) {
        int seat = seat_of(s, i);
        if (seat >= 0) control(s, i, &in->seat[seat]);
    }
}
