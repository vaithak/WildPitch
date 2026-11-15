/* sim.h - the deterministic match simulation: public contract.
 *
 * Invariants (docs/ARCHITECTURE.md):
 *  - SimState is plain data: no pointers, no floats. Copying it = a full snapshot (save/rewind/resume).
 *  - sim_step(state, input) is a pure function of (state, input): same inputs -> same hash on every machine.
 *  - 60 ticks per second (WP_TICK_HZ). Rendering interpolates between ticks; it never writes to SimState.
 *  - Coordinates: origin = pitch centre; +x towards team 1's goal (team 0 attacks +x in the first half),
 *    +y across the pitch, z = height above grass. 1.0 = 1 metre.
 */
#ifndef WP_SIM_H
#define WP_SIM_H

#include <stdint.h>
#include "sim/fixed.h"
#include "sim/rng.h"

#define WP_TICK_HZ       60
#define WP_DT            (FX_ONE / WP_TICK_HZ)   /* one tick in seconds, Q16.16 */
#define WP_MAX_PER_SIDE  11
#define WP_MAX_PLAYERS   (2 * WP_MAX_PER_SIDE)
#define WP_MAX_SEATS     8                       /* human seats (pitch or touchline) */

/* ---- Input: one record per seat per tick (this is what replays / netcode transmit) ---- */
enum {
    WP_BTN_PASS   = 1 << 0,
    WP_BTN_SHOOT  = 1 << 1,
    WP_BTN_SPRINT = 1 << 2,
    WP_BTN_SWITCH = 1 << 3,
    WP_BTN_SHOUT  = 1 << 4,
    WP_BTN_CHIP   = 1 << 5,  /* chip: hold to charge, release (like shoot) */
    WP_BTN_LOB    = 1 << 6,  /* lofted pass to the best team-mate toward the stick */
};

typedef struct {
    int8_t   move_x, move_y;   /* -127..127 stick, already dead-zoned by the platform layer */
    uint16_t buttons;          /* WP_BTN_* currently held */
} WpSeatInput;

typedef struct {
    WpSeatInput seat[WP_MAX_SEATS];
} WpInput;

/* ---- World ---- */
typedef struct {
    fx length, width;            /* playing area, metres */
    fx goal_width, goal_height;
    fx goal_depth;
} WpPitch;

typedef struct {
    fv2 pos;  fx z;              /* z >= 0 */
    fv2 vel;  fx vz;
    fx  spin;                    /* sidespin, rad/s-ish; curls the ball (Q16.16) */
    int8_t owner;                /* player index dribbling it, or -1 */
    int8_t last_touch;           /* player index, or -1 */
} WpBall;

/* Player actions drive both gameplay timing and the avatar's animation pose (so replays animate exactly).
 * Headers (DECISIONS D-010): contextual — pressing pass/shoot while a reachable ball is in the air triggers
 * WP_ACT_JUMP_HEADER; ball contact is resolved in height bands (feet / body / head), never 2D distance alone. */
enum {
    WP_ACT_NONE = 0,
    WP_ACT_KICK,          /* shot / pass wind-up + follow-through */
    WP_ACT_JUMP_HEADER,   /* airborne, head contact window around the apex */
    WP_ACT_STAND_HEADER,  /* header without leaving the ground (ball at head height) */
    WP_ACT_TACKLE,        /* standing tackle / shoulder challenge (CP0.3) */
    WP_ACT_SLIDE,         /* sliding tackle (CP0.3+) */
    WP_ACT_KEEPER_DIVE,   /* CP0.3 */
    WP_ACT_COUNT
};

/* Contact height bands (metres) for ball control; see D-010. */
#define WP_BAND_FEET_MAX  FX(0.55)
#define WP_BAND_BODY_MAX  FX(1.45)
#define WP_BAND_HEAD_MAX  FX(2.05)   /* standing head reach; jumping adds the player's z */
#define WP_JUMP_VZ        FX(3.4)    /* ~0.59 m jump apex */

/* A kick is a parametric move (DECISIONS D-012): what it does to the ball + when contact happens.
 * The avatar reads the same fields (foot, windup) so the visible strike lands on the contact tick. */
enum { WP_KICK_PASS = 0, WP_KICK_SHOT, WP_KICK_CHIP, WP_KICK_HEADER, WP_KICK_THROUGH, WP_KICK_LOB };
typedef struct {
    fx       power;              /* horizontal ball speed at contact, m/s */
    fx       vz;                 /* vertical ball speed at contact, m/s */
    fx       spin;               /* sidespin (curl); + bends counter-clockwise */
    wp_angle dir;                /* direction of the strike */
    uint8_t  foot;               /* 0 = left, 1 = right */
    uint8_t  kind;               /* WP_KICK_* */
    uint8_t  windup;             /* ticks from action start to contact */
    uint8_t  done;               /* contact resolved (struck or missed) */
    uint8_t  to;                 /* pass receiver = player index + 1 (0 = untargeted, so zero-init is safe);
                                    a targeted pass is aimed and weighted at the contact tick */
} WpKick;

typedef struct {
    fv2 pos, vel;
    fx  z, vz;                   /* feet height above grass (jumping), vertical speed */
    wp_angle facing;
    uint8_t action;              /* WP_ACT_* */
    uint16_t action_timer;       /* ticks since the action started */
    uint8_t team;                /* 0 or 1 */
    uint8_t role;                /* WP_ROLE_* */
    uint16_t kick_charge;        /* ticks the shoot/chip button has been held */
    uint8_t  charge_kind;        /* WP_KICK_PASS (tap/through) / SHOT / CHIP while its button is held, else 0xFF */
    uint8_t  immune;             /* ticks this player's body ignores the ball (just kicked it) */
    uint16_t touch_cooldown;     /* ticks before this player may touch the ball again */
    fx stamina;                  /* 0..1 */
    WpKick kick;                 /* current/last kick parameters */
    /* instinct AI decision state (brain.c): what he's doing and where; re-decided every T_AI_DECIDE ticks */
    uint8_t  ai_act;             /* AI_* */
    int8_t   ai_mate;            /* player the action is about (mark / pass target), -1 none */
    uint8_t  ai_wait;            /* ticks until the next decision */
    uint8_t  ai_hold;            /* ticks he has had the ball (AI carriers don't act on the first touch) */
    fv2      ai_goal;            /* where he is going */
} WpPlayer;

enum { WP_ROLE_KEEPER = 0, WP_ROLE_OUTFIELD = 1 };

enum { WP_PHASE_KICKOFF = 0, WP_PHASE_PLAY = 1, WP_PHASE_GOAL = 2, WP_PHASE_OUT = 3 };

/* Per-tick event flags (cleared every tick). Presentation ORs them across the ticks of a frame to drive
 * sound/juice and to skip interpolation on teleports (WP_EV_RESET). */
enum {
    WP_EV_KICK   = 1 << 0,
    WP_EV_TOUCH  = 1 << 1,
    WP_EV_BOUNCE = 1 << 2,
    WP_EV_POST   = 1 << 3,
    WP_EV_NET    = 1 << 4,
    WP_EV_GOAL   = 1 << 5,
    WP_EV_OUT    = 1 << 6,
    WP_EV_RESET  = 1 << 7,   /* players/ball teleported: don't interpolate this tick */
    WP_EV_BODY   = 1 << 8,   /* ball hit a player's body */
    WP_EV_HEADER = 1 << 9,   /* head contact (also sets WP_EV_KICK) */
    WP_EV_TACKLE = 1 << 10,  /* a tackle won the ball off its owner */
};

enum { WP_SIMF_STATIC_OTHERS = 1 << 0 };   /* players no seat controls stand still (statues / cones) */
/* how an assisted receiver plays a pass: step to the closest reachable point (standing/checking receivers, lofts),
 * run through a planned meet at just-in-time speed (runners; in-stride through balls keep >= 4.5 m/s) */
enum { WP_PLAN_CLOSEST = 0, WP_PLAN_RUN = 1, WP_PLAN_STRIDE = 2 };

typedef struct {
    uint32_t tick;
    WpRng    rng;
    WpPitch  pitch;
    WpBall   ball;
    uint8_t  n_per_side;
    uint8_t  phase;
    uint16_t phase_timer;            /* ticks remaining in the current phase (e.g. goal celebration) */
    uint8_t  score[2];
    uint8_t  kickoff_team;           /* team taking the next kickoff */
    uint8_t  flags;                  /* WP_SIMF_* (drills / isolation tests) */
    uint16_t events;                 /* WP_EV_* raised during the last sim_step */
    int8_t   seat_player[WP_MAX_SEATS]; /* player index each seat controls, -1 = none */
    uint16_t prev_buttons[WP_MAX_SEATS];/* for edge detection inside the sim */
    uint8_t  switch_cool[WP_MAX_SEATS]; /* ticks before this seat may auto-switch again */
    int8_t   pass_target;            /* receiver of the pass in flight (-1 none): no auto-switch meanwhile */
    int8_t   pass_from;              /* who struck it */
    uint16_t pass_ticks;             /* age of the pass in flight */
    int8_t   pass_stick[2];          /* receiver's stick at the hand-off; -128 = player has taken over */
    fv2      pass_meet;              /* planned meeting point (receiver's feet) for a pass to a runner (D-020) */
    uint16_t pass_meet_tick;         /* ... and when */
    uint8_t  pass_plan;              /* WP_PLAN_*: how the assisted receiver plays it */
    uint8_t  recv_away;              /* ticks the receiver's stick has pointed away from the meet (takeover rule) */
    uint8_t  recv_gone;              /* ticks with no reachable meeting point (ball has gone) */
    fv2      recv_goal;              /* where the assisted receiver is steering this tick */
    fx       recv_speed;             /* ... and how fast (m/s) */
    int8_t   latch_stick[WP_MAX_SEATS][2]; /* stick held at an auto-switch: treated as neutral until it changes */
    uint8_t  latch_ticks[WP_MAX_SEATS];
    /* pass receiver preview (D-021): the sim owns the pick, the HUD only draws it, so the ring always matches */
    int8_t   pick_recv[WP_MAX_SEATS];    /* who a pass would go to right now (-1 = nobody: a ball into space) */
    uint8_t  pick_danger[WP_MAX_SEATS];  /* 0..255: an opponent near that passing lane (ring tint) */
    wp_angle pick_dir[WP_MAX_SEATS];     /* the effective pass direction (stick, latched diagonal, memory, facing) */
    int8_t   pick_stick[WP_MAX_SEATS][2];/* last non-zero stick ... */
    uint8_t  pick_stick_age[WP_MAX_SEATS];/* ... and ticks since (255 = none) */
    int8_t   pick_diag[WP_MAX_SEATS][2]; /* last keyboard diagonal ... */
    uint8_t  pick_diag_age[WP_MAX_SEATS];
    WpPlayer players[WP_MAX_PLAYERS];
} SimState;

typedef struct {
    uint64_t seed;
    uint8_t  n_per_side;             /* 1..WP_MAX_PER_SIDE */
    WpPitch  pitch;
} WpMatchConfig;

WpMatchConfig sim_default_config(void);   /* 5-a-side, 40 x 25 m (the regression-test format) */
WpMatchConfig sim_config_7v7(void);       /* 7-a-side, 55 x 35 m, 5 m goals (the game's default) */
/* Start a jump for player i (no-op if already airborne). Gameplay triggers come in CP0.2. */
void     sim_player_jump(SimState *s, int i, uint8_t action);
/* Start a kick action with explicit parameters (used by AI later, tests and --demo). No-op if busy. */
void     sim_player_kick(SimState *s, int i, const WpKick *k);
void     sim_init(SimState *s, const WpMatchConfig *cfg);
void     sim_step(SimState *s, const WpInput *in);
uint64_t sim_hash(const SimState *s);   /* field-wise FNV-1a; independent of struct padding */

#endif
