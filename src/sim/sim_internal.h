/* sim_internal.h - private interfaces between sim modules (not for src/game).
 *   sim.c     orchestration: init, kickoff reset, match flow (goal/out phases), hashing
 *   player.c  movement, turning, buttons -> parametric kicks, ball control & dribbling
 *   ball.c    ball physics (substepped), posts/crossbar/net, bodies, goal & out detection
 *   seats.c   which player each human seat controls (switching), pass in flight, assisted receive
 *   pass_pick.c  who receives a pass (stick cone + score, preview, stick memory)
 *   header.c  contextual headers: trigger, take-off timing, head-band contact
 *   tackle.c  standing tackles (human J when they have the ball nearby; AI later)
 *   team.c    placeholder movement for non-human players (shape, runs, keepers) -> WpSeatInput (CP0.3: AI)
 */
#ifndef WP_SIM_INTERNAL_H
#define WP_SIM_INTERNAL_H
#include "sim/sim.h"
#include "sim/tuning.h"

void players_step(SimState *s, const WpInput *in);
void ball_step(SimState *s);
void match_place_kickoff(SimState *s);

/* signed shortest difference a - b between binary angles */
static inline int32_t ang_diff(wp_angle a, wp_angle b) { return (int16_t)(uint16_t)(a - b); }
#define DEG_TO_ANG(d) ((int32_t)((d) * 65536 / 360))

/* 3D speed of the ball */
static inline fx ball_speed3(const WpBall *b) { return fv2_len(fv2_make(fv2_len(b->vel), b->vz)); }

/* rolling deceleration (m/s^2) of a ground ball at speed v (shared by ball physics and dribble touch sizing) */
static inline fx ball_roll_decel(fx v) { return T_ROLL_DECEL + fx_mul(T_ROLL_LIN, v) + fx_mul(T_AIR_DRAG, fx_mul(v, v)); }

/* Ground ball: launch speed that makes it roll exactly distance d before stopping (inverse of the roll model,
 * by bisection over the same per-tick integration ball.c uses). Used for stopping touches; passes later (CP0.2). */
fx ball_roll_speed_for(fx d);
fx ball_roll_distance(fx v);                 /* R(v): how far a ground ball at v rolls (table, D-020) */
fx ball_roll_time(fx v);                     /* T(v): seconds until it stops; flight v0->va = T(v0) - T(va) */
fx ball_roll_speed_after(fx v0, fx d);       /* speed after rolling d from v0 (0 if it stops first) */

fx ball_lob_speed_for(fx d, fx vz);          /* horizontal launch speed so a ball launched with vz lands at d */
fx ball_carry_distance(fx vh, fx vz);        /* forward model: where (horizontally) it first lands */

/* player.c helpers shared with header.c */
/* pass_pick.c */
enum { PICK_DIGITAL = 1, PICK_KEEPER = 2 };
int      pass_pick(const SimState *s, int i, wp_angle dir);        /* stateless (digital + keeper) */
int      pass_pick_ex(const SimState *s, int i, wp_angle dir, int flags, int prev, uint8_t *danger);
void     pass_pick_update(SimState *s, int seat, int i, const WpSeatInput *in);   /* per tick: preview */
void     pass_pick_reset(SimState *s, int seat);
fx       ray_exit_dist(fv2 from, fv2 dir, fx X, fx Y);
wp_angle kick_aim_assist(const SimState *s, int i, wp_angle dir, fv2 aim, int has_aim);
/* header.c */
int  header_wanted(const SimState *s, int i, int *jump);
void header_start(SimState *s, int i, int shot, int jump);
void header_step(SimState *s, int i, const WpSeatInput *in);
int  header_action_ends(const WpPlayer *p);
void header_assist_move(const SimState *s, int i, WpSeatInput *io);

/* seats.c */
void seats_update(SimState *s, const WpInput *in);
void seats_pass_handoff(SimState *s, int kicker, int receiver, const WpSeatInput *in, fv2 meet, fx t_meet,
                        uint8_t plan);
void seats_assist_receive(const SimState *s, int seat, WpSeatInput *io);
void seats_on_gain(SimState *s, int i, const WpInput *in);   /* a team-mate of a seat won the ball */
/* tackle.c */
int  tackle_wanted(const SimState *s, int i);             /* an opponent has the ball within reach of a tackle */
void tackle_start(SimState *s, int i);
void tackle_step(SimState *s, int i);
/* player.c */
void bodies_separate(SimState *s);
/* team.c */
void team_input(const SimState *s, int i, WpSeatInput *out);
void team_steer(const WpPlayer *p, fv2 target, fx deadzone, fx slow_radius, WpSeatInput *out);
void team_steer_speed(const WpPlayer *p, fv2 target, fx speed, WpSeatInput *out);   /* through a point at speed */

/* Can player i take control of a loose ball? Everyone in a real match (AI included, CP0.3); with
 * WP_SIMF_STATIC_OTHERS (drills / isolation tests) only seat players - the others are statues. */
static inline int player_can_control(const SimState *s, int i) {
    if (!(s->flags & WP_SIMF_STATIC_OTHERS)) return 1;
    for (int k = 0; k < WP_MAX_SEATS; k++) if (s->seat_player[k] == i) return 1;
    return 0;
}
#endif
