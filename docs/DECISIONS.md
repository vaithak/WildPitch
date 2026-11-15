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

## D-017 — Passing & switching model (2026-11-15, CP0.2a) — *receive/assist parts superseded by D-020*
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

## D-020 — Passes solve the meeting point; the receiver is steered, never abandoned (2026-11-15, CP0.2c)
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

## D-029 — Keeper's hands, throw-ins, corners, goal kicks, defender auto-switch (2026-11-15, owner request)
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
