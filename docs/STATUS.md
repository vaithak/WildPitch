# Status

_Last updated: 2026-11-14 by Claude session #1_

## Current checkpoint
**CP0.1 — "Ball on grass": ✅ done, waiting for the owner's playtest.** Next: **CP0.2 "Pass and move"** (re-plan
after feedback).

## What works now
- Build: CMake + Ninja, SDL 3.4.8 vendored and statically linked.
- **Sim (modular, D-012/D-013):** `sim.c` orchestration · `ball.c` substepped physics (drag, Magnus curl, bounce,
  roll), posts/crossbar, player bodies, net box, interpolated goal-line crossing, outs · `player.c` movement with
  turn rate, parametric kicks (pass / charged shot / charged chip), height-band control, dribbling · `tuning.h`.
  Goals → 2 s celebration → kickoff reset (conceding team kicks off). Temporary out rule: ball placed back inside.
  Per-tick events (kick/touch/bounce/post/net/goal/out/reset/body).
- **Game:** Modern look, wide zoom (defaults), power bar while charging, leg draws back as you charge, kick pose
  driven by kick params (foot/kind/wind-up), GOAL banner, OUT label, no interpolation across resets, net drawn as
  the physics box, F3 shows ball speed/height/spin/owner/phase/events, control hints on screen.
- Flags: `--demo`, `--gallery`, `--autoplay`, `--autoplay-chip`, `--check-kick-pose`, `--style N`, `--zoom N`, `--screenshot F.bmp [--frames N]`.
- **Tests (7 ctest entries, all passing):** test_fixed, test_sim, test_ball, test_ball_control, test_scoring, test_determinism
  (golden `d304ef46f21376c6`; scripted seat chases the ball: ~110 kicks, 270 touches, 6 goals per 10 min, asserted),
  avatar_kick_pose. `tools/check_determinism.sh`: identical across gcc/clang × -O0/-O2.
- Perf: full game loop ~5% of one core; `bench_sim` ~22,700× real time.

### Feel numbers (from test_ball_control; update when tuning changes)
Dribble numbers are the WORST case over 16 start offsets across a touch cycle (no offset may lose the ball):
| | jog | sprint |
|---|---|---|
| max ball gap, straight dribble | 0.83 m | 1.02 m |
| 90° cut: ball going new way after | 4 ticks (67 ms) | 6 ticks (100 ms) |
| 180° turn: ball heading back after / max gap (lose at 2.2 m) | 0 ticks / 0.84 m | 3 ticks / 1.51 m |
| stop: ball settles from the player | 0.41 m | 0.43 m |
| shot release → ball leaves foot | ≤ 6 ticks (100 ms) | ≤ 6 ticks |

Scoring sweep (`test_scoring`, keyboard 8-way, statue keeper): natural positions (x ≥ 10 m, |y| ≤ 4 m, stick toward
goal) score **52%**; misses are mostly the keeper/kickoff statues. Floor asserted at 45%.
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
1. Log the owner's CP0.1 playtest feedback in `docs/PLAYTEST_FEEDBACK.md`; adjust `src/sim/tuning.h` if needed.
2. Re-plan and start CP0.2 per ROADMAP: aimed passing (tap short / hold through ball), switch player, teammates
   holding shape with basic runs, receiving/first touch from passes, **playable contextual headers** (D-010:
   pass/shoot while a reachable ball is in the air → jump/stand header with head-band contact), HUD clock.

## Known issues / environment notes
- **No audio backend:** owner to run `sudo apt install libasound2-dev libpulse-dev libudev-dev`, then
  `rm -rf build && cmake -S . -B build -G Ninja && cmake --build build` (needed by CP0.4/CP0.5).
- Other players are statues (no AI until CP0.3); they block the ball with their bodies, keepers included.
- Headers: height bands exist; the trigger is CP0.2. Avatar dribble-touch flick not animated yet.
- No gamepad mapping yet (CP0.4). Keeper doesn't dive (CP0.3).
- Avatar nits: eyes only on large heads; back number is a generic "11".
- Agents: never `pkill -f` a pattern that also appears in your own shell command (it kills the shell).

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
