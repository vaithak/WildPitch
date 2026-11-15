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
#define T_OUT_PAUSE_TICKS  45         /* pause before a throw-in / corner / goal kick is set (players reposition) */
#define T_RESTART_DIST     FX(4.0)    /* opponents keep this far from a set piece until it's played */
/* advertising boards around the pitch (owner request, D-033): players can't cross them, a low ball rebounds */
#define T_BOARD_SIDE       FX(3.0)    /* boards this far outside the touchlines ... */
#define T_BOARD_END        FX(4.5)    /* ... and behind the goal lines (room for the nets) */
#define T_BOARD_H          FX(0.9)    /* board height: a ball below it rebounds */
#define T_BOARD_REST       FX(0.3)    /* rebound speed kept */
#define T_BOARD_BODY       FX(0.35)   /* players stay this far inside the boards */
/* player stats (D-034): 1..99, 50 = the old player; k = (stat - 50) / 50 */
#define T_STAT_SPREAD      10         /* generated stats: slot template +- this (zero-sum per team) */
#define T_STAT_PACE        FX(0.07)   /* top speeds x (1 + this * k) */
#define T_KICKOFF_R_FRAC   FX(0.0871) /* centre-circle radius / pitch length (9.15 / 105, a real pitch) */
#define T_RESTART_MAX      600        /* a set piece not taken in 10 s is abandoned (ball live) */
#define T_GOALKICK_DEPTH   FX(2.5)    /* goal kick spot: this far out from the goal line, centred */
#define T_AI_RESTART_DELAY 40         /* AI takers line it up this long first ... */
#define T_AI_CORNER_WAIT   420        /* ... and on a corner wait (up to 7 s, restart_ticks) for two team-mates in the box */

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
#define T_KEEPER_BODY_R    FX(0.36)
#define T_BODY_H           FX(1.80)
#define T_BODY_REST        FX(0.30)

/* ---- ball control (height bands: see sim.h WP_BAND_*) ---- */
#define T_CONTROL_REACH    FX(1.0)    /* gain control of a loose ball passing within this (swept over the tick) */
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
#define T_SWITCH_DEF_HYST  FX(3.0)    /* they have it: switch to the man this much nearer the goal-side spot ... */
#define T_SWITCH_DEF_COOL  45         /* ... at most every 0.75 s */
#define T_PASS_FLIGHT_MAX  240        /* a pass in flight stops blocking auto-switch after this many ticks */
/* passes & reception (D-020: solve the meeting point, steer the receiver, widen the foot) */
#define T_PASS_ARR_MIN     FX(8.0)    /* tap pass arrival pace: MIN + PER_M * (distance - 5 m), capped at MAX */
#define T_PASS_ARR_PER_M   FX(0.3)    /* (CP0.3a feedback "passes very slow, intercepted": was 5.5 + 0.25/m, max 9) */
#define T_PASS_ARR_MAX     FX(12.0)
#define T_PASS_MAX         FX(24.0)   /* hardest ground pass */
#define T_PASS_CONE_DEG    55         /* receivers within this angle of the stick are candidates */
#define T_PASS_W_ANGLE     10         /* pass pick points per degree off the stick ... */
#define T_PASS_W_DIST      8          /* ... per metre of distance (1 m ~ 0.8 deg) ... */
#define T_PASS_W_FAR       16         /* ... and extra per metre beyond T_PASS_PENALTY_M */
#define T_PASS_MIN_DIST    FX(2.0)    /* nearer than this isn't a pass */
#define T_PASS_PREFILTER_M FX(4.0)    /* skip players this much beyond the max distance before predicting */
#define T_PICK_FLIGHT_V    FX(12.0)   /* pass pick judges receivers at distance / this (s) ahead */
#define T_PASS_CONE_WIDE_DEG 70       /* nobody within the cone: keyboard (8-way) input also accepts mates to here */
#define T_PASS_WIDE_BACK_M FX(1.0)    /* ... but never one more than this behind the passer on a forward/back key */
#define T_PASS_KEEPER_CONE_DEG 25     /* the keeper (only when no outfield mate fits; never for through/lob) */
#define T_PICK_DIAG_LATCH  4          /* a keyboard diagonal released one key at a time stays diagonal this long */
#define T_PICK_STICK_MEM   30         /* stick released: the pass still uses the last direction for this long */
#define T_PASS_STICKY      40         /* points the previewed receiver keeps (hysteresis, ~4 deg / 5 m) */
#define T_PASS_W_LANE_M    100        /* points per metre an opponent is inside T_LANE_CLEAR of the lane (cap 150) */
#define T_LANE_CLEAR       FX(1.5)
#define T_LANE_LOOK        FX(0.3)    /* opponents advanced along their velocity up to this (s) */
#define T_FREE_MARGIN_X    FX(2.5)    /* a ball into space (nobody fits) is weighted to stop this far inside ... */
#define T_FREE_MARGIN_Y    FX(1.5)    /* ... the goal lines / touchlines */
#define T_FREE_MIN_ROLL    FX(6.0)    /* ... but rolls at least this far */
#define T_FREE_MAX_ROLL    FX(18.0)
#define T_PASS_PENALTY_M   FX(24.0)   /* receivers further than this are scored down ... (bigger pitches) */
#define T_PASS_MAX_DIST    FX(32.0)   /* ... and beyond this not picked at all */
#define T_PASS_RUNNER_MS   FX(2.0)    /* receiver moving away from the passer faster than this = a runner */
#define T_SPRINT_SPILL     FX(0.12)   /* extra first-touch spill when receiving at a sprint */
#define T_PASS_TAP_TICKS   12         /* J released within this (200 ms) = tap pass; held longer = through ball */
#define T_THROUGH_STRIDE   FX(0.8)    /* through ball meets the runner this far ahead of his feet */
#define T_THROUGH_MAX      FX(24.0)   /* hardest through ball (futsal kicks ~24 m/s) */
#define T_THROUGH_MARGIN_X FX(3.0)    /* meeting point stays this far inside the goal line ... */
#define T_THROUGH_MARGIN_Y FX(1.5)    /* ... and the touchline */
#define T_THROUGH_STAND_V  FX(6.0)    /* a standing receiver is planned to set off at this speed ... */
#define T_THROUGH_STAND_DELAY FX(0.3) /* ... after this reaction time */
#define T_THROUGH_FREE_MIN FX(12.0)   /* hold J with nobody in the cone: power 12 -> 20 m/s with the hold */
#define T_THROUGH_FREE_MAX FX(20.0)
#define T_CONTROL_STRETCH  FX(1.3)    /* the pass's receiver can stretch for a ball up to here (heavier touch) */
#define T_STRETCH_SPILL    FX(0.3)    /* extra spill at the far edge of the stretch band */
#define T_RECV_STICK_TOL   48         /* stick within this of the hand-off stick (or released) = no opinion */
#define T_RECV_TAKEOVER_TICKS 15      /* stick > 120 deg away from the meet this long = full manual control */
#define T_RECV_GONE_TICKS  6          /* no reachable meeting point this long = the ball has gone */
#define T_RECV_HEADON_MS   FX(4.0)    /* closing on the ball head-on in its last 0.5 s: at most this fast */
#define T_SWITCH_LATCH_TICKS 30       /* after an auto-switch the held stick is ignored until it changes (max) */
#define T_LOB_VZ_BASE      FX(4.0)    /* lofted pass: vz = BASE + PER_M * distance, clamped */
#define T_LOB_VZ_PER_M     FX(0.22)
#define T_LOB_VZ_MIN       FX(5.0)
#define T_LOB_VZ_MAX       FX(9.5)
#define T_HEADER_SHOT      FX(11.0)   /* header at goal: base speed (+25% of the incoming ball's speed) */
#define T_HEADER_SHOT_MAX  FX(17.0)
#define T_HEADER_SHOT_VZ   -FX(0.8)   /* nodded down */
#define T_HEADER_PASS_MAX  FX(12.0)

/* ---- tackles & bodies (CP0.3) ---- */
#define T_TACKLE_START     FX(1.6)    /* J with an opponent on the ball this close = a standing tackle */
#define T_TACKLE_WINDUP    6          /* ticks from the lunge to the foot arriving */
#define T_TACKLE_LUNGE     FX(3.0)    /* extra speed toward the ball when lunging (m/s) */
#define T_TACKLE_REACH     FX(0.8)    /* the ball must be this close to the tackling foot at contact */
#define T_TACKLE_BLOCK_V   FX(1.5)    /* a won tackle sends the ball back toward the tackler this fast (+60% his speed) */
#define T_TACKLE_STUN      24         /* the tackled player can't touch the ball again for this long */
#define T_TACKLE_TICKS     30         /* whole tackle action (recovery included) */
#define T_PLAYER_GAP       FX(0.6)    /* two players' bodies never overlap closer than this */

/* ---- team plan (team_plan.c, D-030) ---- */
#define T_SHAPE_LAST_GAP   FX(1.5)    /* back line stays this far goal-side of their deepest outfield man ... */
#define T_SHAPE_LAST_MIN   FX(9.0)    /* ... but never deeper than this from our goal line (no offside yet) */
#define T_PLAN_EVERY       3          /* the plan's heavy part (winner/cover/marks) refreshes every N ticks */
#define T_PLAN_FOCUS_RATE  FX(0.07)   /* shape follows a smoothed ball (per-tick blend, ~0.25 s) */
#define T_PLAN_MARK_ZONE   FX(9.0)    /* a marker only takes an opponent within this of his own spot */
#define T_PLAN_SUPPORT_TICKS 90       /* near-support / runner re-chosen at most every 1.5 s */
#define T_PLAN_COUNTERPRESS 150       /* after losing it, the cover presses too for 2.5 s */

/* ---- instinct AI (brain.c, D-023) ---- */
#define T_AI_DECIDE        6          /* each AI player re-decides every this many ticks (staggered) [NORMAL] */
/* difficulty (per team, WP_AI_EASY / NORMAL / HARD): decision beat, reaction to a struck pass, keeper reaction to a
 * shot, how exposed a carrier's ball must be for a tackle. NORMAL = the values above/below. */
#define T_AI_DECIDE_BY_LEVEL      {9, 6, 4}
#define T_AI_PASS_REACT_BY_LEVEL  {18, 12, 8}
#define T_KEEPER_REACT_BY_LEVEL   {26, 20, 15}
#define T_AI_TACKLE_EXPOSED_BY_LEVEL {FX(0.75), FX(0.6), FX(0.5)}
#define T_AI_COMMIT        FX(0.8)    /* utility bonus for sticking with the current action/target */
#define T_AI_FIRST_TOUCH   10         /* an AI carrier controls the ball this long before passing/shooting */
#define T_AI_AIM_ERR       FX(0.3)    /* AI shot aim error (uniform +-, m at the goal line): base ... */
#define T_AI_AIM_ERR_M     FX(0.05)   /* ... + this per metre of distance ... */
#define T_AI_AIM_ERR_PRESS FX(0.6)    /* ... + this with a defender within 2 m */
#define T_AI_SHOT_POWER    FX(24.0)   /* AI shot speed (m/s) */
#define T_AI_SHOT_RANGE    FX(22.0)   /* AI considers shooting within this of goal */
#define T_AI_CROSS_ZONE    FX(16.0)   /* crosses / corner targets: team-mates within this of goal */
#define T_AI_TACKLE_DIST   FX(0.95)    /* AI tackles when the ball is this close ... */
#define T_AI_TACKLE_EXPOSED FX(0.6)  /* ... and this far from the carrier's feet (between touches) ... */
#define T_AI_TACKLE_SHIELD FX(0.3)    /* ... or (carrier slow < 2 m/s, holding it) this far */
#define T_AI_PASS_REACT    12         /* AI defenders react to a pass being struck after this many ticks (0.2 s) */
#define T_INTERCEPT_REACH  FX(0.8)    /* cutting out someone else's pass needs the ball this close (the receiver: 1.0) */
#define T_KEEPER_REACH     FX(1.0)    /* keepers take a loose ball within this (hands) ... */
#define T_KEEPER_HANDS_Z   FX(1.45)   /* ... up to this high (body height until CP0.3b adds hands: higher = parry) ... */
#define T_KEEPER_CATCH_REL FX(16.0)   /* ... arriving at up to this speed (faster: it beats/bounces off him) */
#define T_KEEPER_REACT     20         /* a keeper reacts to a shot 0.33 s after it is struck ... */
#define T_KEEPER_SHUFFLE   FX(5.0)    /* ... and shuffles across his line at most this fast (dives: CP0.3b) */
#define T_DIVE_SPEED       FX(3.0)    /* keeper dive: sideways speed ... */
#define T_DIVE_HOP         FX(1.6)    /* ... small leave-the-ground (vz) ... */
#define T_DIVE_REACH       FX(1.0)    /* ... outstretched body from his centre toward the dive side ... */
#define T_DIVE_HANDS       FX(0.25)   /* ... a ball within this of that line is reached ... */
#define T_DIVE_TOP         FX(2.2)    /* ... up to this high ... */
#define T_DIVE_CATCH_REL   FX(18.0)   /* ... caught at body height up to this speed, else parried */
#define T_DIVE_EXTEND      15         /* ticks for the dive to stretch to full reach */
#define T_DIVE_LIVE        30         /* ticks of a dive in which he can still make the save */
#define T_DIVE_TICKS       54         /* whole dive incl. getting up */
#define T_DIVE_MIN_GAP     FX(0.7)    /* he only dives for shots crossing this far from him ... */
#define T_DIVE_WHEN        FX(0.55)   /* ... arriving within this (s) */
#define T_AIM_POST_MARGIN  FX(0.6)    /* aim-assisted shots go this far inside the post (scales with the goal) */
#define T_KEEPER_HOLD_MAX  360        /* a keeper may hold the ball in his hands this long (6 s), then it's released */
#define T_AI_KEEPER_HOLD   50         /* the AI keeper looks up for this long before distributing */
#define T_THROW_SPEED      FX(11.0)   /* keeper's throw with nobody targeted */
#define T_PUNT_SPEED       FX(21.0)   /* keeper's punt from the hands ... */
#define T_PUNT_VZ          FX(8.0)    /* ... and its loft */
#define T_KEEPER_RUSH_V    FX(8.0)    /* he only comes out for loose balls slower than this (never to cut out a shot) */

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
