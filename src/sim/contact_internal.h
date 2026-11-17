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
/* D-048: a buffered intent at the aerial contact: volley / half-volley / volley pass / lay-off / volleyed lob
 * (1 = struck; 0 = control it, the intent stays) */
int  contact_first_time(SimState *s, int i, const WpSeatInput *in);
void contact_strike(SimState *s, int i, const WpSeatInput *in, uint8_t kind, uint16_t gauge);   /* SHOT / PASS / LOB */
/* intent.c (D-048): the intent buffer per seat */
void intent_press(SimState *s, int i, uint8_t kind);
void intent_release(SimState *s, int i, uint8_t kind, uint16_t gauge);
void intent_clear_seat(SimState *s, int seat);
void intent_clear_player(SimState *s, int i);
int  intent_of(const SimState *s, int i, uint16_t *gauge);
WpSeatInput intent_aim(const SimState *s, int i, const WpSeatInput *in);   /* stick, else the press's aim */   /* WP_KICK_* buffered for player i, -1 = none */
void intent_step(SimState *s, int i, const WpSeatInput *in);   /* per tick, after his buttons: cancel / fire */
/* player.c */
void player_fire(SimState *s, int i, uint8_t kind, uint16_t ticks, const WpSeatInput *in);
fv2  player_pass_solve(const SimState *s, int i, int r, fv2 *meet, fx *t_meet, uint8_t *plan);
/* the planned contact point, if he has a plan (movement: receive assist, AI receive / chase) */
static inline int contact_plan_at(const WpPlayer *p, fv2 *at) {
    if (p->plan.kind == WP_CK_NONE) return 0;
    *at = p->plan.at;
    return 1;
}
#endif
