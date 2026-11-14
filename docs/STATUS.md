# Status

_Last updated: 2026-11-14 by Claude session #1_

## Current checkpoint
**CP0.0 — Foundations: ✅ done.** Waiting on the owner's roadmap feedback before starting **CP0.1 "Ball on grass"**.

## What works now
- Build: CMake + Ninja, SDL 3.4.8 vendored and statically linked (`build/wildpitch` ≈ 15 MB with debug info).
- Deterministic sim core (`src/sim/`): Q16.16 maths, PCG32, `SimState`/`WpInput`, kickoff layout for 5v5,
  player movement from seat 0 input (WASD/arrows, Shift sprint), basic ball flight/bounce/roll (ball isn't
  kickable yet).
- Game window: tilted 2.5D tabletop pitch, goals with nets, players (team colours, keepers distinct),
  ball + shadows, camera easing, F3 debug overlay, `--screenshot` flag.
- Tests (3, all passing): `test_fixed` (maths accuracy), `test_sim` (movement speeds/stop/facing; lobbed ball
  never below ground, decaying bounces, comes to rest), `test_determinism` (golden hash `c5cdc2d8ec209d89`,
  snapshot/resume; script launches the ball every 3 s and drives one player per team so physics is covered).
  `tools/check_determinism.sh`: identical under gcc/clang × -O0/-O2.
- `bench_sim`: ~38,000× real time — but the sim has no AI yet, so this is NOT a training-headroom figure.
- Lesson recorded in CLAUDE.md: gcc/clang evaluate function arguments in different orders; never do two
  RNG draws in one expression (this caused a real cross-compiler hash mismatch, now fixed).

## Next step (exact)
Once the owner OKs the roadmap, start CP0.1 per `docs/ROADMAP.md`:
1. Split ball physics into `src/sim/ball.c`; add spin/curl; tune bounce/roll on grass.
2. Ball control: possession/dribble (ball pushed ahead in small touches), touch cooldowns.
3. Kicking: shoot (hold to charge, release), chip modifier, power/direction from stick + facing.
4. Goal detection (posts/crossbar collisions, net catches ball), score, celebration pause, kickoff reset.
5. Camera framing for attack direction; HUD power bar; F3 overlay with ball speed/height.
6. Tests: ball physics unit tests (a kicked ball lands/rolls within expected ranges), goal detection test,
   golden hash update. Then the playtest guide in this file, commit, and ask the owner to play.

## Known issues / environment notes
- **No audio backend:** `libasound2-dev`, `libpulse-dev` are not installed, so SDL builds without sound.
  `libudev-dev` is also missing (gamepad hotplug falls back to non-udev path). Needed by CP0.4/CP0.5 —
  owner to run `sudo apt install libasound2-dev libpulse-dev libudev-dev`, then `rm -rf build && cmake ...`.
- ImageMagick not installed; use `./build/wildpitch --screenshot x.bmp` + Python PIL to view frames.

## Playtest guide (current build)
Not a game yet. `./build/wildpitch`: move the marked orange player with WASD/arrows, Shift to sprint; F3 shows
tick + sim cost. Judge only: is the camera angle / tilt / look a good base for the game?
