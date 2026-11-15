/* header.c - playable contextual headers (D-010, CP0.2b). No header button: pressing PASS or SHOOT while a ball
 * is coming in above body height triggers a header. The player waits for the ball (and may still walk under it),
 * jumps at the right moment when it will be too high to reach standing, and contacts it in the HEAD band only.
 * Aim comes from the stick at the contact tick: PASS heads it to the best team-mate, SHOOT at goal (aim assist). */
#include "sim/sim_internal.h"

#define HEAD_Z        FX(1.70)   /* head centre above the feet */
#define HEAD_FWD      FX(0.12)
#define HEAD_REACH_XY FX(0.45)
#define HEAD_REACH_Z  FX(0.32)
#define WAIT_MAX      70         /* ticks a header attempt waits for the ball */

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

int header_wanted(const SimState *s, int i, int *jump) {
    const WpBall *b = &s->ball;
    const WpPlayer *p = &s->players[i];
    if (b->owner >= 0 || p->z > 0 || (b->z <= WP_BAND_BODY_MAX - FX(0.25) && b->vz <= 0)) return 0;
    fx t, z, miss;
    ball_approach(s, p->pos, &t, &z, &miss);
    if (t > FX(1.2) || miss > FX(2.0) || z < WP_BAND_BODY_MAX - FX(0.15)) return 0;
    *jump = z > HEAD_Z + FX(0.20);
    return 1;
}

void header_start(SimState *s, int i, int shot, int jump) {
    WpPlayer *p = &s->players[i];
    p->action = jump ? WP_ACT_JUMP_HEADER : WP_ACT_STAND_HEADER;
    p->action_timer = 0;
    p->kick.kind = WP_KICK_HEADER;
    p->kick.foot = (uint8_t)shot;            /* header sub-kind: 0 = pass, 1 = shot */
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
    fv2 head = fv2_add(p->pos, fv2_from_angle(p->facing, HEAD_FWD));
    /* missed: the ball has gone past (or someone has it) -> give up now, body collision back, free soon */
    fv2 to_head = fv2_sub(head, b->pos);
    if (b->owner >= 0 || (fv2_dot(to_head, b->vel) < 0 && fv2_len(to_head) > HEAD_REACH_XY)) {
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
    fv2 head = fv2_add(p->pos, fv2_from_angle(p->facing, HEAD_FWD));
    fx hz = p->z + HEAD_Z;
    fx d = fv2_len(fv2_sub(b->pos, head));
    if (fx_abs(b->z - hz) > HEAD_REACH_Z || d > HEAD_REACH_XY) return -1;
    return d + fx_abs(b->z - hz);
}

/* head contact by player i: direction from `in`'s stick (the human's, or the AI's aim) */
void header_contact(SimState *s, int i, const WpSeatInput *in) {
    WpPlayer *p = &s->players[i];
    WpBall *b = &s->ball;
    int has;
    fv2 aim = fv2_make((fx)in->move_x * (FX_ONE / 127), (fx)in->move_y * (FX_ONE / 127));
    has = in->move_x != 0 || in->move_y != 0;
    wp_angle dir = has ? fx_atan2(aim.y, aim.x) : p->facing;
    fx in_speed = ball_speed3(b);
    int to = -1;
    if (p->kick.foot) {                       /* header at goal: aim assist, firm and downward */
        dir = kick_aim_assist(s, i, dir, aim, has);
        b->vel = fv2_from_angle(dir, fx_min(T_HEADER_SHOT + fx_mul(in_speed, FX(0.25)), T_HEADER_SHOT_MAX));
        b->vz = T_HEADER_SHOT_VZ;
    } else {                                  /* header pass: cushioned toward the best team-mate */
        to = pass_pick(s, i, dir);
        fx d = FX(8.0);
        if (to >= 0) {
            fv2 dd = fv2_sub(s->players[to].pos, b->pos);
            dir = fx_atan2(dd.y, dd.x);
            d = fv2_len(dd);
        }
        b->vel = fv2_from_angle(dir, fx_clamp(fx_mul(d, FX(0.7)) + FX(3.0), FX(5.0), T_HEADER_PASS_MAX));
        b->vz = FX(1.8);
    }
    b->spin = 0;
    b->owner = -1;
    b->last_touch = (int8_t)i;
    b->no_hands = 0;                                      /* headers back to the keeper are allowed */
    p->immune = T_KICK_IMMUNE;
    p->touch_cooldown = 18;
    p->kick.dir = dir;
    p->kick.done = 1;
    if (p->action == WP_ACT_STAND_HEADER) p->kick.windup = (uint8_t)(p->action_timer + 1 > 255 ? 255 : p->action_timer + 1);
    s->events |= WP_EV_KICK | WP_EV_HEADER;
    if (to >= 0) seats_pass_handoff(s, i, to, in, s->players[to].pos, FX(1.0), WP_PLAN_CLOSEST);
}

/* While a header attempt waits for the ball, the stick only AIMS; the legs step under the ball's path. */
void header_assist_move(const SimState *s, int i, WpSeatInput *io) {
    const WpPlayer *p = &s->players[i];
    if (p->kick.kind != WP_KICK_HEADER || p->kick.done || p->z > 0) return;
    if (p->action != WP_ACT_JUMP_HEADER && p->action != WP_ACT_STAND_HEADER) return;
    fx t, z, miss;
    ball_approach(s, p->pos, &t, &z, &miss);
    fv2 meet = fv2_sub(fv2_add(s->ball.pos, fv2_scale(s->ball.vel, t)), fv2_from_angle(p->facing, HEAD_FWD));
    uint16_t buttons = io->buttons;
    team_steer(p, meet, FX(0.08), FX(0.8), io);
    io->buttons = buttons;
}

/* action end rule for header actions (called from air_and_timers) */
int header_action_ends(const WpPlayer *p) {
    if (p->action == WP_ACT_JUMP_HEADER) {
        int took_off = p->kick.kind != WP_KICK_HEADER || p->kick.windup;
        return (took_off && p->z == 0 && p->vz == 0 && p->action_timer > 4) || p->action_timer > WAIT_MAX + 45;
    }
    if (p->kick.kind == WP_KICK_HEADER && p->kick.done) return p->action_timer > (uint16_t)p->kick.windup + 12;
    return p->action_timer > WAIT_MAX;
}
