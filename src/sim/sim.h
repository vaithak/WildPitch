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
    WP_BTN_CHIP   = 1 << 5,  /* modifier */
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

typedef struct {
    fv2 pos, vel;
    fx  z, vz;                   /* feet height above grass (jumping), vertical speed */
    wp_angle facing;
    uint8_t action;              /* WP_ACT_* */
    uint16_t action_timer;       /* ticks since the action started */
    uint8_t team;                /* 0 or 1 */
    uint8_t role;                /* WP_ROLE_* */
    uint16_t kick_charge;        /* ticks the kick button has been held */
    uint16_t touch_cooldown;     /* ticks before this player may touch the ball again */
    fx stamina;                  /* 0..1 */
} WpPlayer;

enum { WP_ROLE_KEEPER = 0, WP_ROLE_OUTFIELD = 1 };

enum { WP_PHASE_KICKOFF = 0, WP_PHASE_PLAY = 1, WP_PHASE_GOAL = 2 };

typedef struct {
    uint32_t tick;
    WpRng    rng;
    WpPitch  pitch;
    WpBall   ball;
    uint8_t  n_per_side;
    uint8_t  phase;
    uint16_t phase_timer;            /* ticks remaining in the current phase (e.g. goal celebration) */
    uint8_t  score[2];
    int8_t   seat_player[WP_MAX_SEATS]; /* player index each seat controls, -1 = none */
    uint16_t prev_buttons[WP_MAX_SEATS];/* for edge detection inside the sim */
    WpPlayer players[WP_MAX_PLAYERS];
} SimState;

typedef struct {
    uint64_t seed;
    uint8_t  n_per_side;             /* 1..WP_MAX_PER_SIDE */
    WpPitch  pitch;
} WpMatchConfig;

WpMatchConfig sim_default_config(void);
/* Start a jump for player i (no-op if already airborne). Gameplay triggers come in CP0.1/0.2. */
void     sim_player_jump(SimState *s, int i, uint8_t action);
void     sim_init(SimState *s, const WpMatchConfig *cfg);
void     sim_step(SimState *s, const WpInput *in);
uint64_t sim_hash(const SimState *s);   /* field-wise FNV-1a; independent of struct padding */

#endif
