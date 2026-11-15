# Status

_Last updated: 2026-11-14 by Claude session #1_

## Current checkpoint
**CP0.0b — Human avatars: ✅ done.** Owner chose Modern look, wide zoom, contextual headers (D-011, defaults set).
Owner asked architecture questions (pitch sizes, flair-move training) → answered; D-012 (parametric moves).
**CP0.1 "Ball on grass": 🚧 in progress** (owner said go). Sim slice done + committed (ball/goals/dribble/kicks);
remaining: game side (HUD power bar, F3 ball info, avatar kick pose from kick params, reset interpolation, demo via
`sim_player_kick`), playtest guide, final commit.

## What works now
- Build: CMake + Ninja, SDL 3.4.8 vendored and statically linked.
- Deterministic sim core (`src/sim/`): Q16.16 maths, PCG32, `SimState`/`WpInput`, 5v5 kickoff layout, player
  movement from seat input (WASD/arrows, Shift sprint), basic ball flight/bounce/roll (ball not kickable yet),
  **player jump state for headers** (`z/vz/action/action_timer`, `sim_player_jump`; no gameplay trigger yet).
- Avatars (D-009): procedural skeleton footballers — run cycle, kick, jump-header, keeper stance; action poses
  blend in/out (no popping; verified with frame strips); seeded skin/hair/height/build; kits per team; back
  number + front crest so facing reads at match scale; 3 look presets (P), 3 zoom levels (Z). ~5% of one core.
- Visual-check recipe for agents: `--screenshot F.bmp --frames N` prints `seat0_screen x y` → crop that player
  across consecutive N to make a motion strip (PIL). Gallery renders Retro through the low-res target too.
- Flags: `--demo`, `--gallery`, `--style N`, `--zoom N`, `--screenshot F.bmp [--frames N]`.
- Tests (3, passing): `test_fixed`, `test_sim` (movement, jump apex/airtime/landing, ball bounce/rest),
  `test_determinism` (golden `632e624d8c1b6fc0`; script now includes jumps); `tools/check_determinism.sh` identical across gcc/clang × -O0/-O2.

## Next step (exact)
1. (done) owner's look/zoom/header answers recorded.
2. Start CP0.1 (kicks as parametric actions per D-012) per `docs/ROADMAP.md`: split ball physics into `src/sim/ball.c` (+ spin/curl); possession/dribble
   with **height-band contact** (D-010); turn rate in the sim; shoot (charge) + chip; goal detection with
   posts/crossbar/net; score + kickoff reset; HUD power bar; F3 ball speed/height. Avatar: dribble-touch pose.
3. Tests: kicked-ball ranges, goal detection, overhead ball not controllable; update golden; playtest guide; commit.

## Known issues / environment notes
- **No audio backend:** owner to run `sudo apt install libasound2-dev libpulse-dev libudev-dev`, then
  `rm -rf build && cmake -S . -B build -G Ninja && cmake --build build` (needed by CP0.4/CP0.5).
- ImageMagick not installed; view screenshots by converting BMP→PNG with Python PIL.
- Avatar nits for later: eyes only drawn on large heads; torso is a capsule blend (fine at match scale);
  header-rise pose legs could be tidier; back number is a generic "11" (real numbers later).

## Playtest guide (current build)
- `./build/wildpitch --demo` — 8 players walk/jog/sprint/stand, kick and jump. Press **P** to cycle looks
  (Modern / Arcade / Retro pixel) and **Z** to cycle zoom. Judge: do they look and move like footballers? Which look?
- `./build/wildpitch --gallery --style 0|1|2` — every pose up close + at match scale (static sheet).
- Jumping is visible only in `--demo` for now; you can't trigger it yourself until headers land in CP0.2.
- `./build/wildpitch` — move the marked player yourself (WASD/arrows, Shift sprint).
