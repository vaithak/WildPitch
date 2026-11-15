/* sim_internal.h - private interfaces between sim modules (not for src/game).
 *   sim.c     orchestration: init, kickoff reset, match flow (goal/out phases), hashing
 *   player.c  movement, turning, buttons -> parametric kicks, ball control & dribbling
 *   ball.c    ball physics (substepped), posts/crossbar/net, bodies, goal & out detection
 *   seats.c   which player each human seat controls (switching), pass in flight, assisted receive
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
fx ball_roll_distance(fx v);                 /* forward model: how far a ground ball at v rolls */

/* seats.c */
void seats_update(SimState *s, const WpInput *in);
void seats_pass_handoff(SimState *s, int kicker, int receiver, const WpSeatInput *in);
void seats_assist_receive(const SimState *s, int seat, WpSeatInput *io);
/* team.c */
void team_input(const SimState *s, int i, WpSeatInput *out);
void team_steer(const WpPlayer *p, fv2 target, fx deadzone, fx slow_radius, WpSeatInput *out);

/* Can player i take control of a loose ball? (CP0.1: only human-controlled players; AI joins in CP0.3) */
static inline int player_can_control(const SimState *s, int i) {
    for (int k = 0; k < WP_MAX_SEATS; k++) if (s->seat_player[k] == i) return 1;
    return 0;
}
#endif
