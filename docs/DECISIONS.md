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

## D-017 — Passing & switching model (2026-11-15, CP0.2a) — *receive/assist parts superseded by D-020; switching amended by D-050*
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

## D-019 — Through balls, lofted passes, headers (2026-11-15, CP0.2b) — *through-ball model + tap window superseded by D-020*
- **J is tap/hold:** released within 9 ticks (150 ms) = targeted pass (fires on RELEASE: costs the tap's length in
  latency, ~50-100 ms); held longer = through ball, lead 2.5→10 m ahead of the receiver (mostly toward the goal
  he attacks), weighted to arrive there at 3 m/s; white power bar appears only past the tap window.
- **L = lofted pass:** to the best team-mate toward the stick, dropped where he'll be in 0.8 s; vz = 4 + 0.22·d
  (5–9.5 m/s); horizontal speed from `ball_lob_speed_for()` (air-model inverse, agreement-tested ±0.4 m).
- **Headers (D-010 made playable):** J/K pressed while the ball comes in above body height (within 1.2 s, 2 m)
  starts a header attempt: stand header if it'll arrive ≤ 1.9 m, else a jump timed so the apex meets it. While it
  waits, the stick only AIMS and the legs step under the ball. Contact only in the head band (±0.32 m, 0.45 m
  reach); the attacker's body cylinder ignores the ball meanwhile. K: aim-assisted at goal, 11 m/s + 25% of the
  incoming pace (max 17), nodded down; J: cushioned to the best team-mate (hands control over).
- **Missed headers:** the attempt ends as soon as the ball has gone past (or someone has it): body collision back,
  player free ~12 ticks later. Auto-switch never fires while a header attempt (or kick wind-up) is pending. The
  avatar's header pose is timed from take-off (jump) / contact (stand), holding a ready stance while waiting.
- **Assisted receive meet point:** cheap 50 ms ball prediction; ground/lofted passes → the reachable point CLOSEST to
  the receiver (step across, let it come: arrives ~6 m/s); through balls → where it slows to ~3.5 m/s (run onto it).

## D-020 — Passes solve the meeting point; the receiver is steered, never abandoned (2026-11-15, CP0.2c) — *deflection switch amended by D-050*
Owner: "something off about the through ball power and passing/through ball reception" + "while a ball is being
received, and the moving key is pressed, player moves ahead without reception". Research + headless playtest:
`reports/Through ball and pass reception.md`. **Amends D-017 (receive/switch) and D-019 (through ball).**
- **Exact roll tables** R(v) (distance to stop) and T(v) (time to stop), built once from one trajectory of the real
  integrator; flight v0→va = T(v0) − T(va). Replaces the bisection.
- **Tap pass:** arrival pace 5.5 m/s + 0.25 per m beyond 5 m (max 9); leads the receiver by the REAL flight time
  (3 iterations); receivers judged where they'll be; none beyond 26 m, scored down beyond 18 m. Runners (moving away
  from the ball > 2 m/s) keep their line and run through the meeting point; others step to the closest point.
- **Through ball (hold J > 12 ticks):** intercept solve — first time t at which the ball reaches the runner's path
  point P(t) = pos + u(s·t + 0.8 m) no later than he does; arrival pace in [max(5, s−1.5), min(1.5s, 12)] and the
  hold h picks inside it (h=0 firm & early, h=1 soft & deep: the bar = how far ahead); launch ≤ 24 m/s and never
  softer than a tap to him; meet ≥ 3 m inside the goal line / 1.5 m inside the touchline. Fallbacks: firmest pace →
  runner planned 25% slower (≥ 5) → lead pass clamped inside the margins (short room near goal: he eases). Runs
  always go toward goal (a man drifting back is planned to set off forward). Hold with nobody in the cone = power
  12→20 m/s.
- **Reception:** released or unchanged stick = no opinion (the assist plays it); any other direction PICKS among the
  reachable meeting points (GRF cost t/2 + 0.3·(1−cos)); full manual only after > 120° away for 250 ms, SWITCH, the
  ball gone (6 ticks) or the pass ending. A planned runner lets the ball run until 0.2 s before the meet. Control
  reach 1.0 m, swept over the tick; the pass's receiver may stretch to 1.3 m (heavier touch). Head-on in the last
  0.5 s he closes at ≤ 4 m/s.
- **Deflection mid-pass:** switch at once to the team-mate with the best time to the ball; after any auto-switch the
  inherited stick is ignored (he chases the ball) until it changes/releases, ≤ 30 ticks.
- A planned runner lets a ball run only while it passes CLEAR of him (> 0.5 m); one coming through his body/feet is
  taken in stride (never rolls through him, D-016).
- Re-test fixes (playtest agent on 965555a): the keeper is a pass target only when no outfield team-mate fits;
  receivers are judged at a prediction kept on the pitch; the stretch (1.3 m) applies only to a ball that would
  otherwise pass out of clean reach (one coming to him rolls in to his foot); the inherited stick is ignored after a
  hand-off to the keeper and after a deflection even when control stays put (and while latched it doesn't dribble).
  Known: a team-mate beyond 26 m isn't picked (untargeted 12 m/s ball instead).
- **Owner decisions (CP0.2c playtest):** J hold stays "deeper" (bar = how far ahead); no separate through-ball key;
  a STANDING receiver (meet at his feet) is taken over by holding any direction that isn't toward the ball for
  250 ms — so you can make a run while the pass travels (quick presses still receive).
- Not built from the report's plan (yet): the 200 ms first-touch stick buffer (the touch already follows the stick
  held at control), time-based (0.25 s) switch hysteresis (still 2 m distance). CP0.3 AI: one-twos / passer
  follow-up runs, off-ball run timing, pass-lane scoring (passes still hit cones standing in lanes).

## D-021 — Pass receiver selection: coverage, two bearings, stick memory, preview ring (2026-11-15, CP0.2d)
Owner asked whether our receiver pick matches the best games → research `reports/Pass receiver selection.md`
(commercial, open-source code, HCI/aim-assist + pass analytics, empirical audit) → owner: "yes, go ahead".
Structure kept (cone + one score, as SWOS/YSoccer/GRF); ranking was fine (~2.5% wrong-mate). Changes, all in
`src/sim/pass_pick.c`:
- **Two bearings:** the stick counts as pointing at a mate if it points at him now OR where he'll be.
- **Stage B** (only when nobody is within 55°): keyboard (8-way) input also takes outfield mates to 70°, never one
  > 1 m behind the passer on a key with a forward/back component; the keeper only here, within 25° (reachable from
  some key at every bearing), never for through balls or lobs.
- **Nobody fits = a deliberate ball into space:** weighted at contact to stop 2.5 m / 1.5 m inside the lines
  (6–18 m roll), no inherited run; a held J is longer up to the line (monotone in the hold). Amends D-017/D-020.
- **Stick memory:** a keyboard diagonal released one key at a time stays diagonal for 4 ticks; a released stick keeps
  the last direction for 0.5 s before falling back to facing; reset on any switch.
- **Preview ring:** `pick_recv` per seat is recomputed every tick the player could pass (sim-owned, hashed); a pass
  reads it, so the ring under a team-mate's feet IS who gets the ball (test: 92/92 releases). Hysteresis 40 points.
- **Lane danger:** an opponent within 1.5 m of the lane costs ≤ 150 points (near-tie breaker only, never overrides
  the stick — eFootball's lane override drew a petition) and tints the ring amber.
- Ties: fewer degrees off, then lower index. All weights/constants in tuning.h.
Measured (`tools/playtest/sel.c`, opposition on, settle 60/300): keyboard pick rate 61.8 → 69.2/69.5%; untargeted
taps out of play 77 → 11.8/12.0%; stick at a mate's visible bearing → him 83.7 → 90.8/93.7%; keeper bearings
reachable 100%. CP0.3: the striker's forward key still finds nobody (team.c puts no one ahead of him); AI pass choice
(forwardness + interception race) will reuse the lane function.

## D-022 — Possession rules for a match with AI players (2026-11-15, CP0.3a)
- **Anyone can win the ball** (AI included); with `WP_SIMF_STATIC_OTHERS` non-seat players stay statues (and
  ghosts for body contact) so isolation tests keep their meaning.
- **Contested loose ball:** the eligible player with the smallest swept distance wins (not the lowest index).
- **Possession is owner-only:** a dribbled ball is blocked by other bodies; nobody can kick a ball someone else owns —
  you **tackle** it: J pressed while an opponent has the ball within 1.6 m = lunge (+3 m/s toward the ball), the
  foot arrives 6 ticks later; ball within 0.8 m of it → knocked on the way you came in, the carrier stunned 24 ticks;
  otherwise a miss and you're committed (≤ 2.5 m/s) until the 30-tick action ends. Geometry + timing, no dice.
- **Bodies block:** players never overlap closer than 0.6 m (pushed apart equally, closing speed removed).
- **Owner decisions:** when an AI team-mate (or the keeper) of a seat wins the ball, control jumps to him with the
  held stick latched; the human controls the keeper with the ball.

## D-023 — Instinct AI: candidates × linear feature utility (2026-11-15, CP0.3a)
Realises D-008 in `src/sim/brain.c` for every player no seat controls (team-mates and opposition alike):
observation → candidate actions → a fixed FEATURE vector per candidate (forward gain, openness, lane clearance,
cost, threat, first-to-ball, crowding, bias) → utility = W[action]·features (+ commitment bonus) → argmax → a
WpSeatInput (same physics as humans) or an action (kick via `sim_player_kick`, tackle). The learned per-player brain
(Phase 1) adds its own score over the same candidates/features — training changes which candidate wins, never
physics. Actions: off the ball SHAPE / SUPPORT / RUN / CHASE / PRESS / COVER / MARK / RECEIVE (+ TACKLE when the
carrier's ball is exposed in front); on the ball DRIBBLE (8 dirs) / PASS / THROUGH / SHOOT; keepers KEEP (line
between ball and goal centre, meet a shot's line, come for loose balls they reach first; catch ≤ 22 m/s within
1.3 m up to 2.3 m high). Attack-direction geometry (one code path for both teams). No dice: decisions every 6 ticks,
staggered BY ROLE (staggering by index made mirrored players decide on different ticks → measured bias), first touch
before an AI pass. Verification: `tests/test_ai_match.c` (AI vs AI, nudged starts: balance + football-like rates),
`tools/playtest/aimatch.c`, mirror-symmetry harness. Tackles (CP0.3a re-check): AI tackles a running carrier when his ball
is ≥ 0.6 m from his feet, a slow/holding carrier at ≥ 0.3 m, only on its decision beat (reaction); a won tackle
blocks the ball back toward the tackler, who recovers at once. Kickoff: the non-kicking team holds shape until the
first touch. Keeper catches capped at body height (1.45 m) until CP0.3b adds hands.
Passing vs AI (owner: "passes very slow, intercepted almost every time"; measured 40% of taps to the ringed mate
intercepted, 22% even with clear lanes, launch 10 m/s): tap arrival 8 + 0.3/m (max 12; was 5.5 + 0.25, max 9,
amends D-020), AI defenders react to a struck pass after 0.2 s, cutting out the other team's pass needs the ball
within 0.8 m (the receiver keeps 1.0). Now 20% intercepted (6% with clear lanes), launch 12.9 m/s; AI-vs-AI pass
completion 60% → 77%.
Shots vs the keeper (owner: "the shoot is getting intercepted in between"; measured: keeper took 77% of live shots,
running out to cut shots off and reacting instantly): keepers come out only for balls < 8 m/s, react to a shot
after 0.2 s and shuffle across at ≤ 5 m/s (dives: CP0.3b), catch ≤ 16 m/s within 1.0 m, body-block radius 0.36
(was 0.48). Live natural shots now score 40% (was 20%); vs a statue keeper 53%. Not yet (CP0.3b): player
stats, AI aerial duels, keeper dives/poses/hands, difficulty, AI set pieces.

## D-024 — Rebindable controls + pause menu (2026-11-15, owner request)
Defaults (owner): arrows move, **E** sprint, **S** pass (hold: through ball; near their ball: tackle), **D** shoot,
**A** lofted pass, **C** zoom. My picks (rebindable): **W** chip, **Q** switch, **P** look. Fixed: F3 debug, Esc.
**Esc** opens a pause menu (Resume / Controls / Quit; the sim is paused and the window only redraws on input).
Controls: Enter on an action, press the new key (a key already in use is swapped, never shared), R = defaults.
Saved immediately as text (`action = Key Name`, SDL key names) in the SDL pref dir (`controls.cfg`), loaded at
startup (not in harness/capture modes). `src/game/keymap.c`, `src/game/menu.c`; ctest `keymap` (`--check-keymap`).

## D-025 — 7v7 is the game's format now (2026-11-15, owner request)
Owner: "increase now 7v7 and increase the pitch size, it is getting tough to test with 5v5". `sim_config_7v7()`:
7 a side (keeper + 2-3-1), 55 × 35 m pitch, 5 × 2 m goals (1.5 m deep). The game starts 7v7 (`--5v5` = the old
format; harness modes `--autoplay*/--demo/--gallery` keep 5v5). `sim_default_config()` stays 5v5 = the regression
format (tests, golden hash). Formations are one table per format (`formation_frac`, shared by kick-offs and the AI
shape); forward runs by formation depth (not index); the striker = last index. Pitch markings and camera zoom scale
with the pitch. AI vs AI 7v7 (30 × 5 min): goals 58–60, shots 132–125, 76% passes arrive. 11v11 later (ROADMAP).

## D-026 — Keeper dives; shots keep their aim (2026-11-15, CP0.3b)
- **Dive** (`src/sim/keeper.c`, decided in brain.c): after his 0.33 s reaction, if a shot will cross his line more
  than 0.7 m from him and arrive within 0.55 s, the AI keeper dives sideways (3 m/s, small hop). His body is a
  segment from his centre toward the dive side that STRETCHES to 1.0 m over 15 ticks (+0.25 m hands); a ball crossing
  it ≤ 2.2 m high is caught (≤ 18 m/s at body height) or parried out from goal. Down for 54 ticks. Pose: arms
  overhead, rolled onto his side (avatar `roll`); tackles got a lunge pose.
- **Shots keep their aim:** shots/chips inherit only the run velocity ALONG the strike (a run at goal no longer
  drags a corner-aimed shot ~1 m to the middle); aim-assisted shots go 0.6 m inside the post (scales with the goal).
- Control never stays on our keeper once he's played/lost the ball (the AI keeps goal).
7v7 live shots 7–16 m out (wide angles incl.): goal 36% (weak 13% / medium 43% / hard 52%), was 19%.

## D-027 — AI aerial duels and crosses (2026-11-15, CP0.3b)
AI players go for headers (`header_start` from brain.c): only the team's first man to the ball, on his decision beat
— at goal if within 14 m with a shooting chance, a clearance upfield/wide in our third, otherwise nodded on (pass
pick finds a mate). The AI's aim is its `ai_goal` (not its movement stick). Head contact is resolved after everyone
moves: of all heads that can reach the ball this tick, the NEAREST wins (no index-order bias); the legs-only step
under the ball (`header_assist_move`) applies to AI and humans. AI carriers wide in the attacking third get a CROSS
candidate (lofted pass to a team-mate within 13 m of goal). AI vs AI: ~3.5 crosses/min, ~2 headers/min (7v7).

## D-028 — Difficulty: per-team AI level (2026-11-15, CP0.3b)
`SimState.ai_level[2]` (hashed; `WpMatchConfig.ai_level`), EASY / NORMAL / HARD changes only reaction and decision
speed (no dice, no stat boosts): decision beat 9/6/4 ticks, reaction to a struck pass 18/12/8, keeper reaction to a
shot 26/20/15, tackle needs the carrier's ball 0.75/0.6/0.5 m exposed. NORMAL reproduces the previous behaviour bit
for bit (verified against the old goldens with the field unhashed). The game applies the chosen level to the
OPPOSITION; our AI team-mates stay NORMAL. Esc menu: Difficulty < Easy | Normal | Hard > (saved to `settings.cfg`,
separate from `controls.cfg`), applied by "Restart match". EASY vs HARD AI (7v7): goals 15–21, possession 44–56%.

## D-029 — Keeper's hands, throw-ins, corners, goal kicks, defender auto-switch (2026-11-15, owner request) — *defender switch amended by D-050*
- **Hands:** a keeper who catches/gathers the ball inside his own box holds it (`WpBall.held`): chest height, no
  ball physics, can't be tackled, can't leave the box, 6 s limit (then released). Distribution: S throw (to the
  ringed mate), A high throw, D/W punt; the AI keeper looks up ~0.8 s then throws to a free man or punts. Outside
  his box a keeper has no hands. Opponents don't press a keeper holding it.
- **Restarts** (`src/sim/restart.c`): touchline → THROW-IN for the team that didn't touch it last, from the
  crossing point (thrown from the hands: S / A, no punt); goal line → CORNER if the defenders touched it last, else
  GOAL KICK (keeper, 2.5 m out, centred). After the 0.75 s pause (everyone repositions; the ball is dead) the
  nearest player of that team is the taker: stands still (can turn), the other team is held 4 m away until it's
  played, control jumps to him if he's ours. AI takers: throw to an open man, cross a corner once two team-mates
  are in the box (attackers take near-post / penalty-spot / far-post spots), goal kick short or long. The HUD
  shows THROW-IN / CORNER / GOAL KICK and the team. Replaces the temporary "placed back inside" rule.
- **Defender auto-switch:** when they have the ball (or their pass is travelling) control moves to the team-mate
  nearest a spot just goal-side of the ball if he's > 3 m closer than the current man (0.75 s cooldown).

## D-030 — Team plan + ball-dependent formation: both teams play as a unit (2026-11-15, owner request)
Owner: "right now it is like everyone going towards the ball". Research: `reports/Team shape and collaborative AI.md`.
- **Team plan** (`team_plan.c`, `WpTeamPlan plan[2]` in SimState, hashed): once per tick each team assigns roles
  BEFORE any player decides — exactly ONE ball-winner (fastest time-to-ball, 15% hysteresis, frozen while reading
  their pass; a human seat counts only if he is fastest and engaging), one cover (behind the winner, goal-side),
  capped zonal markers (5v5: 1; 7v7: 2, 3 near our goal) for men inside a zone around their spot, rest defenders
  (counter-risk 1..3 stay behind the ball when we attack), one near-support and one far-side runner. Heavy part every
  3 ticks; a counter-press window (2.5 s) lets the cover press too after losing it.
- **Formation** (`formation.c`): two parametric blocks (in / out of possession: back-line height from the ball,
  length, width, lateral shift toward the ball side, clamps) with slots (u, v, mindset), blended by `poss_fade`
  (+1/60 per tick when we have it, −1/30 when they do, holds on a loose ball) — the shape slides and breathes with
  the ball. Out of possession non-strikers are clamped goal-side of the ball. Distances scale with the pitch.
- **Brain:** the plan gates candidates — only the winner may CHASE/PRESS (plus a 2.5 m escape), only the cover
  COVERs, MARK only the assigned man or men near my spot, new REST action, SUPPORT points around my shape spot + a
  near-support ring, RUN for the striker and the runner. New features F_ROLE (does this action match my plan role)
  and F_HOME (distance from my shape spot). D-008 holds: the plan only picks which candidates exist and adds
  features; utility still chooses.
- **Measured (7v7, AI vs AI, `shape_metrics.h`, printed by test_ai_match):** on the old 55 m pitch team length
  11 → 19 m, area 89 → 223 m² in possession / 172 out, players within 10 m of the ball 4.0 → 2.3, def–mid gap
  1.5 → 7.9 m, goal-side 2.0 → 2.65. On the new 80 m pitch: length ~25 m, width ~28 m, area ~430 / 380 m²,
  near-ball 1.7 / 2.0, goal-side ~2.9 of 6. AI vs AI 7v7 (30 × 5 min): goals 58–53, shots 109–99, 62% passes arrive.
- **No offside yet (until CP0.4):** back-line slots (mindset 0) stay 1.5 m goal-side of the opponents' deepest
  outfield man (never deeper than 9 m from goal). Without it, strikers camped behind the line and AI shots were
  breakaways (shooters had 0.5 defenders goal-side; 35/57 with nobody within 3 m → 15).
- **Mirror-fair plan:** both teams' heavy plan update runs on the same tick (a per-team stagger made the exact-mirror
  check diverge at tick 1). AI shots now carry an aim error (uniform ±(0.3 + 0.05/m + 0.6 under pressure) m at the
  goal line, one `SimState.rng` draw) instead of perfect far-corner placement; shot quality falls off quadratically
  (shoots from the box edge). AI corner takers wait up to 7 s (`restart_ticks`) for two team-mates in the box.

## D-031 — Pitches 1.5× bigger, real proportions (2026-11-15, owner request)
Owner: "increase both for all formats by some factor (1.5) but ensuring proportion matches that of actual soccer
fields". Real pitch 105 × 68 = 1.544. **7v7: 80 × 52 m** (was 55 × 35), goal 5.6 × 2.1 m. **5v5: 60 × 39 m** (was
40 × 25), goal 4.2 × 2 m — goal width = 7.32/68 of the pitch width, like a real pitch (6 m goals made 58–83%
of AI shots score). Boxes, circle and markings scale with the pitch; zoom view scales with length. The 40 × 25
`sim_default_config` stays ONLY as the regression/harness pitch (5v5 golden, scripted tests); test_ai_match now
plays the real game formats. AI shooting range 22 m, cross zone 16 m, shot utility re-weighted for the longer field.

## D-032 — Back-pass rule; the kick-off is a pass-only set piece (2026-11-15, owner request)
- **Back-pass:** `WpBall.no_hands` (= team+1, hashed; fits the struct's padding) is set when a player kicks the ball
  with his feet or throws it in (and on his dribble touches), and cleared by a header, a body deflection, a tackle,
  anyone controlling it, or a restart. A keeper (`keeper_hands()`) uses his hands only in his own box AND when
  `no_hands` isn't his team's: a team-mate's deliberate pass is played with the feet (no catch, no dive save).
  Headers / chests / deflections back to him may be picked up, as in Law 12.
- **Kick-off:** after every goal (and, in the game, at the start) `sim_kickoff_setpiece()` makes it a set piece
  (`WP_RS_KICKOFF`): the kicking team's striker stands on the ball (it's dead, can't be dribbled), every player stays
  in his own half and the other team outside the centre circle (radius 9.15/105 of the pitch length, drawn the same)
  until it's played; control jumps to the taker if his team has a seat; the AI taker passes after ~0.7 s. The team
  that conceded kicks off (unchanged). `sim_init` keeps a free ball for tests/drills. Halves (CP0.4) will alternate
  the opening kick-off.

## D-033 — Advertising boards, stands, corner flags (2026-11-15, owner request)
Boards stand 3 m outside the touchlines and 4.5 m behind the goal lines (room for the nets), 0.9 m tall
(`T_BOARD_*`). In the sim players are clamped inside them and a ball below their top rebounds (30% speed kept); a
higher ball flies over. `src/game/stadium.c` draws the stands' concourse, the boards and four corner flags (flags
blow outward). Each board is panels of "WILDPITCH" + a football logo, and at its right-hand end (as seen from the
pitch) the owner's dedication, two lines in a smaller font with the red heart: the exact D-018 words. Board faces
are two textures made once with the debug font (no new dependency); `--boards --screenshot F.bmp` captures them.
At the wide zoom the far boards are small; the dedication reads best on the near board and at closer zooms.

## D-034 — Player stats (2026-11-15, CP0.3c)
- **Seven stats, 1..99, 50 = the old all-equal player:** pace, control, passing, shooting, tackling, stamina,
  keeping. Stored in `SimState.stats[player][WP_STAT_*]` (kick-off resets don't touch them). `stat_k` = (s-50)/50.
- **Modes (`WpMatchConfig.stats_mode`):** NEUTRAL (everyone 50, every effect off, no RNG draw: bit-identical to the
  pre-stats sim; stats unhashed - verified, both goldens unchanged by the plumbing commit), MIRRORED (team 1 = team 0
  slot by slot: the AI balance tests), OWN (each team generated; the game). Generation (`stats.c`): a slot template
  (keeper / defender / midfielder / striker, equal outfield totals) +-10, zero-sum per stat over the squad, so both
  squads total exactly the same. Difficulty still changes no stats (D-028).
- **Effects:** pace = top speeds x (1 +- 7%); stamina = energy drains 4%/s sprinting (x(1-0.4k)), recovers 2%/s
  jogging (x2 below 3 m/s), below 40% the sprint's extra pace fades to 25% (AI settles ~75% energy); control =
  first-touch spill x(1-0.5k); passing = pass arrival pace x(1+0.1k) (good passers zip it); shooting = power x
  (1+0.06k), the aim-assist target nearer the post (human: deterministic), AI aim error x(1-0.6k); tackling = reach
  x(1+0.15k), the AI needs the ball less exposed; keeping = reaction -8k ticks, dive reach x(1+0.25k).
- **No random scatter on the human's passes or shots** (D-021: the pass goes to the ringed man); the only random
  draw remains the AI shot aim error. One `player_top_speed()` is used at every speed assumption (movement,
  dribble predictor, steering, team plan, meet prediction, receive assist, AI caps).
- **AI:** stamina-aware (below 55% energy it jogs unless pressing / chasing / covering; runs in behind cost legs);
  good finishers value shots more. HUD: the controlled player's stats + energy bar (`hud.c`).

## D-035 — AI pass choice by time, dribbling under pressure, keeper narrows the angle (2026-11-15, CP0.3c)
Baseline (`passfail.c`): 61% of AI passes arrived; >20 m passes 28% (61% cut out in flight) - selection, not skill.
- AI pass / through-ball lanes are judged by TIME (`lane_time`): clearance left after each opponent runs at the
  ball's path (3 m/s after 0.3 s) for as long as the ball takes to get there. Pass utility: lane weight 0.8 -> 1.6,
  forward weight 0.16 -> 0.13 (through 0.20 -> 0.18).
- Dribbling under pressure costs utility (an opponent within 3 m), so carriers pass instead of running into tackles.
- The keeper comes off his line to 30% of the distance (max 3.5 m) against an opponent carrying it within 16 m.
- AI shot aim error base 0.3 -> 0.5 m, under pressure 0.6 -> 0.8 m.
Result (7v7, 20 x 5 min, stats on): passes 61% -> 78% complete (>20 m 28% -> 47%), goals ~0.5/min, shots ~1/min,
conversion ~60% -> ~45-50% in 7v7 (5v5 ~25%). Still high from close range: 1v1 smothers are follow-up work.

## D-036 — Match clock at 9x, halves, change of ends, stoppage time (2026-11-15, owner request)
Owner: "clock running at 9x, so 5 mins both halves, ensure that side changes after each half, also add the extra
time thing" ("extra time" clarified = stoppage time).
- `WpMatchConfig.half_ticks` (game formats: `T_HALF_TICKS` = 18000 = 5 real minutes = 45'; the clock shows 9x).
  0 = an endless match (tests, drills, regression config): no clock, nothing extra hashed (5v5 golden unchanged).
- The clock runs through goals and restarts. Goals add 45 game-seconds, throw-ins/corners/goal kicks 8 to the
  stoppage pot; at 45' it is announced in whole minutes (1..4) and the half ends at the first live moment after
  (`WP_EV_WHISTLE`). Half time: 3.5 s pause ("teams change ends"), then the teams swap ends and the team that didn't
  kick off the match starts the second half. Full time: the match freezes, the banner says how to restart.
- Every "team 0 attacks +x" assumption now goes through `team_dir(s, team)` (flips in the second half): formation,
  team plan, brain, keeper box, aim assist, through-ball solver, kick-off placement, restarts, goal crediting
  (refactor verified bit-identical before the clock was switched on).
- HUD: `27:15   1ST HALF`, `45:00 (+3)` once announced, `45+1:12 (+3)` in stoppage; HALF TIME / FULL TIME banners.

## D-037 — Movement assist toward the ball (2026-11-15, owner request + research) — *cone amended by D-049, switching by D-050*
Research: `reports/Assisted movement toward the ball.md` (EA FC / eFootball settings, SWOS port, YSoccer and Google
Research Football source, assist literature, WildPitch audit). Owner decisions (AskUserQuestion): AI team-mates'
passes get the receive assist even with the stick released; a released stick never chases a loose ball; an Esc-menu
setting Off / Low / Normal / High, default Normal.
- **Chase cone** (`seats_chase_assist`, legs only, after the D-020 receive assist): a loose ball, a stick held past
  48/127, no pass travelling to another team-mate, the ball is "his" (his earliest meeting time vs the 3 team-mates
  nearest it: full weight if he's first, fading to 0 when one is 0.3 s quicker). The stick's heading is turned
  toward his meeting point (`meet_chase`: body-height points only, grass bounces modelled, reach at the speed he is
  actually running) by w x min(|delta|, F): full within F = 30 deg, fading linearly to exactly 0 at E = 55 deg of the
  ball or the meeting point (Low 20/40, High 40/70). Never sprints for him, never changes his speed, buttons/aim stay
  his. Measured (test_chase_assist, 8-way keys onto a still ball): 3 m 56/72 -> 72/72, 5 m 40/72 -> 72/72.
- **AI pass to the human:** the same receive assist as his own passes; his stick at the kick is the "no opinion"
  reference (a runner keeps his run). **Latch** survives analogue jitter (T_RECV_STICK_TOL). **Loose-ball
  auto-switch by time** to the ball (YSoccer's 0.5x hysteresis), no longer blocked by a pass to the other team.
- **Off = the pre-D-037 sim bit for bit** (verified: goldens unchanged with every seat Off, then Normal turned on).
  Per seat `SimState.assist_level` (hashed when any is on); the menu applies it on Restart match.

## D-038 — Restarts like real matches; keeper distribution (2026-11-16, CP0.3e, owner feedback)
- **Throw-in** (Law 15): taken from over the head (ball at 2.0 m between both hands, two-handed release, feet
  planted; avatar poses), opponents 2 m away, the throw arced to drop into the receiver at ~0.9 m. **Corners:**
  opponents 9.15 m scaled to the pitch (7.0 m on 80 m). **Goal kicks** (Law 16): from the goal-area line (5.5/105
  of the length) in line with the post on the side it went out; every opponent outside the penalty area until it's
  played. **Receiving shape** (`setpiece.c`): for throw-ins (down the line, inside, back, long) and goal kicks
  (split wide of the box, wide and higher, a midfielder showing) the nearest team-mates take the spots.
- **Keeper distribution:** roll along the ground up to 18 m, overarm throw up to 32 m, punt beyond (AI); human
  keeper: S rolls to the ringed mate when he's within 18 m, else throws overarm; A throws overarm; D/W punt.
  New kick kind `WP_KICK_ROLL`; poses: underarm roll, overarm throw, drop-kick punt; keeper cradle now shows.
- A held ball with no holder drops (never frozen) - found by the golden's all-AI quarter.

## D-039 — Through-ball meter, sprint dribble, receiving, carrier press (2026-11-16, CP0.3e, owner feedback)
- **Through ball:** tap threshold 12 -> 20 ticks (0.33 s). Every through ball is met in stride (solved per
  candidate point) AND struck >= 1.1 x a tap to the same man (untargeted too); the hold picks the depth inside the
  window of points that satisfy both (physics: deeper in stride = softer, so "more meter = more power" is kept as
  "every meter level is firmer than a pass").
- **Sprint dribble:** a knock-on (lead 1.3 m, touch every 28 ticks); slowed only for 8 ticks at each touch, full
  sprint between; turning while it's ahead = run onto it first, then the touch goes the new way.
- **Receiving:** no takeover once the ball is < 0.6 s away - a held stick is the first-touch direction.
- **Chase cone (D-037) on an opponent with the ball:** a stick held roughly at the carrier (<= 12 m) is turned
  (same cone and cap) toward where you meet him; on an opponent's pass in flight it already applies.

## D-040 — Fouls, free kicks, penalties, advantage (2026-11-16, CP0.4)
- **Fouls** (`tackle.c`): a tackle whose foot misses the ball but comes within 0.9 m of an opponent, or that wins
  the ball through a man running >= 2.5 m/s from behind (within ~60 deg of his back). AI vs AI: ~0.2-0.4 fouls/min.
- **Advantage:** a missed-ball foul while the fouled team still has it = play on (HUD "ADVANTAGE"); if they lose it
  within 1.5 s the free kick is given after all.
- **Free kick** (`restart_foul`): direct, at the foul; opponents 9.15 m scaled; the defending AI builds a wall (2 in
  5v5, 3 in 7v7) when it's within 40% of the length of goal; the AI taker shoots from <= 24 m with a decent angle,
  otherwise passes. **Penalty** for a direct foul in the box: from the mark (11/105 of the length), the team's best
  finisher takes it, everyone else out of the box and the D, the keeper on his line.
- `WpMatchConfig.rules` (`WP_RULE_FOULS | WP_RULE_OFFSIDE`): on in the game formats, off on the bare regression
  pitch (isolation tests have no real defence). The half never ends in the middle of a set piece or advantage.

## D-041 — Offside (2026-11-16, CP0.4)
`offside.c`: at every kick / header, team-mates beyond the ball and the second-last opponent in the other half are
flagged (level = onside); if a flagged man is the next to play it - before an opponent, or an onside team-mate /
the passer taking a new touch - offside: indirect free kick where he played it (HUD "OFFSIDE"). Throw-ins, goal
kicks and corners can't be offside. A goal straight from an indirect free kick is a goal kick. AI: never passes to
a man in an offside position, waits on the line (0.3 m onside) and times runs in behind when the carrier has
2.5 m of room; formation spots stay onside. With offside on, the D-035 "last man" stand-in is off. Measured
(7v7, 20 x 5 min, stats on): rules off 54 goals / 109 shots; fouls + offside 42 / 92 (no more camping in behind).

## D-042 — Gamepad, instant replay (2026-11-16, CP0.4, subagent)
- **Gamepad** (`gamepad.c`, SDL3 gamepad, hotplug): left stick (radial deadzone 0.2) / D-pad move; South pass /
  tackle, East shoot, West lob, North chip, RB or RT sprint, LB switch, R3/L3 zoom/look, Back replay (slow-mo while
  replaying), Start menu (D-pad / A / B drive the menu). Keyboard and pad together: the keyboard's direction wins,
  buttons OR. Pure mapping unit-checked (`--check-gamepad`); untested on real hardware.
- **Replay** (`replay.c`): the last 10-12 s re-simulated from a SimState snapshot (every 2 s) + the logged
  WpInput of every tick (~42 KB static); R / Back starts it, R / Back toggles slow motion, any other key returns to
  the paused live match (never written). `--check-replay` (ctest) proves the replay hash equals the live run tick by
  tick - it fails if sim_step ever depends on anything outside SimState + WpInput.

## D-043 — Sliding tackle on the lob key (2026-11-16, owner request)
Owner: "add sliding tackle, think carefully with the advisor about which key". EA FC puts the slide on its lob/cross
button (X/Square), eFootball on its lofted-pass/cross button (Circle); our LOB key (keyboard A, pad West/X) is also
the only action key that's idle off the ball. Contextual: only with an opponent on the ball within 3 m (and not just
after being tackled), so a lob press can't become a slide by accident. The slide: >= 8.5 m/s along the grass,
decelerating at 9 m/s^2; the leading foot (0.85 m ahead) wins the ball within 0.75 m between ticks 4 and 28 and
knocks it on; reaching a man before the ball, or through a running man from behind, is a foul (a penalty in the
box); 1.17 s on the ground - committed. Human-only for now (AI slides = tuning AI fouls; later).

## D-044 — Restart transitions, kick-off hold, zone-aware AI, passes that stay in (2026-11-16, owner feedback)
- **Transitions** (`WP_PHASE_SETUP`, restart.c): after the whistle/OUT pause the ball is placed and the taker RUNS
  to it while everyone walks into place (the AI heads for legal spots: out of the box for goal kicks, 9.15 m
  scaled for corners / free kicks, the line-up for penalties); the restart goes live once the taker is there and
  nobody (AI) is still inside a zone, at most 4 s, or at once when a human presses an action button (skip).
  Statue isolation tests keep the instant restart. The clock runs (stoppage covers it).
- **Kick-off:** nobody but the taker moves until the ball is played (owner: "why does the opponent team start moving
  even before the first pass").
- **Zones:** during a restart an AI player's movement target is projected out of the zone it may not enter (no more
  walking into the box at a goal kick and being pushed back).
- **Passes near a line** (owner: "the ball is passed outside of the field because prediction goes out"): the
  receiver lead is kept 2.5 m / 2.0 m inside the goal / touch lines (was 1 m), and the arrival pace is capped so a
  missed pass stops inside the pitch (>= 4.5 m/s). AI vs AI 100 min: outs after passes 107 -> 39 (completion
  79% -> 75%: softer balls near the lines get cut out a bit more).
- **Offside display:** the line where it was when the pass was played (`SimState.offside_x`) is drawn across the
  pitch during the stoppage, with a big "O F F S I D E" banner.
- test_ai_match shot floor 0.7 -> 0.5/min (restarts and their transitions take clock time).

## D-045 — Lofted-pass power meter (2026-11-16, owner request)
Owner: "there should be power meter in lofted pass also, that will decide how far to lob in air". A (lob) now uses
the shared hold/release mechanic: a tap (<= 12 ticks) is the old lob to the ringed team-mate; a hold shows a violet
meter and the ball lands lob_meter_dist(h) = 12 .. 45 m along the aim - a team-mate within 4 m (+12%) of that spot
gets it (led onto his run), otherwise it drops into space. Targeted lobs now lead the receiver by the real flight
time (~2 vz/g; was a fixed 0.8 s). Measured: 17.5 / 25.7 / 34.0 m on the meter land at 18.7 / 27.5 / 36.2 m. AI lobs
and crosses are unchanged (no meter). Also: AI shot utility weight 24 -> 32 (D-041 cost back: 49 goals / 110 shots
per 100 AI minutes, vs 54 / 109 before offside).

## D-046 — Lofted passes from one flight table: unclamped loft, predicted snap, drop-point hand-off (2026-11-16, CP0.5a stage 1)
Report `reports/Aerial ball control and lofted passes.md` ("The lob meter: keep D-045's controls, fix what it
inherited"). Owner decisions: D-045's controls stay (tap A = auto lob to the ringed mate, hold = meter = distance along
the aim with a receiver snap), no landing ghost ring, meter range 12 m .. min(45 m, 0.62 x pitch length).
- **Trajectory:** vz = 5.0 + 0.245 D (D = where it first lands), no 9.5 m/s clamp: ~45 deg at 10 m easing to ~35 deg at
  50 m (the old clamp turned every lob past ~25 m into a 15-21 deg drive).
- **Table (`src/sim/lob.c`):** 64 entries (D = 0..47.25 m, 0.75 m steps): launch vh (bisection over
  `ball_flight_to`), and the distance / time at which the ball comes down through the receiving height. Filled once
  by `sim_init` from fixed-point code only (a pure function of tuning.h: identical on gcc/clang, immutable afterwards,
  not in SimState, replays unaffected); lookups interpolate linearly. Meter landings: within 0.37 m of D on the 40 /
  60 / 80 m pitches (was ~7% long).
- **Meter:** D(h) = 12 m .. lob_max_d = min(45, 0.62 L) (24.8 / 37.2 / 45 m). Snap on PREDICTED points: each
  outfield mate's P_j = q_j + v_j t_c (3 iterations), miss = |P_j - land| + 6 m x angle/pi, accepted within
  4 m + 12% of D; the snapped mate gets the ball on P_j with the meter's surplus / shortfall as a depth bias along his
  run clamped to +-3 m. Untargeted: D along the aim, shortened along the ray to stay T_THROUGH_MARGIN_X/Y inside
  (a carrier who dribbles the ball past the margin and lobs outward gets D = 0: it pops up at his feet and his
  momentum can still take it over the line - 14 of 120 such scripted cases on the 40 m pitch; known). Every targeted point
  is clamped by the same margins. "Never out of play" = the first bounce (a 20 m/s ball may still roll out after).
- **Lead:** targeted lobs are led to where the ball comes DOWN through z = 1.0 m (T_LOB_RECV_Z), not the grass; the
  solved point P and t_c go to `seats_pass_handoff` with WP_PLAN_RUN, so the receive assist (and the AI receiver) runs
  to the drop point. Receiver picked at the release, P / bias / t_c solved at contact.
- **AI:** AI lobs, crosses and restart lobs use the same solve with D clamped to [12 m, lob_max_d]
  (T_LOB_AI_MIN_D); `do_kick` sets power 0 for lobs (power = h). An AI ball to a mate within T_AI_CROSS_ZONE of the
  goal is met at T_LOB_HEAD_Z = 2.3 m (a jumping head) instead of 1.0 m - a second table column. Measured: led to
  1.0 m, crosses lost the aerial duel (the receiver waited at the drop and the marker jumped in front: opponent
  headers 25 -> 71 per 400 min, lob completion 69% -> 62%); 1.8 m: 67%; 2.3 m: 70%. This is a stopgap until stage 4's
  driven cross class.
- **From the hands** (keeper's overarm throw, long throw-in): unchanged, the pre-D-046 clamped loft
  (T_THROW_LOFT_VZ_*).
- **Avatar:** the lob strike poses kick_power01 = h (taps / AI: 0.5 as before).
- Precedence unchanged: slide (D-043) wins the LOB press edge with their carrier within 3 m; ball in the hands is a
  throw. Tests: `tests/test_lob.c`.

## D-047 — Aerial control by stance: prediction ring, contact plans, graded touches (2026-11-17, CP0.5a stage 2)
Report "Action selection" / "Per-stance contact physics" (CONTROL intent only). New `src/sim/contact.c`
(+ `contact_internal.h`):
- **Bands at the planned contact** (tuning.h `T_CB_*`): GROUND <= 0.25 m trap (the old first touch, unchanged), LOW
  0.25-0.55 instep cushion, MID 0.55-1.0 thigh, CHEST 1.0-1.5 chest, HEAD 1.5-2.05 header-control only for a slow
  ball (3-D relative speed <= its v_clean 11 m/s; owner: "a slow head-high ball = cushion it, a fast one deflects"),
  JUMP 2.05-2.6 (headers, unchanged).
- **Prediction ring:** 180 ticks of the untouched ball from ball.c's own integrator (`ball_predict_tick`; integrate()
  / boards() now take a `WpBall*`), kept in a static cache keyed on the complete ball state + pitch. A hit is the
  suffix of the same sequence topped up to the full horizon = exactly a fresh fill, so replays, snapshots and several
  sims stay deterministic. Only while the loose ball is in the air. Derived data: not in SimState.
- **Plan per player** (`WpPlayer.plan`, hashed: kind, band, flags, contact tick, first-planned tick, point, z) for the
  pass's receiver, seat players and each team's ball-winner: candidates = reachable ring ticks (0.8125 x sprint after
  0.15 s, + 0.6 m), score -4 difficulty - dt/30 - 2 pressure, difficulty = stance base (head-control 0.5: let it
  drop to the chest) + speed over v_clean + 0.6 setup deficit; frozen 5 ticks before contact. A planner may let an
  early contact go by (`contact_waiting`) only while the ball is > 0.45 m off and still coming.
- **One predicate** (`contact_stance`) for the first touch (gain_check), the body (collide_bodies exemption; keepers'
  hands keep the old rule) and possession while settling. Above 1.6 x v_clean (head: v_clean) the body deflects it.
- **Touch = outcome + residual:** clean ball pops h_pop and drops d_ready along the exit (stick, stick within 12
  ticks, or facing) by the time it is down to 0.30 m, plus his run; residual (1-q) e_firm x the 3-D relative velocity
  mirrored about the stance normal; then +-6 deg x (1-q) (one RNG draw, only when q < 1: "Readable", owner).
  q = (1 - over - 0.6 setup deficit - 0.3 pressure - stretch)(1 + 0.25 k_control), over from the 3-D speed.
  Stance rows (h_pop, d_ready, e_firm, v_clean, setup, normal, reach, base): instep .05/.35/.5/13/12/70/.9/.1, thigh
  .12/.5/.55/15/18/60/.6/.05, chest .1/.6/.55/17/24/35 (5 driven)/.45/0, head-control .1/.45/.7/11/15/45/.45/.5
  (reaches and head d_ready trimmed from the report so the drop is 0.3-1.0 m from the feet).
- **settle_ticks** (hashed): after an aerial control touch he owns the dropping ball up to 2.1 m for its fall time.
- **Movement:** the human's receive assist runs to the plan's point in time. The AI keeps its own receive / chase
  targets until stage 4 (moving AI receivers to the control point made them head crosses away: lob completion 59%);
  its touches use the stance model.
- Avatar stance poses deferred to stage 5. bench_sim 15.6 -> 16.8 us/tick (+8%).

## D-048 — Intents and chains: buffered presses, first-time contacts, the header law (2026-11-17, CP0.5a stage 3)
Owner decisions: a button pressed early = a first-time action, falling back to control and then the action if the
first-time contact isn't possible; "Readable" error. New `src/sim/intent.c`; first-time strikes in `contact.c`.
- **Intent buffer per seat** (SimState, hashed): PASS / SHOOT / LOB pressed while the ball is loose and not yet
  playable records {kind, held, gauge, age, predicted contact tick, aim at the press}; the newest press wins.
  Cancelled 90 ticks after the release (not while he holds a settling ball - the chain waits), when the predicted
  contact slips > 42 ticks, when anyone else gets the ball, on a switch, when play stops. A buffered LOB never
  becomes a slide. The AI has no buffer (stage 4).
- **Fallback order:** (1) first time - a header if it reaches him >= 1.6 m (T_INTENT_HEAD_Z; also the threshold for
  a press at a high ball, was 1.30 m: a chest-high ball with PASS/SHOOT is now chested, not headed), a volley /
  half-volley / volley pass / volleyed lob at the aerial contact, a chest lay-off (PASS, mate <= 8 m within 60 deg),
  or the kick the moment it is at his feet; (2) otherwise the stance control touch with the intent kept, so chest ->
  pass fires as the ball reaches his feet (a ground pass, same pace as from a still ball) and chest -> volley fires as
  it drops through 1 m. The aim is the stick at the strike, else the aim at the press.
- **Volley:** min(T_SHOT_MAX, 0.9 x the set shot + 0.36 x the ball's pace into it), aim error +-2 deg x (2 LOW /
  3 MID) x (1 + over) (one draw), rises 0.15 rad per metre above 0.45 m (the report's 0.35 sent every chest volley
  over). Half-volley / volley pass: the ground pass x 1.05 struck down at vz -0.5; a MID volley pass only off a ball
  <= 10 m/s within 60 deg of facing.
- **Header law:** the wanted ball is capped by sqrt((0.63 u_n + 1.63 v_head)^2 + (0.85 |r_t|)^2) about the bisector
  normal (v_head 2.5 standing / 3.5 jumping); too far = it falls short, after first trying the best-aimed on-pitch
  team-mate the head CAN reach. Header pass solve: onto the mate's predicted point at 0.3 m, nodded down (vz -0.5) up
  to 6 m, else vz = 1 + 0.15 d (lofted header: the lob formula); RUN hand-off. Turned >= 135 deg: a flick-on (0.5-
  0.88 of the pace, bent <= 40 deg). Error +-2 deg x (1 + phi/90) x (1 + over), x3 between 90 and 135 deg - on every
  header pass and on a human's header at goal; the AI's header shots keep no extra error until stage 4 (with it AI
  goals fell 0.48 -> 0.41/min). Header passes in practice reach ~10 m off a 13 m/s cross.
- Replaces the old header pass (0.7 d + 3 at his current position, vz 1.8); `T_HEADER_PASS_K/BASE/MIN/MAX/VZ` removed.
- **Bands:** CHEST now runs to 1.6 m (= T_INTENT_HEAD_Z; report: 1.5) - otherwise a 1.5-1.6 m ball faster than
  head-control's 11 m/s with a button pressed was neither headed nor controlled. HEAD band 1.6-2.05.
- test_ai_rules' throw-in check accepts the marker at the 2 m line heading the throw first (it already did in stage 2;
  where it then went was luck).

## D-049 — Chase assist wherever the controlled man is near the ball (2026-11-17, CP0.5b, owner request)
Owner: "whenever the player being actively controlled is near the ball, always have some system assist so that player
moves towards the ball as long as the direction of movement is somewhat in the direction of where the ball is".
**Amends D-037** (the cone itself, its levels and its contract are unchanged).
- **Audit of `seats_chase_assist` (D-037/D-039) - where a stick held at the ball got NO help:** (1) a team-mate
  quicker to it (the "his ball" share weight faded to 0 when a mate was 0.3 s quicker - even with the ball 5 m away);
  (2) a ball he can't reach at the pace he is running (no meeting point in 4 s: a ball rolling away, a jog after a
  long ball); (3) meeting points later than T_CHASE_MAX_T (2/3/3.5 s: high lofts); (4) any pass of ours travelling to
  another team-mate (always raw); (5) aerial balls steered to meet_chase's body-height point instead of the contact
  plan's point (D-047) where his first touch actually happens. Already covered: still / stray / deflected balls,
  their pass in flight (when no mate was quicker), their carrier within 12 m (D-039), his own receive (D-020).
- **Rule now:** "near the ball" = the ball OR his meeting point within `T_CHASE_NEAR_R` = 12 m (the same radius as
  the carrier press; ~1.5-2 s of running). Near, the ball is always his to chase: no share fade, no time limit; an
  unreachable ball -> the cone steers toward where it ends up (meet_chase's last predicted point). Further away the
  D-037 rules hold unchanged.
- **Targets:** aerial ball with a contact plan -> `plan.at` (contact.c); otherwise meet_chase's stick-preferred
  reachable point (still ball = the ball, rolling / deflected = intercept on its path with bounces, their pass =
  intercept, through ball / loft once down at body height); their carrier -> the led press point (unchanged).
- **Our pass to another team-mate:** the receiver is added to the share-weight comparison; the cone helps only if the
  controlled man is quicker to it than him (near or far), so the assist never steals a mate's pass for you.
- **Contract kept:** neutral stick never moves him, the turn is <= F (20/30/40 deg), never beyond E, speed and sprint
  are his, Off = raw. Restarts excluded. Tests: `test_chase_assist` 11-17 (each situation, assisted vs OFF, and the
  contract across all of them). 7v7 golden changed on purpose (the scripted seat plays at Normal); 5v5 (Off) unchanged.

## D-050 — Active-player switching: one ETA oracle, event gates, a visible next man (2026-11-17, CP0.5b, owner request)
Owner: "the switching of active player needs improvement ... frequent switching sometimes when a couple of players
near the ball". Research: `reports/Active player switching.md`. **Amends D-017, D-020, D-029 and D-037** (switching
parts only). Owner decisions (binding): a menu setting "Auto switching: Manual / Events / Assisted", default Events
with the 3x override; Q + a held direction picks the man that way (ball-relative), repeated Q cycles, no right stick
(R3 stays zoom); keep the 1.5 s manual lock.
- **Modes** (`SimState.switch_mode` per seat, `WpMatchConfig.switch_mode`): LEGACY = the pre-D-050 rules bit for bit
  (sim_default_config, every regression test; 5v5 golden unchanged), MANUAL (forced hand-overs + the button only),
  EVENTS (game default), ASSISTED (more eager). Independent of the movement assist level (Off / Low / Normal / High
  keep their D-037 meaning). Menu row saved in settings.cfg (`switching = manual|events|assisted`), applied on
  Restart match like the assist row. New fields hashed only when a seat isn't LEGACY.
- **Oracle** (`switching.c`): per seat, `sw_next` = the eligible team-mate (outfield, no seat) with the smallest
  intercept ETA in ticks: first 50 ms step of meet_choose's cached ball path (<= 1.9 m) he reaches from his position
  0.2 s ahead, react 0.15 s + turn (0.30 s x (1-cos)/2 toward THAT point) + acceleration (0.22 s x (1 - speed along it
  / top)) at sprint speed, squared compares with an optimistic pre-check (a root only for steps that pass it). None
  in 4 s: his time to the path's end (>= 241; no 9 s cliff). Their carrier: a press point led 0.3 s and 1.5 m toward
  our goal centre, +0.5 s if he's the wrong side. GRF hysteresis (t_c + 30)·5 < (t_inc + 30)·4, and the man shown
  stays at least 36 ticks (T_SW_NEXT_HOLD) unless he can no longer be picked. Refreshed every 3 ticks (and on a press).
- **Forced** (fire at once, set the dwell): our pass struck (hand-off), gaining the ball (AI mate / keeper / restart
  taker), a deflection (best ETA if quicker than him), the keeper handing back (best ETA), kick-offs.
- **Unprompted** (Events / Assisted only), all of: dwell over (every switch: 24 / 18 ticks; manual: 90 = the 1.5 s
  lock), phase PLAY, not his ball / ours / in the hands, not mid-kick or header, no held charge, no buffered press
  (D-048), not the pass target, no pass of ours travelling, **their ground pass travelling: wait** (where it ends up
  decides), the candidate isn't the man just left (return lock 60 ticks), and a ratio on (ETA + 30 ticks):
  *defensive* 1/2 (Events; Assisted 7/10) inside a 9-tick event window (the ball went loose by a tackle / block /
  clearance / shot, their lofted pass, they took it off us, our pass ended loose) and while their carrier has it
  (D-029 kept); otherwise the *override* 1/3 (3x quicker; Assisted 1/2). **Stick intent** (held >= 48/127 within 50
  deg of the ball or where it is in 0.5 s): either rule needs 1/4 (Assisted 1/3). The switch always lands on sw_next.
  Deviations from the report: events-only defensive ratio except their carrier (the report held it for their whole
  possession incl. passes in flight - measured: one switch every dwell while their pass travelled); event ratio 1/2
  (report 3/5); Assisted return lock 60 (report 45: 24 ping-pongs in 15 min).
- **Button:** a press goes to exactly the man the indicator shows (`sw_q`, recomputed every tick so it follows the
  stick at once): with a direction held, the team-mate within
  45 deg of it seen from the ball, minimising d·(2 - cos) (nobody that way: the oracle's man - never a dead press);
  no direction: sw_next (or the previous one if it changed < 6 ticks ago). Presses within 40 ticks of the last cycle
  through men not yet visited (bit mask incl. the start; resets when all are visited). No hold-to-aim mode.
- **Indicator** (`render.c`): a dim hollow chevron over sw_q while we don't have the ball and no pass of ours travels;
  a solid one over sw_next when an automatic switch is near (event window, or within 1.25x of the ratio).
- **Post-switch blend:** while the stick is held, for 18 ticks (Assisted 24) the stick's heading is turned toward his
  own run by w = r / 2 x 18 (at most half the angle, magnitude kept; the report's vector blend cancels to a dead stop
  on a reversal). Released stick untouched.
- **Team plan:** the man a seat just left can't be the AI ball-winner while the return lock runs; the seat's man stays
  the winner when engaging (close / moving at it) and within 1.2x + 0.1 s of the quickest AI man by switch_eta (was:
  only when the fastest). No-op for a team without a non-LEGACY seat (AI vs AI byte-identical).
- **Measured** (`tools/playtest/switchrate.c`, 7v7, assist Normal, 3 seeds x 5 min): unprompted switches/min stick
  released 34.2 -> 12.7, stick held at the ball 17.3 -> 0.47; A->B->A within 60 ticks (both unprompted) 19 / 10 -> 0 /
  0. Manual: 0 unprompted. Assisted: 22.7 / 1.9, A->B->A 0. Indicator while drawn: oracle changes ~31-37/min (report's
  starting target 20: every switch moves it), A->B->A within 30 ticks 0. Tests: `tests/test_switching.c`.
- **Known (not fixed):** a passive seat man within 4 m of a still loose ball is the team's ball-winner (D-030 "fastest
  and engaging"), and the team that last had the ball picks no winner for a loose ball - so a human who never moves
  can freeze play beside a still ball (pre-D-050 too; the 1.2x bonus is gated on engaging). The harness's released
  script walks onto such a ball.

## D-051 — AI aerial play through the planner: utility choice, contest, the header law for everyone (2026-11-17, CP0.5a stage 4)
Report row 4 ("AI intents through the planner, driven crosses, AI stance choice"). New `src/sim/aerial_ai.c`.
- **Same contacts as a human:** non-seat players carry `WpPlayer.ai_intent` (WP_KICK_* + 1, 0 = control) and
  `ai_aim`; `intent_of` / `intent_aim` / `intent_clear_player` read them like a seat's buffered press, so the AI's
  volleys / half-volley passes / lay-offs go through `contact_first_time` / `contact_strike` and its headers through
  `header_start` / `header_contact` (sub-kind from the intent). With a SHOT intent the AI's planner looks only at the
  volley bands (LOW / MID). After any aerial touch the AI plays on from its brain (intent cleared). Hashed (with
  `ai_air`, the choice, and D-052's last-touch fields).
- **Choice (D-008):** for the pass's receiver and each team's ball-winner (a bystander only heads a ball that comes
  to him on his decision beat, as before): candidates CONTROL (the plan), HEAD_SHOT, HEAD_PASS per mate 3-14 m,
  HEAD_CLEAR (our half), VOLLEY (earliest reachable LOW/MID contact), HALF-VOLLEY / LAY-OFF (plan band LOW / CHEST)
  x features THREAT, OPEN, FWD, DIST, CONTEST, PRESS, DIFF, DANGER, ERR, BIAS; weights in `aerial_ai.c` like brain.c's;
  ties in fixed order; commit +1, a header once chosen on our own ball is latched (+50).
- **Contest** = an opponent's earliest contact with the ball (ring, planner reach model, any playable height) before
  ours, graded over 4 ticks after / 12 before. A contested foot or control contact is headed away before it happens
  (weight -10), so a contested receiver attacks it with his head; a free one controls / volleys. (Stage 2's 59%: the
  receiver walked to his control point and the marker headed it.)
- **Error is the law:** the AI's header shots now get the D-048 header error like a human's (removed the AI
  exemption). ERR feature = the spread that law (or the volley law) gives at the goal / half the goal; a turn
  >= 135 deg (a flick) can't be aimed at goal (no candidate); the corner with the smaller turn if it cuts ERR by 0.3;
  the aim is pulled inside the post by the spread. The AI's header shots and volleys skip `kick_aim_assist` (it
  re-picked the corner from the aim's y sign and undid the choice: 16 of 94 header shots were still flicks, 1 goal;
  after: 7 of 86). Humans keep the assist. No dice beyond the existing draws.
- **Movement:** attack point = for our ball the reachable ring point in the head band (1.6-2.35 m) nearest to him,
  for theirs the earliest; our ball to him waits where it was played to (pass_meet) and heads it once it is within 3 m
  (the header then steps him under its path and times the jump; jump from the ring's height); their ball: a defender
  commits only 36 ticks before his contact (`T_AIR_DEF_READ`: he reads the flight late), starts the header at the
  point (0.3 m) or 8 ticks before. Control / volley: `contact_stand_at` = 0.5 m beyond the planned point along the
  flight (standing on the point let the ball clip his head on the way down: 37 of 40 receptions deflected). Aerial AI
  touches cushion the way he faces (the AI's stick is its run).
- **Driven crosses (report: z 1.8 m): built, measured, OFF** (`T_CROSS_DRIVEN 0`; `cross_solve`, own table, vz =
  6 + 0.06 d, a first-man lane check). Scripted marked duels (`tools/playtest/duel.c`, 72 marker spots): lofted 2.3 m
  58/72 to the attackers, driven 0/72 - a ball that stays at head height for its whole descent is met by the marker
  0.5 m in front. 80 matches: driven 0.43 goals/min, crosses 47%, header shots/cross 0.43 vs lofted 0.41 / 51% / 0.49.
  The stage-1 2.3 m column stays (D-046) until a cross that clears the near man exists.
- **Measured** (7v7, WP_STATS=1, rules on; `aerial` 80 x 5 min on seeds 1-80 and again on 81-160 (`WP_SEED0=80`);
  baseline = HEAD before stage 4): goals 0.47 / 0.50 -> 0.43 / 0.46 per min (160 matches: 0.48 -> 0.44, -8%, inside
  +-15%); crosses completed 48% / 53% -> 47% / 48% (50.4% -> 47.3%: below the baseline by 3 points, ~1 sigma of the
  difference - NOT the >= the report asked for); header shots per cross 0.39 / 0.41 -> 0.42 / 0.43; aerial turnovers
  0.37 / 0.45 -> 0.36 / 0.45 per min; lobs 71% / 73% -> 67% / 70%; header shots -> goals 75 -> 50 and 84 -> 61 (no AI
  error) -> 86 -> 39 and 84 -> 39; AI first-time foot strikes ~0-4 per 400 min (candidates exist, rarely the best).
  aimatch 20x5: goals 18+26 -> 18+28, passes 74/76% -> 74/76%. Scripted duels: 70/72 -> 58/72 (both sides cost: the
  defender attacking the ball ~-12, the new receiver ~-7). bench_sim 10.57 -> ~10.6-10.9 us/tick. Defender read
  window (80 matches, an earlier code state): 24 / 36 / 48 ticks -> crosses 47 / 47 / 45%, duels lost in matches 16 /
  21 / 38, goals 0.41 / 0.40 / 0.41; 36 kept (shorter mostly switches the defender off - an owner call on how good
  defenders should be). Header passes completed 40% -> 41-43% (now incl. lofted clearance headers: not like for like).

## D-052 — Aerial contact poses: fitted stance families timed on the sim's contact (2026-11-17, CP0.5a stage 5)
Report row 5 (presentation only; the owner chose no lob ghost ring). New `src/game/avatar_stance.c`,
`stance_vis.c`, `pose_check.c`, `avatar_internal.h`.
- **Sim -> pose (read-only):** before a touch, the contact plan (`WpPlan`: stance, tick, point, height) - turned
  into a volley / half-volley / lay-off when the seat's buffered press or the AI's intent will strike it first time;
  after it, `WpPlayer.last_touch_kind` / `last_touch_tick` (D-051, hashed bytes) and the ball where `prev` had it at
  the touch. A header coming: the drag-free point where the ball meets his head. The stance shows from 24 ticks
  before to 27 after; it is drawn exactly at the contact on alpha 0 of the frame whose `cur` holds the touch.
- **Families with free parameters**, fitted every frame (coordinate search over the real skeleton, ~100 solves,
  costs the limb's contact centre - chest front / thigh top / anywhere on the head + a ball radius, or the instep - to
  the ball, plus a small preference for the limb over a lunge): chest / lay-off (lean, knee give, rise <= 0.15 m, a
  step), head control (same, a hop <= 0.35 m), thigh (raised thigh, shin hanging, standing leg gives), instep and
  volleys (hip / knee / splay of the leg on the ball's side, volleys side-on with a backswing and follow-through),
  headers (the existing header pose nudged by lean / rise / a step; a header pass glances toward its target).
  Pose `Pose.shift_f/shift_l/rise` added for the lunges and hops.
- **Timing:** ramps in over 0.30 s to full weight 0.04 s before the contact, holds, eases out by 0.4 s with a cushion
  (chest leans back, thigh / instep withdraw, head-control nods it down) or a follow-through (volleys, lay-off push).
- **Check:** `--check-kick-pose` (ctest `avatar_kick_pose`) also flies a ball at a standing seat player for every
  stance (instep, thigh, chest, head control, volley, half-volley, lay-off, header pass, header shot) at 3 lateral
  offsets and measures the drawn contact centre on the contact tick: worst 0.104 m (volley), all others <= 0.033 m
  (limit 0.15). Goldens unchanged (presentation only).

## D-053 — A human's corner: runners in the box, a slower dead-ball meter, the ball to his head (2026-11-17, owner)
Owner: "the corner kicks have some problem, the nearby player is too near and I am never able to send a long kick for
header in the D-area. Either it is non-intuitive or not-doable right now, I tried with long press of lofted pass
button". Measured (headless, 7v7): on a human's corner no team-mate ever stood in the box (formation spots 13-29 m
out; the plan's near-support walked to ~2 m of the taker); the open-play meter fills 12 -> 45 m in 0.4 s, so the box
(20-32 m from the flag) was a ~0.15 s window and a "long press" sailed 45 m over everyone; the meter's +-3 m depth
bias dropped the ball past / short of the snapped man, where his marker headed it (0/30 reached an attacker vs 6/6
with a tap).
- **Box runners** (`setpiece.c`, a seat's corner only): penalty spot, far post, near post, a short option ~10 m off
  (n - 3 of them: 4 in 7v7, 2 in 5v5); the rest keep the plan. The walk-up waits for them (within 2 m) up to 7 s
  (`T_CORNER_SETUP_MAX`, a seat's corner only). AI takers keep their own runs (fixed spots cost AI lob completion
  72% -> 59%).
- **Skipping a walk-up places everyone** (`restart_setup_step`, any restart): the press that skips it teleports the
  AI players to where they were walking (legal distance / receiving spot). Was: play started with them mid-walk.
- **Dead-ball lob meter** (`lob_ramp_ticks`): the taker of a dead-ball restart (not a kick-off) fills the meter over
  `T_LOB_SET_RAMP` = 60 ticks after the tap window (1.0 s; open play 24 ticks unchanged); render.c's bar matches.
- **To his head:** a lofted ball (human or AI) snapped to a mate within `T_AI_CROSS_ZONE` of goal comes down at
  `T_LOB_HEAD_Z` (was AI only), and the meter's depth bias is off for it (the meter picks the man; the ball goes to
  him).
- **Known, not fixed:** the snapped runner still loses most headers to his marker (headless, buttons pressed late:
  attackers win the first touch in few cases; the AI's own corners win ~30%). A seat receiver doesn't attack a
  contested ball the way aerial_ai.c's AI receiver does (contact_reachable finds no head-height point for him) - a
  follow-up. Tests: `tests/test_corner.c`. Goldens updated on purpose (5v5 `319956738752144e`, 7v7 `9dfc679575aa1e19`).
