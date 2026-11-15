# Status

_Last updated: 2026-11-15 by Claude session #1_

## Current checkpoint
**CP0.2c — Passing & reception that feel natural: ✅ done, waiting for the owner's playtest.** Owner said through-ball
power and reception felt off and that pressing a key while receiving made the player run past the ball → research
(`reports/Through ball and pass reception.md`) + headless playtest agent → D-020. Next: **CP0.3 "Instinct AI"**.

## What works now
- **CP0.2c (D-020):** through balls land just ahead of a running receiver (hold = deeper/softer, never weaker than a
  tap), taps lead runners by the real flight time and pace grows with distance, receivers keep running and are never
  abandoned when you press or release keys (the stick picks where he meets it), 1 m swept reach, deflections switch
  you to the best-placed man at once, hold J with nobody ahead = harder ball.
- **CP0.2b:** hold J = through ball (white bar = how far ahead; receiver runs onto it), L = lofted pass over
  defenders, J/K at a ball coming in high = header (stand or timed jump; K at goal, J to a team-mate).
- **CP0.2a:** full 5-a-side team. J = pass to the best team-mate toward the stick (weighted to arrive ~6 m/s), control
  jumps to the receiver when the ball is struck and he runs to meet it; Q = switch; auto-switch to the team-mate
  nearest a loose ball; team-mates hold shape + make runs (placeholder, team.c); opposition = passive shuffling
  "cones"; match clock in the HUD. Receiving at a sprint = heavier first touch.
- Build: CMake + Ninja, SDL 3.4.8 vendored and statically linked.
- **Sim (modular, D-012/D-013):** `sim.c` orchestration · `ball.c` substepped physics (drag, Magnus curl, bounce,
  roll), posts/crossbar, player bodies, net box, interpolated goal-line crossing, outs · `player.c` movement with
  turn rate, parametric kicks (pass / charged shot / charged chip), height-band control, dribbling · `tuning.h`.
  Goals → 2 s celebration → kickoff reset (conceding team kicks off). Temporary out rule: ball placed back inside.
  Per-tick events (kick/touch/bounce/post/net/goal/out/reset/body).
- **Game:** Modern look, wide zoom (defaults), power bar while charging, leg draws back as you charge, kick pose
  driven by kick params (foot/kind/wind-up), GOAL banner, OUT label, no interpolation across resets, net drawn as
  the physics box, F3 shows ball speed/height/spin/owner/phase/events, control hints on screen.
- **Dedication card on launch (D-018)**: "In the memory of my brother Shubham Thakkar" + red heart, ~4 s, any key
  skips (not shown in --screenshot/--autoplay/--demo/--gallery; `--dedication --screenshot F.bmp` captures it).
- Flags: `--autoplay-header H` (scripted cross arriving at height H, K pressed: header visual check), `--dedication`, `--demo`, `--gallery`, `--autoplay`, `--autoplay-chip`, `--autoplay-dribble`, `--check-kick-pose`, `--style N`, `--zoom N`, `--screenshot F.bmp [--frames N]`.
- **Tests (9 ctest entries, all passing):** test_fixed, test_sim, test_ball, test_ball_control, test_passing, test_reception, test_scoring, test_determinism
  (golden `ef60beafb1e050e8`; scripted seat chases the ball + taps/holds pass, lobs, switches, heads: ~140 kicks, 360 touches, 10 goals, 750 switches, 5 headers, owner invariant per 10 min, asserted),
  avatar_kick_pose. `tools/check_determinism.sh`: identical across gcc/clang × -O0/-O2.
- Perf: full game loop ~5% of one core; `bench_sim` ~4,900× real time (3.4 µs/tick; receiver ball prediction each tick; was 22,700× before team logic + pass solver — fine for play, revisit with a
  lookup table before RL training).

### Feel numbers (from test_ball_control; update when tuning changes)
Dribble numbers (D-014). Direction-change rows are the WORST case over 16 start offsets and over 40–180-tick run-ups:
| | jog | sprint |
|---|---|---|
| touch rhythm (steady) | every 16 ticks (0.27 s) | every 20 ticks (0.33 s) |
| ball gap, steady / max | ~0.45 / 0.56 m | ~0.75 / 0.91 m (reach 1.0) |
| player speed change per tick at top speed | 0.00 m/s | 0.00 m/s (was 0.30 flicker) |
| 90° cut: ball going the new way after | 3 ticks (50 ms) | 3 ticks |
| 180° turn: ball heading back after / max gap | 3 ticks / 0.63 m | 3 ticks / 1.00 m (lose at 2.2) |
| stop (stick released): ball trapped at | 0.40 m | 0.40 m (steps up if out of reach; never through the body) |
| balls lost after long run-ups + cuts | 0 | 0 (was 2) |
| shot release → ball leaves foot | ≤ 6 ticks (100 ms) | ≤ 6 ticks |

Ground roll (D-015, `test_ball`): 3 m/s → 2.8 m / 2.2 s · 6 m/s → 7.6 m / 3.3 s · **12 m/s pass → 18 m / 4.5 s** ·
22 m/s → 34 m / 5.5 s (was 55 m / 9 s for a pass).

Bounce (D-016): 2 m drop → 0.69 m; full chip stops after ~30.6 m (was 39.7). Possession "no magnet" checks: ball
never closer than 0.40 m to the player's centre on stops/receptions/gathers; no braking from beyond reach.

Scoring sweep (`test_scoring`, keyboard 8-way, statue keeper): natural positions (x ≥ 10 m, |y| ≤ 4 m, stick toward
goal) score **51%** vs statues; in the **live** setup (keeper shades on his line, opposition shuffles) **39%**
(asserted ≥ 25%). Live passes to moving team-mates: 23/24 complete; misses are mostly the keeper/kickoff statues. Floor asserted at 45%.
Frame pacing: 61.9 fps on a 60 Hz display (vsync on). 120/144 Hz not tested here.

| kick | speed | carry to 1st bounce | peak | flight |
|---|---|---|---|---|
| shot 0% | 14 m/s | 4.6 m | 0.07 m | 0.35 s |
| shot 50% | 22 m/s | 7.9 m | 0.18 m | 0.38 s |
| shot 100% | 30 m/s | 20.7 m | 0.81 m | 0.80 s |
| chip 0% | 8 m/s | 7.7 m | 1.25 m | 1.00 s |
| chip 50% | 11.5 m/s | 13.7 m | 2.15 m | 1.32 s |
| chip 100% | 15 m/s | 20.9 m | 3.28 m | 1.62 s |

## Next step (exact)
1. Log the owner's CP0.2c playtest feedback in `docs/PLAYTEST_FEEDBACK.md`; tune the D-020 block in `tuning.h`.
   Open owner questions: hold = deeper (current) or firmer? Separate through-ball key instead of tap/hold?
2. Re-plan and start CP0.3 "Instinct AI" per ROADMAP (candidate actions × utility, D-008): replaces team.c
   placeholder (incl. one-twos / follow-up runs / run timing / pass lanes from D-020's deferred list); real
   opposition; keepers dive; AI aerial duels; simple player stats. Consider a perf pass (bench 4,900×) first.

## Known issues / environment notes
- **No audio backend:** owner to run `sudo apt install libasound2-dev libpulse-dev libudev-dev`, then
  `rm -rf build && cmake -S . -B build -G Ninja && cmake --build build` (needed by CP0.4/CP0.5).
- Opposition = passive shuffling cones (no tackling/stealing, keepers don't dive) until CP0.3. Team-mates use
  placeholder positioning (team.c), not real AI.
- Headers: height bands exist; the trigger is CP0.2.
- No gamepad mapping yet (CP0.4). Keeper doesn't dive (CP0.3).
- Avatar nits: eyes only on large heads; back number is a generic "11".
- Agents: never `pkill -f` a pattern that also appears in your own shell command (it kills the shell).

## Playtest guide (CP0.2c — focus: does passing/receiving feel natural now?)
Same controls. Try: through balls (hold J) to a running striker — he should keep running and meet it just ahead,
in stride; longer hold = deeper and softer (the bar = how far ahead). Taps to a running team-mate lead him. While a
pass travels, press or release keys freely: he still gets it, and the direction you hold picks where he meets it
(toward = attack it, along = take it on the run); only holding straight back for 1/4 s gives you full manual. A tap
is now anything up to 200 ms. Deflected passes switch you to the best-placed man and he chases the ball.
Questions: through ball depth/pace right? Should the bar mean "deeper" (now) or "firmer"? Separate through key?

## Playtest guide (CP0.2b — focus: through balls, lofts, headers)
New on top of CP0.2a: **hold J** = through ball — the white bar fills = how far ahead of your team-mate it goes;
control jumps to him and he runs onto it (move the stick to take over). **L** = lofted pass to the team-mate toward
your stick, over anyone in between. **Headers:** when a ball comes in high (try L to a team-mate near goal, then
press K as it drops to him), press **K** to head at goal or **J** to head it to a team-mate — he steps under it
and jumps if it's high; the stick aims. Note: a J *tap* now fires when you release it (~1/20 s later than before).
Judge: is the tap/hold split natural, or would you rather have a separate through-ball key? Lob height/pace OK?
Headers: easy enough to time? Too strong/weak?

## Playtest guide (CP0.2a — focus: passing & switching)
`./build/wildpitch`. You're red, attacking right. New: **J** passes to the team-mate most in line with your stick
(nearer + straighter wins); the camera/marker moves to him as the ball is struck and he runs to meet it — move the
stick to take over. **Q** switches to the team-mate nearest the ball. When the ball is loose, control auto-switches
to whoever's nearest. Team-mates spread out and the striker runs beyond the ball; blue are passive cones.
Judge: do passes go to who you expected? Pace right (not too slow/fast to receive)? Is auto-switching helpful or
jumpy? Is the hand-off moment clear? Does the receiver's run feel natural?

## Playtest guide (CP0.1d — focus: does the ball look physical now?)
`./build/wildpitch` (try `Z` for the close zoom). Watch: the ball visibly rolls (panels turn, blur when fast, streak
on hard shots; shadow slides out when it's in the air). Let go of the stick while dribbling/sprinting: you step up
and trap it under the sole — it should never pass through you or get "reeled in". Pass back/sideways while
sprinting: the foot meets the ball where it is (awkward ones are a bit weaker). Shots/chips: lively first bounce
(~0.7 m from 2 m), low drives skip on, chips sit down sooner. Anything still weird → describe the moment.

## Playtest guide (CP0.1c — focus: ball roll)
Pass (J) on open grass and watch it slow: quick at first, creeping to a stop (~18 m). Loose touches, rebounds and
deflections also die sooner now. Too short/long? One number each in `src/sim/tuning.h` (`T_ROLL_DECEL`, `T_ROLL_LIN`).

## Playtest guide (CP0.1b — focus: dribbling)
Same build/controls as below. Dribble at jog and with Shift held: straight runs, sharp cuts, reversals, stops.
Judge: does the ball now feel carried (regular touches, flick visible), at a comfortable distance? Sprinting: ball a
bit further ahead but always yours? If still off, say which: ball too close / too far / touches too frequent / too
rare / turns too sharp / too sluggish — each is one number in `src/sim/tuning.h`.

## Playtest guide (CP0.1)
Run `./build/wildpitch`. You control the red #11 striker (yellow marker), attacking the goal on the RIGHT.
Everyone else stands still (no AI until CP0.3): statues block the ball but you can walk through them.
- **WASD/arrows** move, **Shift** sprint, **J/Space** pass (ground pass toward the stick, or facing if none),
  **K** hold to charge a shot, release to strike; **I** same for a chip. **F3** debug, **P** look, **Z** zoom.
- **How to score:** get inside ~10 m, avoid shooting straight at the keeper: hold **up or down** with right while
  releasing to pick that side of the goal, or just shoot straight and it goes to the far post from where you stand.
  Aim assist is on (target-based, D-013) and tunable. Release → strike takes 100 ms.
- Try: quick cuts/stops at jog vs sprint, shots at 50% vs 100%, curl (strike across your run), chip the keeper from
  10–15 m (full-power chips from close go over), hit the post/bar, watch it hit the net.
- Judge: dribbling responsive but not glued? Shots weighty? 100 ms OK? Aiming too easy/hard? Chips/curl right?
  Camera OK at wide zoom? Anything that felt "wrong" — describe it and I'll turn it into a number to tune.
- Known/temporary: going out places the ball back inside after 0.75 s; no headers yet (CP0.2); no gamepad yet;
  kickoff has blue statues right in front of you; if your monitor is >60 Hz, the exit line prints the real fps.
