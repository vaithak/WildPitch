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
- Flags: `--demo`, `--gallery`, `--autoplay`, `--check-kick-pose`, `--style N`, `--zoom N`, `--screenshot F.bmp [--frames N]`.
- **Tests (6 ctest entries, all passing):** test_fixed, test_sim, test_ball, test_ball_control, test_determinism
  (golden `3b905d752c5d3054`; scripted seat chases the ball: ~110 kicks, 270 touches, 6 goals per 10 min, asserted),
  avatar_kick_pose. `tools/check_determinism.sh`: identical across gcc/clang × -O0/-O2.
- Perf: full game loop ~5% of one core; `bench_sim` ~22,700× real time.

### Feel numbers (from test_ball_control; update when tuning changes)
| | jog | sprint |
|---|---|---|
| max ball gap, straight dribble | 0.83 m | 1.02 m |
| 90° cut: ball going new way after | 4 ticks (67 ms) | 6 ticks (100 ms) |
| 180° turn: max gap | 0.69 m | 1.76 m |
| shot release → ball leaves foot | ≤ 6 ticks (100 ms) | ≤ 6 ticks |

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
Run `./build/wildpitch`. You control the red #11 striker (yellow marker). Everyone else stands still.
- **WASD/arrows** move, **Shift** sprint, **J/Space** pass (ground pass where you're facing),
  **K** hold to charge a shot and release, **I** hold/release to chip. **F3** debug, **P** look, **Z** zoom.
- Try: dribble with quick cuts and stops (jog vs sprint), shoot at different charge levels, curl a shot by
  striking across your run, chip the keeper from the edge of the box, hit the post/bar, put it in the net.
- Judge: Does dribbling feel responsive but not glued? Do shots feel weighty, and is 100 ms from release to
  strike OK? Is aiming at the goal too easy/hard (aim assist)? Do chips/curl feel right? Is the camera OK at wide zoom?
- Known: statues block the ball; going out places the ball back inside after a moment (temporary).
