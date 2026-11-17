/* contact_internal.h - aerial contact planner + control touches (contact.c, D-047). Included by sim_internal.h. */
#ifndef WP_CONTACT_INTERNAL_H
#define WP_CONTACT_INTERNAL_H

/* per tick, before anyone moves: the ball prediction (only while the loose ball is in the air) and the contact plans
 * of the players who matter (the pass's receiver, seat players, each team's ball-winner) */
void contact_update(SimState *s);
/* the stance player i would use on the loose ball as it is now (WP_CK_*), 0 = he can't control it (it deflects).
 * One predicate for the first touch (gain_check), the body (collide_bodies) and possession (control_owner). */
int  contact_stance(const SimState *s, int i, const WpBall *b);
fx   contact_reach(int kind);                       /* how close (xy) the ball must pass for that stance */
int  contact_waiting(const SimState *s, int i, fx dseg);   /* 1 = let it drop to the planned (later) contact */
/* an aerial control touch (z > T_CB_GROUND): outcome + residual model; sets the ball and his settle_ticks */
void contact_touch(SimState *s, int i, const WpSeatInput *in, fx dseg);
/* the planned contact point, if he has a plan (movement: receive assist, AI receive / chase) */
static inline int contact_plan_at(const WpPlayer *p, fv2 *at) {
    if (p->plan.kind == WP_CK_NONE) return 0;
    *at = p->plan.at;
    return 1;
}
#endif
