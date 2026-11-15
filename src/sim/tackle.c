/* tackle.c - standing tackles (CP0.3). Contextual like headers: J (pass) pressed while an opponent has the ball
 * within T_TACKLE_START starts a lunge; T_TACKLE_WINDUP ticks later the tackling foot arrives (a point just ahead of
 * him, toward the ball). If the ball is within T_TACKLE_REACH of it, it is knocked loose (away from the carrier,
 * the way the tackler came in) and the carrier can't touch it for a moment; otherwise the tackle misses and the
 * tackler is committed (slowed) until the action ends - mistimed tackles cost you. No dice: geometry and timing. */
#include "sim/sim_internal.h"

#define TACKLE_FOOT FX(0.55)

int tackle_wanted(const SimState *s, int i) {
    const WpBall *b = &s->ball;
    const WpPlayer *p = &s->players[i];
    if (b->owner < 0 || s->players[b->owner].team == p->team || p->z > 0 || b->z > WP_BAND_FEET_MAX) return 0;
    return fv2_len(fv2_sub(b->pos, p->pos)) <= T_TACKLE_START;
}

void tackle_start(SimState *s, int i) {
    WpPlayer *p = &s->players[i];
    fv2 to = fv2_norm(fv2_sub(s->ball.pos, p->pos));
    p->action = WP_ACT_TACKLE;
    p->action_timer = 0;
    p->kick.done = 0;
    p->kick_charge = 0;
    p->charge_kind = 0xFF;
    p->facing = fx_atan2(to.y, to.x);
    p->vel = fv2_add(p->vel, fv2_scale(to, T_TACKLE_LUNGE));
}

void tackle_step(SimState *s, int i) {
    WpPlayer *p = &s->players[i];
    WpBall *b = &s->ball;
    if (p->action != WP_ACT_TACKLE || p->kick.done || p->action_timer + 1 < T_TACKLE_WINDUP) return;
    p->kick.done = 1;
    fv2 foot = fv2_add(p->pos, fv2_from_angle(p->facing, TACKLE_FOOT));
    if (s->phase != WP_PHASE_PLAY || b->z > WP_BAND_FEET_MAX) return;
    if (fv2_len(fv2_sub(b->pos, foot)) > T_TACKLE_REACH) return;          /* missed */
    int victim = b->owner;
    fv2 push = fv2_from_angle(p->facing, FX(4.0));                        /* poked on, the way he came in */
    if (victim >= 0) {
        s->players[victim].touch_cooldown = T_TACKLE_STUN;
        push = fv2_add(push, fv2_scale(s->players[victim].vel, FX(0.3)));
        s->events |= WP_EV_TACKLE;
    }
    b->vel = push;
    b->spin = 0;
    b->owner = -1;
    b->last_touch = (int8_t)i;
    s->events |= WP_EV_TOUCH;
}
