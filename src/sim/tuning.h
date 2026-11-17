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
#define T_RESTART_DIST     FX(4.0)    /* opponents keep this far from a set piece until it's played (fallback) */
#define T_THROWIN_DIST     FX(2.0)    /* Law 15: opponents 2 m from the thrower (CP0.3e) */
#define T_CORNER_DIST_FRAC FX(0.0871) /* corners: 9.15 m on a 105 m pitch, scaled to ours (7.0 m on 80 m) */
#define T_GOALAREA_FRAC    FX(0.0524) /* goal area depth = 5.5 / 105 of the length (goal kicks from its line) */
/* advertising boards around the pitch (owner request, D-033): players can't cross them, a low ball rebounds */
#define T_BOARD_SIDE       FX(3.0)    /* boards this far outside the touchlines ... */
#define T_BOARD_END        FX(4.5)    /* ... and behind the goal lines (room for the nets) */
#define T_BOARD_H          FX(0.9)    /* board height: a ball below it rebounds */
#define T_BOARD_REST       FX(0.3)    /* rebound speed kept */
#define T_BOARD_BODY       FX(0.35)   /* players stay this far inside the boards */
/* player stats (D-034): 1..99, 50 = the old player; k = (stat - 50) / 50 */
#define T_STAT_SPREAD      10         /* generated stats: slot template +- this (zero-sum per team) */
#define T_STAT_PACE        FX(0.07)   /* top speeds x (1 + this * k) */
#define T_STAMINA_DRAIN    FX(0.04)   /* energy (0..1) used per second of sprinting (25 s flat out) ... */
#define T_STAMINA_RECOVER  FX(0.02)   /* ... regained per second jogging (x2 below 3 m/s) ... */
#define T_STAMINA_TIRED    FX(0.40)   /* ... below this the sprint fades, to 25% of its extra pace at empty */
#define T_STAT_STAMINA     FX(0.40)   /* drain x (1 - this * k) */
#define T_STAT_CONTROL     FX(0.50)   /* first-touch spill x (1 - this * k) */
#define T_STAT_PASS_PACE   FX(0.10)   /* pass arrival pace x (1 + this * k): good passers zip it */
#define T_STAT_SHOT_POWER  FX(0.06)   /* shot power x (1 + this * k) */
#define T_STAT_SHOT_AIM    FX(0.60)   /* AI shot aim error x (1 - this * k) */
#define T_STAT_SHOT_MARGIN FX(0.50)   /* aim-assisted target this much nearer the post per k (deterministic) */
#define T_STAT_TACKLE      FX(0.15)   /* tackle reach x (1 + this * k) ... */
#define T_STAT_TACKLE_READ FX(0.20)   /* ... and the AI needs the ball x (1 - this * k) exposed */
#define T_STAT_KEEP_REACT  8          /* keeper reaction ticks - this * k ... */
#define T_STAT_KEEP_REACH  FX(0.25)   /* ... dive reach x (1 + this * k) */
#define T_STAT_AI_SHOOT    FX(0.40)   /* AI: shot utility x (1 + this * k): good finishers shoot more */
#define T_AI_LANE_V_PER_M  FX(0.15)   /* AI pass lanes (CP0.3c): the ball's mean pace = 8 + this/m ... */
#define T_AI_LANE_V_MAX    FX(13.0)   /* ... capped */
#define T_AI_LANE_REACH    FX(1.0)    /* an opponent within this of the path cuts it out ... */
#define T_AI_LANE_RUN      FX(3.0)    /* ... after running at it this fast (m/s) ... */
#define T_AI_LANE_REACT    FX(0.30)   /* ... once he's reacted */
#define T_AI_LANE_CAP      FX(4.0)    /* the feature is clamped to +-this (m) */
#define T_AI_PRESSURE_M    FX(3.0)    /* an opponent closer than this to the carrier counts as pressure on a dribble */
#define T_KEEPER_NARROW_D  FX(16.0)   /* an opponent carrying it this close: the keeper comes off his line ... */
#define T_KEEPER_NARROW_K  FX(0.30)   /* ... to this share of the distance ... */
#define T_KEEPER_NARROW_MAX FX(3.5)   /* ... at most this far out */
#define T_AI_SAVE_ENERGY   FX(0.55)   /* AI below this energy jogs unless the job is urgent (press/chase/cover) */
#define T_AI_TIRED_RUN     FX(2.0)    /* ... and a run in behind costs this x (1 - energy) utility */
/* match clock (D-036) */
#define T_HALF_TICKS       18000      /* the game's half: 5 real minutes = 45' (clock at 9x) */
#define T_HALFTIME_TICKS   210        /* the half-time pause (3.5 s), then ends change */
#define T_STOPPAGE_GOAL_S  45         /* game seconds of stoppage per goal ... */
#define T_STOPPAGE_RESTART_S 8        /* ... per throw-in / corner / goal kick ... */
#define T_STOPPAGE_MAX_MIN 4          /* ... announced as whole minutes, 1..this */
/* movement assist toward the ball (D-037, reports/Assisted movement toward the ball.md): per level Low/Normal/High */
#define T_CHASE_ANG_FULL   {3641, 5461, 7282}            /* F = 20/30/40 deg (wp_angle): full strength AND the turn cap */
#define T_CHASE_COS_FULL   {FX(0.940), FX(0.866), FX(0.766)}
#define T_CHASE_COS_EDGE   {FX(0.766), FX(0.574), FX(0.342)}   /* E = 40/55/70 deg: exactly zero beyond */
#define T_CHASE_MAX_T      {FX(2.0), FX(3.0), FX(3.5)}  /* only meeting points this soon (s) */
#define T_CHASE_SHARE_S    {FX(0.15), FX(0.30), FX(0.50)} /* weight fades to 0 when a team-mate is this much quicker */
#define T_CHASE_PRESS_MAX  FX(12.0)   /* ... and onto an opponent with the ball within this (CP0.3e) */
#define T_CHASE_NEAR_R     FX(12.0)   /* D-049: ball or meeting point this close = always his to chase (no share fade) */
#define T_RECV_LOCK_S      FX(0.6)    /* receiver: no takeover once the ball is this close in time (CP0.3e) */
#define T_CHASE_STICK_MIN  48         /* stick magnitude (of 127) below this never chases */
#define T_CHASE_MATES      3          /* team-mates nearest the ball compared for "is it mine" */
#define T_SWITCH_TIME_K    FX(0.5)    /* time-based loose-ball switch: challenger needs < this x my time (YSoccer) */
#define T_SETUP_MAX        240        /* restart transition (D-044): at most 4 s of walking into place ... */
#define T_SETUP_ARRIVE     FX(0.4)    /* ... the taker counts as at the ball within this */
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
#define T_TOUCH_LEAD_SPRINT FX(1.30)  /* CP0.3e: a sprint knocks it on, beyond foot reach - he runs onto it */
#define T_TOUCH_GAP_WALK   16         /* ticks between dribble touches (0.27 s) */
#define T_TOUCH_GAP_SPRINT 28         /* 0.47 s (CP0.3e: was 20) */
#define T_TOUCH_SLOW_TICKS 8          /* sprinting with the ball: slowed only this long around each touch */
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
/* active-player switching (D-050, reports/Active player switching.md), per mode {MANUAL, EVENTS, ASSISTED} */
#define T_SW_K             30         /* ratio offset (ticks): (t_c + K) vs (t_me + K) - GRF's +500 ms */
#define T_SW_NEXT_NUM      4          /* oracle hysteresis: a challenger replaces sw_next when (t_c+K)*5 < (t_inc+K)*4 */
#define T_SW_NEXT_DEN      5
#define T_SW_NEXT_HOLD     36         /* ... and only after the incumbent has been shown this long (ticks) */
#define T_SW_EV_NUM        {0, 1, 7}  /* event window: switch when (t_c+K)*DEN < (t_me+K)*NUM */
#define T_SW_EV_DEN        {1, 2, 10}
#define T_SW_OV_NUM        {0, 1, 1}  /* otherwise only this override (Events: 3x quicker) */
#define T_SW_OV_DEN        {1, 3, 2}
#define T_SW_INTENT_X      {1, 4, 3}  /* stick held at the ball: either rule needs 1/x */
#define T_SW_INTENT_COS    FX(0.643)  /* ... "at the ball" = within 50 deg of it (or of where it is in 0.5 s) */
#define T_SW_SOON_STEP     10         /* ... "in 0.5 s" = this 50 ms step of the cached path */
#define T_SW_HOT_NUM       4          /* the indicator turns solid within 1.25x (5/4) of an unprompted switch */
#define T_SW_HOT_DEN       5
#define T_SW_BLEND_MIN_V   FX(0.5)    /* post-switch blend only for a man moving at least this fast (m/s) */
#define T_SW_DWELL         {24, 24, 18}   /* every switch: no unprompted switch for this long (ticks) */
#define T_SW_BACK          {60, 60, 60}   /* return lock: not back to the man just left (nor him as AI winner) */
#define T_SW_EVENT_WIN     {9, 9, 12}     /* ticks the defensive ratio applies after the ball goes loose */
#define T_SW_BLEND         {18, 18, 24}   /* post-switch blend (ticks; <= 50% of his old motion) */
#define T_SW_LOOK          FX(0.2)    /* ETA: start from where he'll be in 0.2 s (position + velocity) */
#define T_SW_TURN_S        FX(0.30)   /* ... + a full reversal costs this (scaled by (1 - cos)/2) */
#define T_SW_ACC_PEN       FX(0.22)   /* ... + a standing man loses this reaching full speed (top / 2 T_ACCEL) */
#define T_SW_DEF_LEAD      FX(0.3)    /* their carrier: the press point leads him by this ... */
#define T_SW_GS_DIST       FX(1.5)    /* ... and sits this far goal-side of the ball (toward our goal centre) */
#define T_SW_GS_TOL        FX(0.5)    /* a man this far the wrong side of the ball is beaten ... */
#define T_SW_BEATEN_PEN    FX(0.5)    /* ... and pays this (s) */
#define T_SW_RTC           6          /* a press within this of sw_next changing goes to the previous one */
#define T_SW_CYCLE_WIN     40         /* presses this close together cycle through new men */
#define T_SW_DIR_COS       FX(0.707)  /* Q + direction: men within 45 deg of the stick, seen from the ball */
#define T_SW_HUMAN_NUM     6          /* team plan: the seat's man stays the ball-winner within (t_ai+6)*6/5 ticks */
#define T_SW_HUMAN_DEN     5
#define T_SW_HUMAN_OFF     6
#define T_PASS_FLIGHT_MAX  240        /* a pass in flight stops blocking auto-switch after this many ticks */
/* passes & reception (D-020: solve the meeting point, steer the receiver, widen the foot) */
#define T_PASS_ARR_MIN     FX(8.0)    /* tap pass arrival pace: MIN + PER_M * (distance - 5 m), capped at MAX */
#define T_PASS_ARR_PER_M   FX(0.3)    /* (CP0.3a feedback "passes very slow, intercepted": was 5.5 + 0.25/m, max 9) */
#define T_PASS_ARR_MAX     FX(12.0)
#define T_PASS_LEAD_MARGIN_X FX(2.5)  /* a pass never leads a runner closer than this to the goal line ... */
#define T_PASS_LEAD_MARGIN_Y FX(2.0)  /* ... or the touchline (was 1.0 / 1.0, owner: passes ran out) */
#define T_PASS_EDGE_MIN    FX(4.5)    /* near a line a pass is weighted to die inside it, but arrives at least this fast */
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
#define T_FIRST_TOUCH_SPILL FX(0.45)  /* first touch keeps this share of the relative speed at T_CONTROL_MAX_REL */
#define T_GAIN_TOUCH_COOLDOWN 6       /* ticks after gaining the ball before the next touch */
#define T_PASS_TAP_TICKS   20         /* S released within this (333 ms) = tap pass; held longer = through ball (CP0.3e: was 12) */
#define T_THROUGH_MIN_K    FX(1.10)   /* a through ball is struck at least this x a tap to the same man (CP0.3e) */
#define T_THROUGH_DEEP_S   FX(1.2)    /* full meter: the meeting point this much further along his run (s) */
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
#define T_LOB_VZ_BASE      FX(5.0)    /* lofted pass (D-046): vz = BASE + PER_M * D, D = where it lands (no clamp: */
#define T_LOB_VZ_PER_M     FX(0.245)  /* 45 deg at 10 m easing to 35 deg at 50 m) */
#define T_LOB_TAP_TICKS    12         /* lob (D-045): A released within this = lofted to the ringed mate ... */
#define T_LOB_MIN_D        FX(12.0)   /* ... held longer = the meter: it lands this far (just past the tap) ... */
#define T_LOB_MAX_D        FX(45.0)   /* ... up to this far (full meter) along the aim ... */
#define T_LOB_MAX_D_PITCH  FX(0.62)   /* ... but at most this x the pitch length (45 / 37 / 25 m on 80 / 60 / 40 m) */
#define T_LOB_SNAP         FX(4.0)    /* a team-mate whose predicted point is within this (+ SNAP_K x D) ... */
#define T_LOB_SNAP_K       FX(0.12)
#define T_LOB_ANGLE_M      FX(6.0)    /* ... (+ this x angle/pi off the aim) of the landing spot gets it */
#define T_LOB_BIAS_MAX     FX(3.0)    /* meter surplus / shortfall vs the snapped mate: dropped this much deeper / shorter */
#define T_LOB_RECV_Z       FX(1.0)    /* a targeted lob is led to where it comes DOWN through this height ... */
#define T_LOB_HEAD_Z       FX(2.3)    /* ... an AI ball to a mate in the box (T_AI_CROSS_ZONE): through a jumping head (1.7 + 0.6)
                                         (D-051 kept it: the driven 1.8 m cross lost every marked duel) */
#define T_CROSS_DRIVEN     0          /* AI crosses / corners into the box (D-051): 1 = driven to T_CROSS_Z, 0 = lob to HEAD_Z */
#define T_CROSS_Z          FX(1.8)    /* a driven cross comes down through this height at the receiver's predicted point */
#define T_CROSS_VZ_BASE    FX(6.0)    /* ... launched with vz = BASE + PER_M x d (apex ~2.4 m at 15 m, ~2.9 m at 25 m) */
#define T_CROSS_VZ_PER_M   FX(0.06)
#define T_CROSS_CLEAR_W    FX(1.5)    /* ... only with no opponent within this of its path while it is below T_CB_JUMP, */
#define T_CROSS_CLEAR_END  FX(3.0)    /* ... up to this short of the receiver (else it is lofted over them) */
#define T_LOB_TAP_D        FX(18.0)   /* a tap with nobody to pick: lands this far along the aim */
#define T_LOB_AI_MIN_D     FX(12.0)   /* AI lobs / crosses / restart lobs: D clamped to [this, the meter's max] */
#define T_THROW_LOFT_VZ_BASE FX(4.0)  /* from the hands (keeper's overarm throw, long throw-in): the pre-D-046 loft */
#define T_THROW_LOFT_VZ_PER_M FX(0.22) /* vz = BASE + PER_M * distance, clamped to [MIN, MAX] */
#define T_THROW_LOFT_VZ_MIN FX(5.0)
#define T_THROW_LOFT_VZ_MAX FX(9.5)
#define T_HEADER_SHOT      FX(11.0)   /* header at goal: base speed (+25% of the incoming ball's speed) */
#define T_HEADER_SHOT_MAX  FX(17.0)
#define T_HEADER_SHOT_VZ   -FX(0.8)   /* nodded down */
#define T_HEADER_PASS_NOBODY_D FX(8.0) /* header pass with nobody to pick: this far along the aim (D-048 solve) */
#define T_HEAD_Z           FX(1.70)   /* head centre above the feet (header.c) */
#define T_HEAD_FWD         FX(0.12)   /* ... and ahead of the body centre */
#define T_HEAD_REACH_XY    FX(0.45)   /* head contact: ball within this horizontally ... */
#define T_HEAD_REACH_Z     FX(0.32)   /* ... and vertically of the head */
#define T_HEADER_WAIT_MAX  70         /* ticks a header attempt waits for the ball */

/* ---- aerial contact (D-047, contact.c): bands, stances, planner ---- */
#define T_CB_GROUND        FX(0.25)   /* ball-centre z at contact: GROUND <= this: trap (the old first touch) */
#define T_CB_LOW           FX(0.55)   /* LOW: instep cushion */
#define T_CB_MID           FX(1.0)    /* MID: thigh */
#define T_CB_CHEST         FX(1.6)    /* CHEST: chest (report 1.5; 1.6 = where a pressed button heads it, D-048) */
#define T_CB_HEAD          FX(2.05)   /* HEAD: standing header; header-control only for a slow ball */
#define T_CB_JUMP          FX(2.6)    /* JUMP: jumping header */
/* stance rows: h_pop, d_ready, e_firm, v_clean, setup ticks, nominal normal elevation (deg; chest: descending /
 * driven), reach, planner base difficulty. Clean touch: the ball pops h_pop and drops d_ready ahead (plus his run) at
 * T_CONTACT_READY_Z. Base difficulty: the planner lets a ball drop to the chest / thigh rather than head it down. */
#define T_ST_TRAP          { FX(0.0),  FX(0.30), FX(0.50), FX(16.0), 12, 0,  FX(1.0), FX(0.0) }
#define T_ST_INSTEP        { FX(0.05), FX(0.35), FX(0.50), FX(13.0), 12, 70, FX(0.9), FX(0.10) }
#define T_ST_THIGH         { FX(0.12), FX(0.50), FX(0.55), FX(15.0), 18, 60, FX(0.6), FX(0.05) }
#define T_ST_CHEST         { FX(0.10), FX(0.60), FX(0.55), FX(17.0), 24, 35, FX(0.45), FX(0.0) }
#define T_ST_HEADCTL       { FX(0.10), FX(0.45), FX(0.70), FX(11.0), 15, 45, FX(0.45), FX(0.5) }
#define T_ST_CHEST_DRIVEN_DEG 5       /* chest normal for a driven ball (vz > -T_ST_DRIVEN_VZ) */
#define T_ST_DRIVEN_VZ     FX(2.0)
#define T_CONTACT_NOCTRL_K FX(1.6)    /* above this x v_clean: no control, the body deflects it (head: above v_clean) */
#define T_CONTACT_READY_Z  FX(0.30)   /* the cushioned ball is "ready" (at the feet) when it drops to this */
#define T_CONTACT_SETUP_K  FX(0.6)    /* touch quality q = (1 - over - 0.6 setup deficit - 0.3 pressure - stretch) */
#define T_CONTACT_PRESS_K  FX(0.3)    /*   x (1 + 0.25 k_control), clamped 0..1 */
#define T_CONTACT_STAT_K   FX(0.25)
#define T_CONTACT_PRESS_NEAR FX(1.5)  /* pressure 1 with an opponent this close, 0 beyond FAR */
#define T_CONTACT_PRESS_FAR FX(4.0)
#define T_CONTACT_ERR_DEG  6          /* "readable" error: exit angle +- this x (1 - q) (one RNG draw, only if q < 1) */
#define T_CONTACT_STICK_MEM 12        /* the stick held this recently still aims the cushion */
#define T_SETTLE_Z         FX(2.1)    /* a settling ball (aerial control) stays his up to this height ... */
#define T_SETTLE_EXTRA     6          /* ... for its fall time plus this many ticks */
#define T_CONTACT_RING     180        /* ball prediction horizon (ticks) */
#define T_CONTACT_COMMIT   5          /* the plan freezes this many ticks before contact */
#define T_CONTACT_WINDOW   20         /* later candidates (the ball dropping to a better band) up to this after the first */
#define T_CONTACT_REACT    FX(0.15)   /* planner reach: REACH_K x his sprint speed after this reaction time ... */
#define T_CONTACT_REACH_K  FX(0.8125)
#define T_CONTACT_PLAN_REACH FX(0.6)  /* ... plus this */
#define T_CONTACT_W_DIFF   FX(4.0)    /* candidate score = -W_DIFF difficulty - W_TIME (t - t_first)/30 - W_PRESS p */
#define T_CONTACT_W_TIME   FX(1.0)
#define T_CONTACT_W_PRESS  FX(2.0)
#define T_CONTACT_WAIT_D   FX(0.45)   /* waiting for the planned (lower) band: only while the ball is this far off */
#define T_CONTACT_STAND_BACK FX(0.50) /* the AI stands this far beyond its planned contact along the flight (> body radius + ball: D-051) */

/* ---- intents and first-time contacts (D-048) ---- */
#define T_INTENT_MAX_AGE   90         /* a buffered press lives this long after the release ... */
#define T_INTENT_ETA_SLIP  42         /* ... and is dropped when the ball's predicted arrival slips by more than this */
#define T_INTENT_HEAD_Z    T_CB_CHEST /* PASS / SHOOT / LOB on a ball reaching him at least this high: a header (else chest) */
#define T_VOLLEY_K         FX(0.9)    /* volley: min(T_SHOT_MAX, 0.9 x the set shot + 0.36 x the ball's pace into it) */
#define T_VOLLEY_IN_K      FX(0.36)
#define T_VOLLEY_ERR_DEG   2          /* volley aim error +- this x (2 LOW / 3 MID) x (1 + over) (one draw) */
#define T_VOLLEY_RISE      FX(0.15)   /* a volley rises this (rad) per metre of contact above ... */
#define T_VOLLEY_RISE_Z    FX(0.45)
#define T_VOLLEY_REACH     FX(1.1)    /* a settling ball (chest -> volley) is struck within this */
#define T_VOLLEY_MIN_H     FX(0.35)   /* a tapped buffered shot strikes at least this hard (0..1) */
#define T_HALFVOLLEY_K     FX(1.05)   /* half-volley pass: the ground pass x this, struck down at ... */
#define T_HALFVOLLEY_VZ    -FX(0.5)   /* ... this vz */
#define T_VOLLEYPASS_MAX_REL FX(10.0) /* a MID-band volley pass only off a ball this slow ... */
#define T_VOLLEYPASS_DEG   60         /* ... and within this of his facing */
#define T_LAYOFF_DIST      FX(8.0)    /* chest lay-off: a mate this close ... */
#define T_LAYOFF_DEG       60         /* ... within this of the aim */
#define T_LAYOFF_K         FX(0.55)   /* ... speed 0.55 x the ball's pace into the chest + 1.5, at most 6 */
#define T_LAYOFF_BASE      FX(1.5)
#define T_LAYOFF_MAX       FX(6.0)
#define T_HEAD_LAW_U       FX(0.63)   /* header: out = sqrt((0.63 u_n + 1.63 v_head)^2 + (0.85 |r_t|)^2) */
#define T_HEAD_LAW_V       FX(1.63)
#define T_HEAD_LAW_T       FX(0.85)
#define T_HEAD_V_STAND     FX(2.5)    /* head speed: standing / jumping (passes, shots) */
#define T_HEAD_V_JUMP      FX(3.5)
#define T_HEADER_ERR_DEG   2          /* header aim error +- this x (1 + phi/90) x (1 + over); x3 between 90-135 deg */
#define T_HEADER_CLEAN_V   FX(11.0)   /* ("over" = the incoming pace beyond this, as head-control's v_clean) */
#define T_FLICK_DEG        135        /* turned this far from where it came: a flick-on (keeps ~0.88 of the pace, */
#define T_FLICK_MAX_DEG    40         /*  bends it at most this much) */
#define T_FLICK_K          FX(0.88)   /* (at most this share of the pace, at least MIN_K: a short flick dies a bit) */
#define T_FLICK_MIN_K      FX(0.5)
#define T_HEADPASS_NOD_D   FX(6.0)    /* header pass: nodded down to feet up to this far, ... */
#define T_HEADPASS_NOD_VZ  -FX(0.5)
#define T_HEADPASS_VZ0     FX(1.0)    /* ... further: vz = 1.0 + 0.15 d, dropping to ... */
#define T_HEADPASS_VZ_K    FX(0.15)
#define T_HEADPASS_RECV_Z  FX(0.3)    /* ... this height at his predicted point */

/* ---- tackles & bodies (CP0.3) ---- */
#define T_TACKLE_START     FX(1.6)    /* J with an opponent on the ball this close = a standing tackle */
#define T_TACKLE_WINDUP    6          /* ticks from the lunge to the foot arriving */
#define T_TACKLE_LUNGE     FX(3.0)    /* extra speed toward the ball when lunging (m/s) */
#define T_TACKLE_REACH     FX(0.8)    /* the ball must be this close to the tackling foot at contact */
#define T_TACKLE_BLOCK_V   FX(1.5)    /* a won tackle sends the ball back toward the tackler this fast (+60% his speed) */
#define T_TACKLE_STUN      24         /* the tackled player can't touch the ball again for this long */
#define T_SLIDE_START      FX(3.0)    /* sliding tackle (D-043): their carrier this close ... */
#define T_SLIDE_SPEED      FX(8.5)    /* ... the slide starts at least this fast ... */
#define T_SLIDE_DECEL      FX(9.0)    /* ... and grinds to a halt (m/s^2) */
#define T_SLIDE_FOOT       FX(0.85)   /* leading foot ahead of the body */
#define T_SLIDE_REACH      FX(0.75)   /* ball this close to the foot = won */
#define T_SLIDE_FOUL_REACH FX(0.55)   /* a man this close to the foot before the ball = foul */
#define T_SLIDE_FIRST      4          /* the foot is live from this tick ... */
#define T_SLIDE_WIN        28         /* ... to this one */
#define T_SLIDE_TICKS      70         /* whole slide incl. getting up (1.17 s): committed */
#define T_SLIDE_KNOCK      FX(5.0)    /* a won slide knocks the ball on this fast */
#define T_TACKLE_TICKS     30         /* whole tackle action (recovery included) */
#define T_PLAYER_GAP       FX(0.6)    /* two players' bodies never overlap closer than this */

/* ---- team plan (team_plan.c, D-030) ---- */
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
#define T_AI_AIR_GAUGE     36         /* AI first-time strikes (D-051): as a human holding this many ticks (volley h) */
#define T_AIR_CONTEST_SLACK 4         /* AI aerial choice (D-051): an opponent's contact this many ticks after ours ... */
#define T_AIR_CONTEST_SPAN 12         /* ... still contests it a little; this many before ours = fully contested */
#define T_AIR_DEF_READ     36         /* their lofted ball: a defender commits to a contact only this many ticks before it */
#define T_AIR_HEAD_GO      18         /* their ball: he runs to his attack point and starts the header this many ticks */
#define T_AIR_HEAD_AT      FX(0.3)    /* before it, once he is this close to the point (or it is this few ticks away: */
#define T_AIR_HEAD_LAST    8          /*  the header's take-off roots him) */
#define T_AIR_HEAD_NEAR    FX(3.0)    /* our ball to him: he heads it once it is this close (it comes to him) ... */
#define T_AIR_SETTLE_T     30         /* ... and he waits where it was played to until this close to a lower touch */
#define T_AIR_HEAD_TOP     FX(2.35)   /* attack point: a ball he reaches between the head band and this (a jumping
                                         head's centre, 1.7 + 0.59; higher only grazes the top of his reach) */
#define T_AIR_HEAD_MAX_T   90         /* attacking the ball with the head: contacts up to this far ahead (ticks) */
#define T_AIR_HEAD_SHOT_D  FX(14.0)   /* a header at goal from within this */
#define T_AIR_HEADPASS_MIN FX(3.0)    /* header passes to team-mates this far ... */
#define T_AIR_HEADPASS_MAX FX(14.0)   /* ... up to this (the head's budget does the rest, D-048) */
#define T_AIR_HALFVOLLEY_MAX FX(20.0) /* half-volley passes up to this */
#define T_AIR_FAR_CORNER   FX(0.3)    /* a header at goal aims at the near corner if that cuts G_ERR by more than this */
#define T_AIR_COMMIT       FX(1.0)    /* utility bonus for keeping the previous aerial choice ... */
#define T_AIR_LATCH        FX(50.0)   /* ... on our own ball a header, once chosen, is kept */
#define T_AIR_EASE         FX(4.0)    /* he goes to his contact point at min(sprint, this x the distance) m/s */
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
#define T_THROWIN_ARRIVE_Z FX(0.9)    /* a throw-in drops into the receiver at this height */
/* fouls, free kicks, penalties (D-040) */
#define T_FOUL_REACH       FX(0.90)   /* a tackle that misses the ball but whose foot is this close to a man = foul */
#define T_FOUL_BEHIND_COS  FX(0.50)   /* won from behind a running man (within ~50 deg of his back) = through him */
#define T_FOUL_BEHIND_V    FX(2.5)    /* ... when he is running at least this fast */
#define T_ADVANTAGE_TICKS  90         /* advantage: the fouled team keeps it this long = play on (1.5 s) */
#define T_FOUL_PAUSE       60         /* the whistle: the ball is dead this long before the free kick */
#define T_STOPPAGE_FOUL_S  15         /* game seconds of stoppage per foul */
#define T_PENALTY_FRAC     FX(0.1048) /* penalty mark: 11 / 105 of the length from the goal line */
#define T_AI_FK_SHOT_D     FX(24.0)   /* AI takes a direct free kick this close to goal as a shot */
#define T_AI_WALL_FRAC     FX(0.40)   /* defenders build a wall for free kicks within this share of the length of goal */
#define T_SHAPE_LAST_GAP   FX(1.5)    /* no offside rule: back line this far goal-side of their deepest man ... */
#define T_SHAPE_LAST_MIN   FX(9.0)    /* ... never deeper than this from our goal line (D-035) */
#define T_ONSIDE_MARGIN    FX(0.3)    /* AI attackers wait this far onside of the line (D-041) */
#define T_AI_RUN_GO_SPACE  FX(2.5)    /* runners go in behind when our carrier has this much room (D-041) */
#define T_ROLL_MAX         FX(18.0)   /* keeper distribution (CP0.3e): roll it along the ground up to this far ... */
#define T_THROW_MAX        FX(32.0)   /* ... throw it overarm up to this far, punt beyond */
#define T_ROLL_SPEED       FX(10.0)   /* untargeted roll */
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
