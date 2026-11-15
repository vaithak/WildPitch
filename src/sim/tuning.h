/* tuning.h - every gameplay feel constant in one place (metres, seconds, ticks @ 60 Hz).
 * Changing any value here changes sim behaviour -> golden hash update (say so in the commit). */
#ifndef WP_TUNING_H
#define WP_TUNING_H
#include "sim/fixed.h"

/* ---- ball ---- */
#define T_GRAVITY          FX(9.81)
#define T_BALL_R           FX(0.11)
/* Grass bounce (D-016, impulse model): vz' = REST*vin - LIN (0 below CUT); horizontal speed loses
 * min(MU*(1+REST)*vin, GRIP*|v|) - friction during contact, capped where the ball would start to roll. */
#define T_BOUNCE_REST      FX(0.62)   /* normal restitution (FIFA turf 0.55-0.71) -> 2 m drop rebounds ~0.7 m */
#define T_BOUNCE_LIN       FX(0.06)   /* m/s lost per bounce (kills endless micro-hops) */
#define T_BOUNCE_CUT       FX(0.5)    /* rebound slower than this -> the ball just rolls (apex 1.3 cm) */
#define T_BOUNCE_MU        FX(0.45)   /* ball-grass sliding friction during the impact */
#define T_BOUNCE_GRIP      FX(0.39)   /* max share of horizontal speed one bounce can take (shell: a/(1+a), a=2/3) */
/* Rolling on grass: decel = ROLL_DECEL + ROLL_LIN*v + AIR_DRAG*v^2. Fast balls bleed speed quickly, slow balls
 * creep to a stop (a constant-only model made a 12 m/s pass roll 55 m / 9 s). 12 m/s pass -> ~18 m, ~5 s. */
#define T_ROLL_DECEL       FX(0.9)    /* m/s^2 constant part (rolling resistance) */
#define T_ROLL_LIN         FX(0.35)   /* 1/s speed-proportional part (grass) */
#define T_AIR_DRAG         FX(0.013)  /* quadratic drag: a = k*v^2 (real ball ~0.013) */
#define T_CURL             FX(0.045)  /* Magnus: lateral accel = k * spin * speed */
#define T_SPIN_DECAY_AIR   FX(0.5)    /* per second */
#define T_SPIN_DECAY_GROUND FX(4.0)
#define T_SUBSTEP_MAX_MOVE FX(0.08)   /* ball never moves more than this per substep (no tunnelling) */
#define T_POST_R           FX(0.06)
#define T_POST_REST        FX(0.6)
#define T_NET_KEEP         FX(0.12)   /* speed kept when the ball hits the net (soft) */
#define T_NET_DRAG         FX(5.0)    /* per second: ball inside the net loses speed fast */

/* ---- match flow ---- */
#define T_GOAL_PAUSE_TICKS 120        /* celebration before kickoff reset */
#define T_OUT_PAUSE_TICKS  45         /* temporary out-of-play rule (CP0.1 until CP0.4 rules) */

/* ---- players ---- */
#define T_ACCEL            FX(18.0)
#define T_WALK_SPEED       FX(6.0)
#define T_SPRINT_SPEED     FX(8.0)
#define T_STOP_DECEL       FX(14.0)
#define T_DRIBBLE_SPEED_MUL FX(0.88)  /* top speed with the ball */
#define T_KICK_SPEED_MUL   FX(0.45)   /* top speed during a kick wind-up */
#define T_TURN_DEG_S       720        /* body turn rate, no ball (deg/s) */
#define T_TURN_DEG_S_BALL  540
#define T_TURN_DEG_S_SPRINT 420
#define T_BODY_R           FX(0.30)   /* body blocking radius */
#define T_KEEPER_BODY_R    FX(0.48)
#define T_BODY_H           FX(1.80)
#define T_BODY_REST        FX(0.30)

/* ---- ball control (height bands: see sim.h WP_BAND_*) ---- */
#define T_CONTROL_REACH    FX(0.75)   /* horizontal distance from player centre to gain control */
#define T_CONTROL_MAX_REL  FX(16.0)   /* relative speed above which first touch fails (deflects) */
#define T_LOSE_DIST        FX(2.2)    /* owner loses possession beyond this distance */
/* Dribbling = rhythmic touches (D-014): while the ball is in reach, touch every GAP ticks; each touch is sized so
 * the ball sits at LEAD ahead of the feet when the next touch is due. */
#define T_TOUCH_LEAD_JOG   FX(0.45)
#define T_TOUCH_LEAD_SPRINT FX(0.75)  /* must stay well inside T_TOUCH_REACH */
#define T_TOUCH_GAP_WALK   16         /* ticks between dribble touches (0.27 s) */
#define T_TOUCH_GAP_SPRINT 20         /* 0.33 s */
#define T_TOUCH_REACH      FX(1.0)    /* a dribble touch needs the ball within foot reach */
#define T_TOUCH_MIN_GAP    4          /* fastest re-touch when changing direction (ticks) */
#define T_STOP_SPOT        FX(0.4)    /* releasing the stick: the ball is trapped this far out from the player */
#define T_TRAP_GAIN        FX(8.0)    /* 1/s: how firmly the sole steers the ball to that spot */
#define T_TRAP_MAX_REL     FX(2.5)    /* m/s: max speed the sole moves the ball relative to the player */
#define T_GATHER_MAX_REL   FX(6.0)    /* kick wind-up: max speed the foot can drag the ball into position (m/s) */
#define T_GATHER_MAX_DEG   75         /* contact point: at most this far round the body from where the ball is */
#define T_BODY_HIT_COOLDOWN 12        /* after a ball smacks into you, you can't control it for this long */

/* ---- switching & passing (CP0.2) ---- */
#define T_SWITCH_COOL      30         /* ticks between auto-switches */
#define T_SWITCH_LOCK_MANUAL 90       /* after a manual switch, no auto-switch for this long */
#define T_SWITCH_HYST      FX(2.0)    /* auto-switch only to a team-mate this much nearer the ball */
#define T_PASS_FLIGHT_MAX  240        /* a pass in flight stops blocking auto-switch after this many ticks */
#define T_PASS_ARRIVE      FX(5.5)    /* targeted ground pass arrives at the receiver at this speed */
#define T_PASS_MAX         FX(22.0)   /* hardest ground pass */
#define T_PASS_CONE_DEG    55         /* receivers within this angle of the stick are candidates */
#define T_PASS_LEAD_S      FX(0.25)   /* aim at the receiver's position this far ahead */
#define T_SPRINT_SPILL     FX(0.12)   /* extra first-touch spill when receiving at a sprint */
#define T_PASS_TAP_TICKS   9          /* J released within this = tap (targeted pass); held longer = through ball */
#define T_THROUGH_LEAD_MIN FX(2.5)    /* through ball lands this far ahead of the receiver at minimum hold ... */
#define T_THROUGH_LEAD_MAX FX(10.0)   /* ... and this far at full hold */
#define T_THROUGH_ARRIVE   FX(3.0)    /* through ball's speed when it reaches that spot */
#define T_LOB_VZ_BASE      FX(4.0)    /* lofted pass: vz = BASE + PER_M * distance, clamped */
#define T_LOB_VZ_PER_M     FX(0.22)
#define T_LOB_VZ_MIN       FX(5.0)
#define T_LOB_VZ_MAX       FX(9.5)
#define T_HEADER_SHOT      FX(11.0)   /* header at goal: base speed (+25% of the incoming ball's speed) */
#define T_HEADER_SHOT_MAX  FX(17.0)
#define T_HEADER_SHOT_VZ   -FX(0.8)   /* nodded down */
#define T_HEADER_PASS_MAX  FX(12.0)

/* ---- kicks ---- */
#define T_CHARGE_MAX_TICKS 36         /* full power after 0.6 s */
#define T_KICK_WINDUP_PASS 4
#define T_KICK_WINDUP_SHOT 6
#define T_KICK_WINDUP_CHIP 6
#define T_KICK_TOTAL_TICKS 26         /* action ends after this (pose recovers by then) */
#define T_KICK_IMMUNE      8          /* ticks the kicker's own body ignores the ball */
#define T_PASS_SPEED       FX(12.0)
#define T_SHOT_MIN         FX(14.0)
#define T_SHOT_MAX         FX(30.0)
#define T_SHOT_VZ_MIN      FX(1.2)
#define T_SHOT_VZ_MAX      FX(4.2)
#define T_CHIP_MIN         FX(8.0)
#define T_CHIP_MAX         FX(15.0)
#define T_CHIP_VZ_MIN      FX(5.0)
#define T_CHIP_VZ_MAX      FX(8.5)
#define T_AIM_WINDOW_DEG   65         /* shots within this of the goal are aimed at a target (D-013) */
#define T_AIM_RESIDUAL     FX(0.12)   /* share of the stick's angular error that remains (skill) */
#define T_AIM_SIDE         FX(0.30)   /* target = this * goal width from the centre (inside the posts) */
#define T_ERR_DEG_PER_MS   FX(0.55)   /* visible-cause inaccuracy: degrees per m/s of player speed */
#define T_ERR_DEG_CHARGE   FX(3.0)    /* extra degrees at full power */

#endif
