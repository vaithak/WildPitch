/* tackle.c - standing tackles (CP0.3). Contextual like headers: J (pass) pressed while an opponent has the ball
 * within T_TACKLE_START starts a lunge; T_TACKLE_WINDUP ticks later the tackling foot arrives (a point just ahead of
 * him, toward the ball). If the ball is within T_TACKLE_REACH of it, it is blocked and squirts back toward the
 * tackler, the carrier can't touch it for a moment and the tackler recovers at once; otherwise the tackle misses
 * and the tackler is committed (slowed) until the action ends - mistimed tackles cost you. No dice. */
#include "sim/sim_internal.h"

#define TACKLE_FOOT FX(0.55)

int tackle_wanted(const SimState *s, int i) {
    const WpBall *b = &s->ball;
    const WpPlayer *p = &s->players[i];
    if (b->owner < 0 || b->held || s->players[b->owner].team == p->team || p->z > 0 || b->z > WP_BAND_FEET_MAX) return 0;
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
    fx reach = fx_mul(T_TACKLE_REACH, FX_ONE + fx_mul(T_STAT_TACKLE, stat_k(s, i, WP_STAT_TACKLING)));   /* D-034 */
    if (fv2_len(fv2_sub(b->pos, foot)) > reach) return;                    /* missed */
    int victim = b->owner;
    /* a block tackle: the ball is stopped by the tackler's foot and squirts back his way (he moves with it) */
    fv2 from = victim >= 0 ? s->players[victim].pos : b->pos;
    fv2 back = fv2_sub(p->pos, from);
    fv2 push = fv2_add(fv2_scale(p->vel, FX(0.6)), fv2_scale(fv2_norm(back), T_TACKLE_BLOCK_V));
    if (victim >= 0) {
        s->players[victim].touch_cooldown = T_TACKLE_STUN;
        s->events |= WP_EV_TACKLE;
    }
    p->action_timer = (uint16_t)(T_TACKLE_TICKS - 6);   /* won it: recover at once (only a miss commits you) */
    p->touch_cooldown = 4;
    b->vel = push;
    b->spin = 0;
    b->owner = -1;
    b->last_touch = (int8_t)i;
    b->no_hands = 0;
    s->events |= WP_EV_TOUCH;
}
