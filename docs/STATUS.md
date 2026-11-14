# Status

_Last updated: 2026-11-14 by Claude session #1_

## Current checkpoint
**CP0.0b — Human avatars: ✅ done** (inserted after owner feedback on CP0.0). Waiting for the owner to:
(1) pick an avatar look preset, (2) pick a default camera zoom, (3) OK contextual headers (D-010),
(4) give the go-ahead for **CP0.1 "Ball on grass"**.

## What works now
- Build: CMake + Ninja, SDL 3.4.8 vendored and statically linked.
- Deterministic sim core (`src/sim/`): Q16.16 maths, PCG32, `SimState`/`WpInput`, 5v5 kickoff layout, player
  movement from seat input (WASD/arrows, Shift sprint), basic ball flight/bounce/roll (ball not kickable yet),
  **player jump state for headers** (`z/vz/action/action_timer`, `sim_player_jump`; no gameplay trigger yet).
- Avatars (D-009): procedural skeleton footballers — run cycle, kick, jump-header, keeper stance; seeded
  skin/hair/height/build; kits per team; 3 look presets (P), 3 zoom levels (Z). ~5% of one core at 60 fps.
- Flags: `--demo`, `--gallery`, `--style N`, `--zoom N`, `--screenshot F.bmp [--frames N]`.
- Tests (3, passing): `test_fixed`, `test_sim` (movement, jump apex/airtime/landing, ball bounce/rest),
  `test_determinism` (golden `9c15a05267885e09`); `tools/check_determinism.sh` identical across gcc/clang × -O0/-O2.

## Next step (exact)
1. Record the owner's answers (look preset → `WpRenderCtx.style` default in `main.c`; zoom → `start_zoom`;
   header decision) in DECISIONS/PLAYTEST_FEEDBACK.
2. Start CP0.1 per `docs/ROADMAP.md`: split ball physics into `src/sim/ball.c` (+ spin/curl); possession/dribble
   with **height-band contact** (D-010); turn rate in the sim; shoot (charge) + chip; goal detection with
   posts/crossbar/net; score + kickoff reset; HUD power bar; F3 ball speed/height. Avatar: dribble-touch pose.
3. Tests: kicked-ball ranges, goal detection, overhead ball not controllable; update golden; playtest guide; commit.

## Known issues / environment notes
- **No audio backend:** owner to run `sudo apt install libasound2-dev libpulse-dev libudev-dev`, then
  `rm -rf build && cmake -S . -B build -G Ninja && cmake --build build` (needed by CP0.4/CP0.5).
- ImageMagick not installed; view screenshots by converting BMP→PNG with Python PIL.
- Avatar nits for later: eyes only drawn on large heads; torso is a capsule blend (fine at match scale);
  header-rise pose legs could be tidier; no shirt numbers yet.

## Playtest guide (current build)
- `./build/wildpitch --demo` — 8 players walk/jog/sprint/stand, kick and jump. Press **P** to cycle looks
  (Modern / Arcade / Retro pixel) and **Z** to cycle zoom. Judge: do they look and move like footballers? Which look?
- `./build/wildpitch --gallery` (P/Z also work… press a key to redraw) — every pose up close + at match scale.
- `./build/wildpitch` — move the marked player yourself (WASD/arrows, Shift sprint).
