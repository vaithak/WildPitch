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
void contact_mark(SimState *s, int i, uint8_t kind);       /* his last contact (WP_LT_*) and its tick, for the poses */
const WpBall *contact_ring(const SimState *s, int t);      /* the predicted ball t ticks ahead, NULL = none */
int  contact_earliest(const SimState *s, int i, fx zlo, fx zhi, int tmax, fv2 *at, fx *z);   /* D-051 */
int  contact_reachable(const SimState *s, int i, fx zlo, fx zhi, int tmax, int nearest, fv2 *at, fx *z);
fx   contact_pressure(const SimState *s, int team, fv2 at);   /* 1 = an opponent within 1.5 m .. 0 beyond 4 m */
fx   contact_plan_difficulty(const SimState *s, int i);        /* his planned touch's difficulty, -1 = no plan */
fv2  contact_stand_at(const SimState *s, int i);                /* where to stand for it (a little beyond the point) */
/* aerial_ai.c (D-051): the AI's choice at a ball coming in the air (brain.c carries it out) */
typedef struct {
    uint8_t act;                 /* AA_* (aerial_ai.c) */
    uint8_t head;                /* 1 = attack it with the head at the earliest point; 0 = his planned foot/body contact */
    uint8_t intent;              /* WP_KICK_* + 1 at the contact (header sub-kind follows from it), 0 = control */
    int8_t  mate;                /* the team-mate it goes to, -1 */
    fv2     aim, to;             /* where it is sent / where he goes */
    fx      speed;               /* how fast he goes there (m/s) */
    int     t;                   /* ticks to that contact */
    fx      z;                   /* the ball's height there */
} AirChoice;
int  aerial_choose(const SimState *s, int i, AirChoice *c);   /* 0 = no aerial contact for him */
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
