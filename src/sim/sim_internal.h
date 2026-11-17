/* sim_internal.h - private interfaces between sim modules (not for src/game).
 *   sim.c     orchestration: init, kickoff reset, match flow (goal/out phases), hashing
 *   player.c  movement, turning, buttons -> parametric kicks, ball control & dribbling
 *   ball.c    ball physics (substepped), posts/crossbar/net, bodies, goal & out detection
 *   seats.c   which player each human seat controls (legacy switching), pass in flight, assisted receive, chase cone
 *   switching.c  D-050 switching: intercept-ETA oracle, event gates, the button (direction / cycle), post-switch blend
 *   pass_pick.c  who receives a pass (stick cone + score, preview, stick memory)
 *   header.c  contextual headers: trigger, take-off timing, head-band contact
 *   restart.c throw-ins, corners, goal kicks: spot, taker, distance rule
 *   keeper.c  goalkeeper dives: catch / parry along the outstretched body
 *   tackle.c  standing tackles (human J when they have the ball nearby; AI later)
 *   team_plan.c  team layer: ball-winner, cover, capped zonal markers, rest defence, support, runner
 *   formation.c  ball-dependent formation spots (in / out of possession blocks)
 *   brain.c   instinct AI (D-008): observation -> candidate actions x utility -> input / kicks / tackles
 *   team.c    steering helpers (+ the old CP0.2 placeholder positioning) movement for non-human players (shape, runs, keepers) -> WpSeatInput (CP0.3: AI)
 */
#ifndef WP_SIM_INTERNAL_H
#define WP_SIM_INTERNAL_H
#include "sim/sim.h"
#include "sim/tuning.h"

void players_step(SimState *s, const WpInput *in);
void ball_step(SimState *s);
void match_place_kickoff(SimState *s);
static inline fx kickoff_r(const SimState *s) { return fx_mul(s->pitch.length, T_KICKOFF_R_FRAC); }   /* centre circle */
void formation_frac(int n_per_side, int k, fx *fxl, fx *fyl);   /* formation spot (sim.c) */

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
int ball_flight_to(fx vh, fx vz, fx z0, fx z_end, fx *dist, int *ticks);   /* ... from z0 down through z_end */
void ball_predict_tick(const WpPitch *pt, WpBall *b);   /* one tick of an untouched ball (no frame/bodies/lines) */

/* player.c helpers shared with header.c */
/* pass_pick.c */
enum { PICK_DIGITAL = 1, PICK_KEEPER = 2 };
int      pass_pick(const SimState *s, int i, wp_angle dir);        /* stateless (digital + keeper) */
int      pass_pick_ex(const SimState *s, int i, wp_angle dir, int flags, int prev, uint8_t *danger);
void     pass_pick_update(SimState *s, int seat, int i, const WpSeatInput *in);   /* per tick: preview */
void     pass_pick_reset(SimState *s, int seat);
fx       ray_exit_dist(fv2 from, fv2 dir, fx X, fx Y);
fx       pass_lane_clear(const SimState *s, int team, fv2 from, fv2 to);   /* nearest opponent to the lane (m) */
/* seats.c: where can p meet the ball (closest reachable point, or the stick-preferred one) */
typedef struct { fv2 pos; fx t; int found; } Meet;
Meet meet_choose(const SimState *s, const WpPlayer *p, fv2 pref, int has_pref);
/* team_plan.c / formation.c (D-030) */
void team_plan_reset(SimState *s);
void team_plan_update(SimState *s);
fv2  shape_spot(const SimState *s, int team, int k);   /* where formation slot k should stand now */
fx   slot_mindset(const SimState *s, int k);          /* 0 = stays back .. 1 = attacks */
/* brain.c (CP0.3 instinct AI, D-008): input + actions for a player no seat controls */
void brain_input(SimState *s, int i, WpSeatInput *out);
wp_angle kick_aim_assist(const SimState *s, int i, wp_angle dir, fv2 aim, int has_aim);
fx   brain_shot_quality(const SimState *s, int team, fv2 at);   /* 0..1 from `at` at the goal team attacks */
fv2  brain_shot_target(const SimState *s, int team);             /* the corner further from their keeper */
/* header.c */
int  header_wanted(const SimState *s, int i, int *jump);
int  header_wanted_at(const SimState *s, int i, int *jump, fx zmin);
void header_start(SimState *s, int i, int shot, int jump);
void header_step(SimState *s, int i);                 /* take-off timing, give up when missed */
fx   header_contact_dist(const SimState *s, int i);   /* head-ball distance if he can head it now, else -1 */
void header_contact(SimState *s, int i, const WpSeatInput *in);
int  header_action_ends(const WpPlayer *p);
void header_assist_move(const SimState *s, int i, WpSeatInput *io);

/* seats.c */
void seats_update(SimState *s, const WpInput *in);
void seats_pass_handoff(SimState *s, int kicker, int receiver, const WpSeatInput *in, fv2 meet, fx t_meet,
                        uint8_t plan);
int  seats_assist_receive(const SimState *s, int seat, WpSeatInput *io);   /* 1 if it steered the legs */
void seats_chase_assist(const SimState *s, int seat, WpSeatInput *io);     /* the chase cone (D-037) */
void seats_on_gain(SimState *s, int i, const WpInput *in);   /* a team-mate of a seat won the ball */
void seats_switch_to(SimState *s, int seat, int to, uint8_t cool, const WpSeatInput *si, int latch, uint8_t reason);
int  meet_path(const SimState *s, const fv2 **at, const uint8_t **high);   /* meet_choose's cached ball path */
/* switching.c (D-050): non-LEGACY seats */
uint16_t switch_eta(const SimState *s, int j);            /* intercept ETA (ticks) of player j */
int  switch_best(const SimState *s, int seat);            /* eligible team-mate with the smallest ETA, -1 */
void switch_seat(SimState *s, int seat, const WpSeatInput *si);   /* per tick: oracle, button, unprompted gate */
void switch_note(SimState *s, int seat, int from, uint8_t reason);   /* every switch's bookkeeping */
void switch_events(SimState *s, int ended_team);           /* the ball went loose: event windows */
void switch_blend(const SimState *s, int seat, WpSeatInput *io);    /* legs: post-switch blend */
/* tackle.c */
int  tackle_wanted(const SimState *s, int i);             /* an opponent has the ball within reach of a tackle */
void tackle_start(SimState *s, int i);
void tackle_step(SimState *s, int i);
int  slide_wanted(const SimState *s, int i);   /* D-043 */
void slide_start(SimState *s, int i);
void slide_step(SimState *s, int i);
/* restart.c (D-029) */
void restart_out(SimState *s, int touchline, fx x_or_y, int goal_side);
void restart_place(SimState *s, const WpInput *in);
void restart_foul(SimState *s, int team, fv2 spot, int indirect);
void restart_setup(SimState *s, const WpInput *in);        /* D-044: the walk into place */
void restart_setup_step(SimState *s, const WpInput *in);
int  restart_taker_walk(const SimState *s, int i, fv2 *dir, fx *speed);
int  restart_legal_target(const SimState *s, int j, fv2 *out);
int  restart_legal_point(const SimState *s, int j, fv2 pos, fv2 *out);   /* same, for a point he's heading to */   /* free kick / penalty to team (D-040/041) */
void restart_step(SimState *s);
int  restart_is_taker(const SimState *s, int i);
/* keeper.c */
void keeper_dive_start(SimState *s, int i, int side);
void keeper_step(SimState *s, int i, const WpInput *in);
/* player.c */
void bodies_separate(SimState *s);
/* team.c */
void team_input(const SimState *s, int i, WpSeatInput *out);
void team_steer(const WpPlayer *p, fv2 target, fx deadzone, fx slow_radius, WpSeatInput *out);
void team_steer_speed(const SimState *s, const WpPlayer *p, fv2 target, fx speed, WpSeatInput *out);   /* through a point at speed (m/s, his own top speeds) */

/* setpiece.c (CP0.3e): a receiving spot for player i at the pending/live throw-in or goal kick (1 = go there) */
int  setpiece_spot(const SimState *s, int i, fv2 *at);

/* offside.c (D-041) */
fx   offside_line(const SimState *s, int team);           /* attack-frame x a team-mate may not be beyond */
int  offside_position(const SimState *s, int j);
void offside_on_kick(SimState *s, int kicker);
void offside_step(SimState *s);

/* lob.c (D-045 meter, D-046 solve): lofted passes from one D-indexed flight table (filled once by sim_init) */
void lob_table_init(void);
fx   lob_max_d(const SimState *s);                         /* the meter's top end on this pitch */
fx   lob_meter_dist(const SimState *s, fx h);              /* hold h (0..1] -> how far the lofted ball lands */
int  lob_ramp_ticks(const SimState *s, int i);            /* ticks the lob meter takes to fill after the tap window */
fx   lob_recv_time(fx D, int head);                        /* a D lob: time / distance to come down through ... */
fx   lob_recv_dist(fx D, int head);                        /* ... T_LOB_RECV_Z (head: T_LOB_HEAD_Z) */
int  lob_snap(const SimState *s, int i, wp_angle dir, fx h);   /* meter release: the team-mate it snaps to, or -1 */
fv2  lob_solve(const SimState *s, int r, wp_angle dir, fx h, fx d_min, int head, fx *vz, fv2 *meet, fx *t_meet);
fv2  cross_solve(const SimState *s, int r, fx *vz, fv2 *meet, fx *t_meet);   /* AI driven cross into the box (D-051) */

/* clock.c (D-036) */
void clock_step(SimState *s);
void clock_add_stoppage(SimState *s, int game_seconds);

/* stats.c (D-034) */
void stats_init(SimState *s, uint8_t mode);
fx   stat_k(const SimState *s, int i, int which);           /* (stat - 50) / 50; 0 when stats are neutral */
fx   player_top_speed(const SimState *s, int i, int sprint);  /* THE top speed (pace, fatigue) */
void stamina_step(SimState *s, int i);                      /* energy drain / recovery (stats on) */

/* Is `at` inside `team`'s own penalty area? (depth 15% of the length, half-width 24% of the width; render matches) */
/* back-pass rule (D-032): a keeper uses his hands only in his own box and not on a ball his team-mate deliberately
 * kicked or threw to him (headers, chests and deflections are fine) */
/* which way team attacks along x (+1 / -1): team 0 attacks +x in the first half; sides change at half time (D-036) */
static inline int team_dir(const SimState *s, int team) { return ((team == 0) != (s->half == 1)) ? 1 : -1; }
static inline int in_own_box(const SimState *s, int team, fv2 at);
static inline int keeper_hands(const SimState *s, int i);
static inline int in_own_box(const SimState *s, int team, fv2 at) {
    fx hl = s->pitch.length / 2, depth = fx_mul(s->pitch.length, FX(0.15)), hw = fx_mul(s->pitch.width, FX(0.24));
    fx x = team_dir(s, team) > 0 ? at.x : -at.x;
    return x <= -hl + depth && x >= -hl - FX(0.5) && at.y >= -hw && at.y <= hw;
}
static inline int keeper_hands(const SimState *s, int i) {
    const WpPlayer *p = &s->players[i];
    return p->role == WP_ROLE_KEEPER && in_own_box(s, p->team, p->pos) && s->ball.no_hands != p->team + 1;
}
static inline fv2 clamp_own_box(const SimState *s, int team, fv2 at) {
    fx hl = s->pitch.length / 2, depth = fx_mul(s->pitch.length, FX(0.15)), hw = fx_mul(s->pitch.width, FX(0.24));
    fx x = team_dir(s, team) > 0 ? at.x : -at.x;
    x = fx_clamp(x, -hl + FX(0.3), -hl + depth - FX(0.2));
    return fv2_make(team_dir(s, team) > 0 ? x : -x, fx_clamp(at.y, -hw + FX(0.2), hw - FX(0.2)));
}

/* put the ball in keeper i's hands (chest height, in front of him); ball physics skips a held ball */
static inline void ball_to_hands(SimState *s, int i) {
    WpBall *b = &s->ball;
    const WpPlayer *p = &s->players[i];
    b->held = 1;
    b->pos = fv2_add(p->pos, fv2_from_angle(p->facing, FX(0.25)));
    b->z = FX(1.0); b->vz = 0; b->vel = p->vel; b->spin = 0;
    if (p->role != WP_ROLE_KEEPER && s->restart_kind == WP_RS_THROW) {   /* a throw-in: both hands, over his head */
        b->pos = fv2_add(p->pos, fv2_from_angle(p->facing, FX(0.2)));   /* just inside the line (he stands outside) */
        b->z = FX(2.0);                               /* between the raised hands of an average player */
    }
}

/* Can player i take control of a loose ball? Everyone in a real match (AI included, CP0.3); with
 * WP_SIMF_STATIC_OTHERS (drills / isolation tests) only seat players - the others are statues. */
static inline int player_can_control(const SimState *s, int i) {
    if (!(s->flags & WP_SIMF_STATIC_OTHERS)) return 1;
    for (int k = 0; k < WP_MAX_SEATS; k++) if (s->seat_player[k] == i) return 1;
    return 0;
}
#include "sim/contact_internal.h"

#endif
