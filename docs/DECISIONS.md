# Decision log

Newest last. Each entry: ID, date, decision, why, who decided. These override `docs/DESIGN.md` where they differ.
Never delete entries; supersede them with a new entry.

**D-001 · 2026-11-14 · Language: C17** (owner). Plain C17, no engine. Matches design doc recommendation.

**D-002 · 2026-11-14 · Team size: 5v5 first, then 7v7 default, 11v11 later for cups** (owner).
The sim supports N players per side from the start (compile-time max 11). Pitch size/goal size are per-match
config (`WpMatchConfig.pitch`), verified headless with 11v11 on 105×68 m. Still 5v5-specific (to generalise when
formats arrive): kickoff formation table (players 6–11 use a placeholder spread), pitch markings in render.c
(fixed 6 m box, 3 m circle), camera zoom presets in metres (main.c).

**D-003 · 2026-11-14 · Art: tilted 2.5D "tabletop"** (owner). Mild perspective tilt; ball height shown by
lift above a ground shadow.

**D-004 · 2026-11-14 · Git: local commits at each checkpoint / meaningful work unit; never push unless asked** (owner).

**D-005 · 2026-11-14 · SDL3 3.4.8 vendored as source, built static, NOT committed** (Claude).
`third_party/SDL` is git-ignored (55 MB); `tools/fetch_deps.sh` fetches the pinned tag. Revisit (commit a
trimmed copy or tarball) before packaging so offline clones build.

**D-006 · 2026-11-14 · Renderer starts on SDL_Renderer (SDL_RenderGeometry), not SDL_GPU** (Claude).
Why: far less code for a 2D-first tabletop view; SDL_Renderer itself uses GPU backends. All drawing goes
through `src/game/render.*` so it can be swapped for SDL_GPU later if profiling demands. Owner informed at end of session #1 (2026-11-14).

**D-007 · 2026-11-14 · Simulation numeric model** (Claude).
Q16.16 fixed point (`fx` = int32), 64-bit intermediates; units: 1.0 = 1 metre, time step 1/60 s.
Sim compiled with `-fwrapv`; no floats, no libm, no pointers in `SimState`; RNG state lives in `SimState`.
The renderer may use floats (presentation only, never fed back into the sim).

**D-008 · 2026-11-14 · AI architecture = candidate actions × utility scores** (Claude, from design doc).
The instinct layer scores candidate actions from an observation vector; Phase 1's learned brain adds to the
same scores. No hard-coded state machines for decisions.

**D-009 · 2026-11-14 · Players are procedural 3D-skeleton avatars, not hand-drawn sprites** (Claude, after owner
feedback asking for more human avatars, inspired by Sensible Soccer / Google Research Football; we copy no art).
17 joints posed procedurally (run cycle driven by distance travelled, kick, jump-header, keeper stance; later
tackle/slide/dive/celebrate), drawn as soft-edged outlined capsules through the tabletop camera.
Why: any new action is a pose function, not new art; poses come from sim `action`/`action_timer` so replays animate
exactly; perspective-correct in the tilted view; appearance (skin, hair, height, build) is seeded per player, which
will later come from the player genome. Three presets (owner picks): **Modern**, **Arcade** (bigger heads, chunkier),
**Retro pixel** (same figures rendered at 1/3 res, nearest upscale). Cost: ~5% of one core for 10 players at 60 fps.
Cosmetic state (stride phase, smoothed facing, looks) is presentation-only, never in SimState.

**D-010 · 2026-11-14 · Headers: contextual jump, height-band ball contact** (Claude; owner asked that jumping for
headers be guaranteed). No extra button (keeps "simple to play"): pressing pass/shoot while a reachable ball is in
the air triggers `WP_ACT_JUMP_HEADER` (or `WP_ACT_STAND_HEADER` at head height). Players have `z/vz` in the sim
(jump apex ≈ 0.59 m). ALL ball contact is resolved in height bands — feet ≤ 0.55 m, body ≤ 1.45 m, head ≤ 2.05 m
+ jump height — never by 2D distance alone (a ball flying overhead must not be "controlled"). Delivery plan:
jump physics + avatar poses now (CP0.0b); contact bands in CP0.1 dribble/kick code; playable headers from lofted
passes/crosses in CP0.2; AI aerial duels CP0.3; corners CP0.4. Owner may still ask for a dedicated jump button.

**D-011 · 2026-11-14 · Look = Modern, default zoom = wide, headers = contextual (no jump button)** (owner).
Arcade/Retro stay selectable with P (may become a player setting).

**D-012 · 2026-11-14 · Moves are parametric, data-described actions; bodies stay kinematic** (Claude, from owner's
question about bicycle kicks / rabonas / "Messi + Neymar" styles). Three separable layers:
1. *What a move does* (sim, deterministic): an action type + small parameter vector (e.g. kick: foot L/R,
   behind-standing-leg flag, contact height band, swing direction, power, spin) + integer timing metadata
   (wind-up ticks, contact window, recovery ticks, airborne?). A rabona or bicycle kick is a new action
   entry/region of parameter space, not new engine code paths.
2. *Which move, when* (decision layer, D-008): instinct utility + learned per-player brain score candidate
   (action, params). "Style" (close control, flair, risk appetite) lives here and is what training changes.
3. *How it looks* (presentation): pose keyframes per action (code now; data-driven move library later),
   timed by the same metadata so the visual contact frame matches the sim's.
Player bodies are NOT physically simulated (no ragdoll/muscle RL): orders of magnitude more compute, breaks
"runs cool" and one-core deterministic training. Emergent techniques = new sequences/parameter combos of
primitives (design doc layer 5). New body state (e.g. body pitch for overhead kicks/dives) is added as sim fields
when a move needs it. Applies from CP0.1: kicks are implemented as parameterised actions, not hard-coded shot/pass types.

**D-013 · 2026-11-14 · CP0.1 controls and feel rules** (Claude; owner to judge in playtest).
- Keys: WASD/arrows move, Shift sprint, J (or Space) pass, K shoot (hold to charge, release), I chip (hold/release).
- Shots/chips use **target aim assist**: when you shoot within 65° of the goal, the stick picks a target —
  up/down = that side of the goal (30% of goal width from centre), straight/no stick = the far post from where you
  stand — and 12% of your angular error remains (skill). Why: 8-way keyboard aiming + an 18° "pull" scored only 21%
  from natural positions (`test_scoring` sweep); target aim scores ~52% vs a statue keeper. Tunables
  `T_AIM_WINDOW_DEG`, `T_AIM_RESIDUAL`, `T_AIM_SIDE`; analogue sticks may get finer control later.
- **No dice:** inaccuracy comes only from visible causes — the ball inherits 50% of the kicker's velocity, harder
  strikes rise more, striking across your run adds curl.
- Dribble touches follow the stick (not the slower-turning body), need foot reach (≤1 m), and push relative to the
  player's speed in the new direction. Reversing on an out-of-reach knock-on makes the player fetch it first.
- An owned ball is gathered to the foot during the wind-up, so the owner's kicks never whiff.
- **Temporary out-of-play rule** (until CP0.4): ball goes dead, then is placed just inside the line after 0.75 s.
- Other players have no AI yet (CP0.3): they stand still and block the ball with their bodies.

**D-014 · 2026-11-14 · Dribbling = rhythmic, predictive touches** (Claude, after owner feedback "dribbling feels a
bit off, worse while sprinting"; supersedes the dribble bullet of D-013). While the ball is within foot reach the owner
touches it every `T_TOUCH_GAP_*` ticks (jog 16 = 0.27 s, sprint 20 = 0.33 s; a direction change may re-touch after 4).
Each touch predicts where the player will be at the next touch (velocity accelerating toward stick × top speed) and
sends the ball to that point + `T_TOUCH_LEAD_*` (jog 0.45 m, sprint 0.75 m — well inside the 1.0 m reach), plus what
rolling friction will take. One rule handles straight runs, cuts and reversals (a reversal becomes a drag-back).
The dribble speed penalty ramps off smoothly beyond reach (no flicker); an out-of-reach owned ball is always fetched.
The avatar's forward-swinging leg flicks on each touch (presentation only). Steady-state tests (cadence band, speed
smoothness, gap < 0.9 m, cuts/reversals after 40–180-tick run-ups) guard it.

## Open (owner to decide later)
- Name: keep WILDPITCH? (working title in use)
- Business model: one-time purchase vs open-source core.
- Audio: requires `sudo apt install libasound2-dev libpulse-dev libudev-dev` then re-configure (needed by CP0.5).

## D-015 — Speed-dependent rolling friction (2026-11-15, CP0.1c)
Ground-ball deceleration = `T_ROLL_DECEL + T_ROLL_LIN·v + T_AIR_DRAG·v²` (`ball_roll_decel()` in sim_internal.h), used
by ball physics AND dribble-touch sizing so they never disagree. Fast balls bleed speed, slow balls creep to a stop.
Why: owner said a constant-decel pass "doesn't stop" (12 m/s rolled 55 m). Future pitch surfaces (wet/dry/long grass,
procedural world) = different values of these three numbers.

## D-016 — Ball looks physical: impulse bounce, no-magnet possession, visible rotation (2026-11-15, CP0.1d)
From the ball-physics research (`reports/Football game ball physics.md`, steps R1-R3).
- **Bounce:** vz' = 0.62·vin − 0.06 (0 below 0.5 m/s); horizontal loss = min(0.45·(1+0.62)·vin, 0.39·|v|).
  Continuous in landing speed (no threshold cliff); 2 m drop → 0.69 m (FIFA turf 0.6–1.0).
- **Possession never moves the ball "by magic":** stick released → step up if out of reach, then a reach-limited
  sole trap to a spot on the ball's own side (capped 2.5 m/s relative). Kick wind-up gathers to a contact point on
  the ball's side (≤ 75° round the body toward the kick), capped 6 m/s; no height teleport; body turns to meet the
  ball. Contact quality (across-body angle + off-spot distance) scales kick power, floor 0.75. Rule: any new code
  that sets ball velocity for a touch must be reach-limited, capped, and never route the ball through the body
  (tests in test_ball_control "no magnet").
- **Rendering:** presentation-only orientation rolled from the drawn displacement; 12 pentagon panels fading out
  3→9 m/s (no wagon-wheel); streak above 15 m/s; shadow slides out with height. No spin state in the sim yet
  (R4 skid-to-roll/topspin is optional later).
- `ball_roll_speed_for(d)` (inverse roll model) exists for CP0.2 weighted passes.

## D-017 — Passing & switching model (2026-11-15, CP0.2a)
- **Tap pass = targeted ground pass:** receiver = best team-mate within 55° of the stick (angle + distance score;
  keeper only within 20°). Aimed and weighted at the CONTACT tick from where ball and receiver are (0.25 s lead), so
  the passer's run never spoils it: launch = inverse roll model so it arrives at 5.5 m/s (measured ~6.0). No receiver
  in the cone → untargeted 12 m/s pass along the stick (+ inherited run). Through ball / lofted pass: CP0.2b.
- **Control follows the ball:** hand-off to the receiver on the contact tick (a whiff doesn't switch); no auto-switch
  while the pass is in flight; never switch away from the ball carrier (invariant asserted in the scripted match:
  owner is -1 or a seat's player). Loose ball → auto-switch to the nearest outfield team-mate (2 m hysteresis,
  0.5 s cooldown). Q = manual switch (nearest to ball other than you; locks auto 1.5 s).
- **Assisted receive:** the receiver runs to meet the ball on its path until the human moves the stick away from
  where it was at the pass; it steers the legs only — aim/buttons always use the human's real stick.
- Non-human movement is generated as WpSeatInput (team.c placeholder) through the same move() physics as humans.

## D-018 — Dedication (2026-11-15, owner request)
The game is dedicated: **"In the memory of my brother Shubham Thakkar ❤️"** — the owner's exact words, never
edited or "tidied". Shown as a quiet card on every normal launch (`src/game/dedication.c`: dark screen, the line
centred, a drawn red heart; ~4 s with fades, any key skips; never in harness/capture modes; `--dedication
--screenshot F.bmp` captures it). Also at the top of README.md. When menus exist (CP0.4/0.5) it also gets a
permanent line on the title screen footer and in the credits. Never on the in-match HUD.
